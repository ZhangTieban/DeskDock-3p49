#include "event_log.h"
#include "event_log_policy.h"
#include "ota_config.h"
#include "desk_font.h"
#include "sd_access.h"
#include <Arduino.h>
#include <WiFi.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_core_dump.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <freertos/queue.h>
#include <freertos/idf_additions.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <atomic>
#include <algorithm>
#include <cstring>

namespace {
struct Event {
  uint64_t uptime;
  int64_t epoch;
  uint32_t seq, heap, largest, minimum, psram, buffer;
  int32_t a,b,rssi;
  uint8_t level,station,state;
  char code[28],detail[112];
};
QueueHandle_t queue=nullptr;
std::atomic<uint32_t> sequence{0},dropped{0},failedAllocs{0},failedSize{0};
std::atomic<uint32_t> droppedTotal{0};
std::atomic<uint32_t> buffered{0};
std::atomic<bool> audioLow{false};
std::atomic<int> station{0},audioState{0},rssi{0};
std::atomic<uint32_t> committed{0},storageFailures{0};
std::atomic<int> storageErrno{0};
std::atomic<bool> writing{false};
std::atomic<bool> readLast{false};
std::atomic<TaskHandle_t> asyncHttpOwner{nullptr};
uint32_t bootId;
char filePath[100]={};
char readPath[100]={};
portMUX_TYPE pathMux=portMUX_INITIALIZER_UNLOCKED;
int fd=-1;
vprintf_like_t originalPrintf=nullptr;

Event makeEvent(int level,const char *code,int32_t a,int32_t b,const char *detail) {
  Event e{};
  e.uptime=esp_timer_get_time()/1000;
  timeval tv{};gettimeofday(&tv,nullptr);e.epoch=(int64_t)tv.tv_sec*1000+tv.tv_usec/1000;
  e.seq=sequence.fetch_add(1)+1;e.level=level;e.a=a;e.b=b;
  e.heap=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  e.largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  e.minimum=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  e.psram=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  e.station=station.load();e.state=audioState.load();e.buffer=buffered.load();e.rssi=rssi.load();
  snprintf(e.code,sizeof(e.code),"%s",code ? code : "UNKNOWN");
  snprintf(e.detail,sizeof(e.detail),"%s",detail ? detail : "");
  deskLogTrimUtf8(e.detail);
  return e;
}
void allocFailed(size_t bytes,uint32_t,const char *) {
  // Allocator callbacks must neither allocate nor inspect the heap recursively.
  failedSize.store(bytes>UINT32_MAX ? UINT32_MAX : bytes);
  failedAllocs.fetch_add(1);
}
int sdkPrintf(const char *format,va_list args) {
  if(xPortInIsrContext())return originalPrintf ? originalPrintf(format,args) : 0;
  char text[112];va_list copy;va_copy(copy,args);
  vsnprintf(text,sizeof(text),format,copy);va_end(copy);deskLogTrimUtf8(text);
  // Pinned IDF transport logs EAGAIN as E even during supported asynchronous
  // HTTPS polling. It is socket-not-ready, not process/memory exhaustion.
  // Suppress only that exact normal wait from the explicitly registered task.
  // Its final HTTP error/timeout is still recorded with status, errno and time.
  if(asyncHttpOwner.load()==xTaskGetCurrentTaskHandle() &&
     strstr(text,"transport_base: esp_tls_conn_read error") &&
     (strstr(text,"errno=No more processes") || strstr(text,"errno=Resource temporarily unavailable")))
    return strlen(text);
  const int result=originalPrintf ? originalPrintf(format,args) : 0;
  if (strstr(text,"E (") || strstr(text,"W (")) {
    const bool error=strstr(text,"E (")!=nullptr;
    if (strstr(text,"://") || strstr(text,"password") || strstr(text,"Authorization"))
      strcpy(text,"SDK message contains URL/credentials; inspect serial output");
    desk_log_event(error ? DESK_LOG_ERROR : DESK_LOG_WARN,"SDK_LOG",0,0,text);
  }
  return result;
}
bool openLog() {
  if (!deskFontSdMounted()) return false;
  if (xSemaphoreTake(deskSdMutex(),pdMS_TO_TICKS(50))!=pdTRUE) return false;
  if (!SD_MMC.exists("/logs") && !SD_MMC.mkdir("/logs")) {
    xSemaphoreGive(deskSdMutex());return false;
  }
  time_t now=time(nullptr);tm t{};localtime_r(&now,&t);
  char date[24]="clock-unset";
  if (t.tm_year>=124) strftime(date,sizeof(date),"%Y%m%d-%H%M%S",&t);
  for (unsigned n=0;n<8 && fd<0;++n) {
    char path[100];snprintf(path,sizeof(path),"/sdcard/logs/events-%s-%08lx-%08lx.jsonl",
             date,(unsigned long)bootId,(unsigned long)esp_random());
    fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0666);
    if(fd>=0){portENTER_CRITICAL(&pathMux);memcpy(filePath,path,sizeof(filePath));portEXIT_CRITICAL(&pathMux);}
    if (fd<0 && errno!=EEXIST) break;
  }
  xSemaphoreGive(deskSdMutex());
  if (fd>=0) Serial.printf("[EVENTLOG] ready %s\n",filePath+7);
  return fd>=0;
}
bool persist(const Event &e) {
  char escaped[680],code[64],stamp[40]="";
  deskLogEscape(escaped,sizeof(escaped),e.detail);deskLogEscape(code,sizeof(code),e.code);
  const bool valid=e.epoch>=1704067200000LL;
  if (valid) {
    time_t seconds=e.epoch/1000;tm t{};localtime_r(&seconds,&t);
    strftime(stamp,sizeof(stamp),"%Y-%m-%dT%H:%M:%S",&t);
    const size_t n=strlen(stamp);snprintf(stamp+n,sizeof(stamp)-n,".%03d+08:00",(int)(e.epoch%1000));
  }
  char line[1152];
  const int count=snprintf(line,sizeof(line),
    "{\"schema\":1,\"firmware\":\"" DESKDOCK_VERSION "\",\"boot\":\"%08lx\",\"seq\":%lu,\"time\":%s%s%s,\"uptime_ms\":%llu,"
    "\"level\":\"%s\",\"event\":\"%s\",\"a\":%ld,\"b\":%ld,\"detail\":\"%s\","
    "\"heap\":%lu,\"largest\":%lu,\"min_heap\":%lu,\"psram\":%lu,\"rssi\":%ld,"
    "\"station\":%u,\"audio_state\":%u,\"buffer\":%lu}\n",
    (unsigned long)bootId,(unsigned long)e.seq,valid ? "\"" : "",valid ? stamp : "null",valid ? "\"" : "",
    (unsigned long long)e.uptime,e.level==DESK_LOG_ERROR ? "ERROR" : e.level==DESK_LOG_WARN ? "WARN" : "INFO",
    code,(long)e.a,(long)e.b,escaped,(unsigned long)e.heap,(unsigned long)e.largest,
    (unsigned long)e.minimum,(unsigned long)e.psram,(long)e.rssi,e.station,e.state,(unsigned long)e.buffer);
  if (count<=0 || (size_t)count>=sizeof(line)) return false;
  if (xSemaphoreTake(deskSdMutex(),pdMS_TO_TICKS(50))!=pdTRUE) return false;
  bool okay=true;size_t done=0;
  while (done<(size_t)count) {
    const ssize_t written=write(fd,line+done,count-done);
    if (written<=0) {okay=false;break;}
    done+=written;
  }
  if (okay) okay=fsync(fd)==0;
  if (!okay) {storageErrno.store(errno);close(fd);fd=-1;}
  else if (lseek(fd,0,SEEK_CUR)>=8*1024*1024) {close(fd);fd=-1;}
  xSemaphoreGive(deskSdMutex());
  if(!okay)desk_log_event(DESK_LOG_ERROR,"SD_WRITE_FAILED",storageErrno.load(),e.seq,"pending event retained; retry in a new file");
  return okay;
}
void dumpLast() {
  char text[1537]={};
  char target[100];portENTER_CRITICAL(&pathMux);
  memcpy(target,readPath[0] ? readPath : filePath,sizeof(target));readPath[0]=0;
  portEXIT_CRITICAL(&pathMux);
  if (xSemaphoreTake(deskSdMutex(),pdMS_TO_TICKS(50))!=pdTRUE) return;
  const int input=open(target,O_RDONLY);
  if(input>=0){const off_t size=lseek(input,0,SEEK_END);lseek(input,size>1536 ? size-1536 : 0,SEEK_SET);
    const ssize_t n=read(input,text,1536);if(n>0)text[n]=0;close(input);}
  xSemaphoreGive(deskSdMutex());
  Serial.println("[EVENTLOG] LAST BEGIN");
  // Native USB may accept only part of a long write. Check progress and pace
  // small packets; no SD mutex is held while waiting for the USB host.
  const uint32_t started=millis();size_t sent=0,total=strlen(text);
  while(sent<total && millis()-started<2000){
    const size_t n=std::min((size_t)32,total-sent);
    sent+=Serial.write((const uint8_t *)text+sent,n);
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  Serial.println("[EVENTLOG] LAST END");
}
void preserveCoredump() {
  size_t address=0,size=0;
  if(esp_core_dump_image_get(&address,&size)!=ESP_OK || esp_core_dump_image_check()!=ESP_OK)return;
  const esp_partition_t *part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_COREDUMP,nullptr);
  if(!part || address<part->address || address+size>part->address+part->size)return;
  char path[80];snprintf(path,sizeof(path),"/sdcard/logs/coredump-%08lx.bin",(unsigned long)bootId);
  if(xSemaphoreTake(deskSdMutex(),pdMS_TO_TICKS(50))!=pdTRUE)return;
  const int output=open(path,O_WRONLY|O_CREAT|O_EXCL,0666);xSemaphoreGive(deskSdMutex());
  if(output<0)return;
  bool okay=true;uint8_t block[512];
  for(size_t pos=0;pos<size && okay;){
    const size_t n=std::min(sizeof(block),size-pos);
    if(esp_partition_read(part,address-part->address+pos,block,n)!=ESP_OK){okay=false;break;}
    if(xSemaphoreTake(deskSdMutex(),pdMS_TO_TICKS(50))!=pdTRUE){vTaskDelay(1);continue;}
    okay=write(output,block,n)==(ssize_t)n;xSemaphoreGive(deskSdMutex());pos+=n;vTaskDelay(1);
  }
  xSemaphoreTake(deskSdMutex(),portMAX_DELAY);
  if(okay)okay=fsync(output)==0;close(output);xSemaphoreGive(deskSdMutex());
  desk_log_event(okay ? DESK_LOG_INFO : DESK_LOG_ERROR,okay ? "COREDUMP_SAVED" : "COREDUMP_COPY_FAILED",
    size,0,"retained Flash coredump; may predate this boot; Flash is not erased");
}
void worker(void *) {
  uint32_t lastHealth=0,lastSnapshot=0,lastRetry=0,lastNotice=0;
  bool memoryLow=false,signalLow=false,wasConnected=false,dumpChecked=false;
  for (;;) {
    const uint32_t now=millis();
    if (now-lastHealth>=1000) {
      lastHealth=now;const bool connected=WiFi.status()==WL_CONNECTED;
      rssi.store(connected ? WiFi.RSSI() : 0);
      if (connected!=wasConnected) {desk_log_event(connected ? DESK_LOG_INFO : DESK_LOG_WARN,
        connected ? "WIFI_CONNECTED" : "WIFI_DISCONNECTED",0,0,"");wasConnected=connected;}
      const uint32_t failures=failedAllocs.exchange(0);
      if (failures) desk_log_event(DESK_LOG_ERROR,"ALLOC_FAILED",failedSize.load(),failures,"allocator callback");
      const uint32_t heap=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
      const uint32_t largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
      const bool low=memoryLow ? heap<32768 || largest<20480 : heap<24576 || largest<16384;
      if(low!=memoryLow){memoryLow=low;desk_log_event(low ? DESK_LOG_WARN : DESK_LOG_INFO,
        low ? "MEMORY_LOW" : "MEMORY_RECOVERED",heap,largest,"");}
      const bool weak=connected && (signalLow ? rssi.load()<-70 : rssi.load()<-75);
      if(weak!=signalLow){signalLow=weak;desk_log_event(weak ? DESK_LOG_WARN : DESK_LOG_INFO,
        weak ? "WIFI_WEAK" : "WIFI_SIGNAL_RECOVERED",rssi.load(),0,"");}
    }
    if(now-lastSnapshot>=60000){lastSnapshot=now;desk_log_event(DESK_LOG_INFO,"HEALTH",uxTaskGetStackHighWaterMark(nullptr),storageFailures.load(),"logger_stack / storage_failures; 60 second snapshot");}
    if(fd<0 && now-lastRetry>=5000){lastRetry=now;if(!openLog()){
      if(!lastNotice || now-lastNotice>=30000){lastNotice=now;
        desk_log_event(DESK_LOG_ERROR,"SD_UNAVAILABLE",storageErrno.load(),0,"events queued; old files retained");
        Serial.println("[EVENTLOG] SD unavailable; events remain queued, overflow counted");}
    } else if(storageFailures.load())desk_log_event(DESK_LOG_WARN,"SD_RECOVERED",storageErrno.load(),storageFailures.load(),"previous write failed; queued events retained");}
    if(fd>=0) {
      if(!dumpChecked){dumpChecked=true;preserveCoredump();}
      writing.store(true);
      const uint32_t lost=dropped.exchange(0);
      if(lost && !persist(makeEvent(DESK_LOG_ERROR,"LOG_DROPPED",lost,droppedTotal.load(),"queue overflow / pre-init / ISR")))dropped.fetch_add(lost);
      Event e;
      if(fd>=0 && xQueuePeek(queue,&e,0)==pdTRUE) {
        if(persist(e)){xQueueReceive(queue,&e,0);committed.fetch_add(1);}
        else {if(fd<0)storageFailures.fetch_add(1);vTaskDelay(pdMS_TO_TICKS(100));}
      }
      if(readLast.exchange(false))dumpLast();
      writing.store(false);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}
extern "C" void desk_log_event(int level,const char *code,int32_t a,int32_t b,const char *detail) {
  if(!queue || xPortInIsrContext()){dropped.fetch_add(1);droppedTotal.fetch_add(1);return;}
  Event e=makeEvent(level,code,a,b,detail);
  if(xQueueSend(queue,&e,0)!=pdTRUE){dropped.fetch_add(1);droppedTotal.fetch_add(1);}
}
extern "C" bool desk_log_begin() {
  if(queue)return true;bootId=esp_random();
  queue=xQueueCreateWithCaps(128,sizeof(Event),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!queue){Serial.println("[EVENTLOG] queue allocation failed");return false;}
  if(xTaskCreatePinnedToCore(worker,"SD events",6144,nullptr,1,nullptr,0)!=pdPASS){
    vQueueDeleteWithCaps(queue);queue=nullptr;Serial.println("[EVENTLOG] task creation failed");return false;}
  heap_caps_register_failed_alloc_callback(allocFailed);
  originalPrintf=esp_log_set_vprintf(sdkPrintf);
  const esp_reset_reason_t reason=esp_reset_reason();
  const bool abnormal=reason==ESP_RST_PANIC || reason==ESP_RST_INT_WDT ||
    reason==ESP_RST_TASK_WDT || reason==ESP_RST_WDT || reason==ESP_RST_BROWNOUT;
  const char *name=reason==ESP_RST_PANIC ? "panic" : reason==ESP_RST_TASK_WDT ? "task watchdog" :
    reason==ESP_RST_INT_WDT ? "interrupt watchdog" : reason==ESP_RST_WDT ? "watchdog" :
    reason==ESP_RST_BROWNOUT ? "brownout" : reason==ESP_RST_SW ? "software restart" :
    reason==ESP_RST_POWERON ? "power on" : "other reset; see reason code";
  desk_log_event(abnormal ? DESK_LOG_ERROR : DESK_LOG_INFO,"BOOT",reason,0,name);
  return true;
}
extern "C" void desk_log_audio(int s,int state,uint32_t fill){
  station.store(s);audioState.store(state);buffered.store(fill);
  if(state!=2){audioLow.store(false);return;}
  const bool old=audioLow.load(),low=old ? fill<65536 : fill<32768;
  if(old!=low){audioLow.store(low);desk_log_event(low ? DESK_LOG_WARN : DESK_LOG_INFO,
    low ? "AUDIO_BUFFER_LOW" : "AUDIO_BUFFER_RECOVERED",fill,0,"");}
}
extern "C" uint32_t desk_log_buffered(){return buffered.load();}
extern "C" void desk_log_async_http(bool active){
  const TaskHandle_t self=xTaskGetCurrentTaskHandle();
  if(active)asyncHttpOwner.store(self);
  else {TaskHandle_t expected=self;asyncHttpOwner.compare_exchange_strong(expected,nullptr);}
}
extern "C" void desk_log_ui(uint32_t handler,uint32_t render){
  if(handler>=100000)desk_log_event(DESK_LOG_WARN,"UI_SLOW",handler,render,"handler_us / render_us; timestamp is handler completion");
}
extern "C" bool desk_log_flush(uint32_t timeout){
  const uint32_t start=millis();
  while(queue && (uxQueueMessagesWaiting(queue) || dropped.load() || writing.load()) && millis()-start<timeout)vTaskDelay(pdMS_TO_TICKS(10));
  return queue && uxQueueMessagesWaiting(queue)==0 && !dropped.load() && !writing.load();
}
extern "C" void desk_log_serial_command(const char *command){
  char path[100];portENTER_CRITICAL(&pathMux);memcpy(path,filePath,sizeof(path));portEXIT_CRITICAL(&pathMux);
  if(!strcmp(command,"LOG STATUS"))Serial.printf("[EVENTLOG] path=%s queued=%u committed=%lu dropped=%lu io_failures=%lu\n",
    path,queue ? (unsigned)uxQueueMessagesWaiting(queue) : 0,(unsigned long)committed.load(),(unsigned long)droppedTotal.load(),(unsigned long)storageFailures.load());
  else if(!strcmp(command,"LOG TEST"))desk_log_event(DESK_LOG_INFO,"SELF_TEST",123,456,"USB self-test: quote=\" slash=\\ newline=\n 日本語; not a real fault");
  else if(!strcmp(command,"LOG LAST"))readLast.store(true);
  else if(!strncmp(command,"LOG LAST events-",16)){
    const char *name=command+9;const size_t n=strlen(name);
    if(n>=7 && n<75 && !strstr(name,"..") && !strchr(name,'/') && !strchr(name,'\\') &&
       !strcmp(name+n-6,".jsonl")){
      portENTER_CRITICAL(&pathMux);snprintf(readPath,sizeof(readPath),"/sdcard/logs/%s",name);portEXIT_CRITICAL(&pathMux);
      readLast.store(true);
    }
  }
}
