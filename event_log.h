#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { DESK_LOG_INFO=0, DESK_LOG_WARN=1, DESK_LOG_ERROR=2 };
bool desk_log_begin(void);
void desk_log_event(int level, const char *event, int32_t a, int32_t b, const char *detail);
void desk_log_text(int level,const char *event,int32_t id,const char *text);
void desk_log_mp3(int state,int track,uint32_t buffered,int progress);
void desk_log_mp3_count(int count);
void desk_log_audio(int station, int state, uint32_t buffered);
uint32_t desk_log_buffered(void);
void desk_log_ui(uint32_t handler_us, uint32_t render_us);
void desk_log_async_http(bool active);
bool desk_log_flush(uint32_t timeout_ms);
void desk_log_serial_command(const char *command);
#ifdef __cplusplus
}
#endif
