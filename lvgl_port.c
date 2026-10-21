#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lvgl_port.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "user_config.h"
#include "driver/spi_master.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "src/axs15231b/esp_lcd_axs15231b.h"
#include "i2c_bsp.h"
#include "display_rotate.h"

extern void desk_touch_activity(void);
extern void desk_touch_report(int x, int y);
extern void desk_display_report(uint32_t rotate_us, uint32_t transfer_us);
extern void desk_ui_report(uint32_t handler_us);

static const char *TAG = "lvgl_port";
static SemaphoreHandle_t lvgl_mux = NULL;

static uint16_t *trans_buf_1 = NULL; 
uint8_t *lvgl_dest = NULL;                //旋转buffer
static SemaphoreHandle_t flush_done_semaphore;
static SemaphoreHandle_t frame_done_semaphore;
static SemaphoreHandle_t first_frame_semaphore;
static lv_draw_buf_t display_buffers[2];
static TaskHandle_t lvgl_task_handle;
static QueueHandle_t flush_queue;
typedef struct {
  lv_display_t *display;
  esp_lcd_panel_handle_t panel;
  uint8_t *pixels;
  int width, height;
  uint32_t source_stride, destination_stride;
  lv_display_rotation_t rotation;
  lv_area_t dirty;
} display_flush_job_t;

#define LCD_BIT_PER_PIXEL 16
#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define BUFF_SIZE (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * BYTES_PER_PIXEL)


static const axs15231b_lcd_init_cmd_t lcd_init_cmds[] = 
{
  {0x11, (uint8_t []){0x00}, 0, 100},
  {0x29, (uint8_t []){0x00}, 0, 100},
};

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
  BaseType_t TaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(flush_done_semaphore,&TaskWoken);
  return TaskWoken == pdTRUE;
}

static void example_increase_lvgl_tick(void *arg)
{
  lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void display_rotation_self_test(void)
{
  enum { width = 19, height = 17, pixels = width * height };
  uint16_t source[pixels], actual[pixels + 2], expected[pixels + 2];
  const int regions[][4] = {{0,0,18,16}, {2,15,2,15}, {14,14,18,16}, {0,0,3,12}};
  for (int i = 0; i < pixels; ++i) source[i] = (uint16_t)(i * 173 + 31);
  for (unsigned r = 0; r < sizeof(regions) / sizeof(regions[0]); ++r) {
    for (int i = 0; i < pixels + 2; ++i) actual[i] = expected[i] = 0xA55A;
    const int *region = regions[r];
    // Independent scalar mapping matches LVGL's 270-degree RGB565 rotation.
    for (int x = region[0]; x <= region[2]; ++x)
      for (int y = region[1]; y <= region[3]; ++y) {
        const uint16_t value = source[y * width + x];
        expected[1 + x * height + height - 1 - y] = (uint16_t)((value >> 8) | (value << 8));
      }
    desk_rotate270_rgb565(source, actual + 1, height, width, height,
                          region[0], region[1], region[2], region[3]);
    if (memcmp(actual, expected, sizeof(actual))) abort();
    for (int i = 0; i < pixels; ++i)
      if (source[i] != (uint16_t)(i * 173 + 31)) abort();
  }
  printf("[LCD] rotation self-test passed (full/partial/tile edges/guards)\n");
}

// LVGL keeps the source frame in native RGB565 for DIRECT buffer synchronization.
// The retained rotated image uses panel byte order; never modify the source frame.
static void display_flush_task(void *arg)
{
  display_flush_job_t job;
  while (true) {
    xQueueReceive(flush_queue, &job, portMAX_DELAY);
    const int64_t started = esp_timer_get_time();
    uint16_t *map = (uint16_t *)job.pixels;
    bool panel_byte_order = false;
#if (Rotated == USER_DISP_ROT_90)
    if (job.rotation == LV_DISPLAY_ROTATION_270) {
      static bool initialized = false;
      if (!initialized) {
        job.dirty = (lv_area_t){0, 0, job.width - 1, job.height - 1};
        initialized = true;
      }
      desk_rotate270_rgb565((const uint16_t *)job.pixels, (uint16_t *)lvgl_dest,
                            job.height, job.source_stride / 2, job.destination_stride / 2,
                            job.dirty.x1, job.dirty.y1, job.dirty.x2, job.dirty.y2);
      map = (uint16_t *)lvgl_dest;
      panel_byte_order = true;
    } else if (job.rotation != LV_DISPLAY_ROTATION_0) {
      lv_draw_sw_rotate(job.pixels, lvgl_dest, job.width, job.height,
                        job.source_stride, job.destination_stride,
                        job.rotation, LV_COLOR_FORMAT_RGB565);
      map = (uint16_t *)lvgl_dest;
    }
#endif
    const int64_t rotated = esp_timer_get_time();
    const int rows = LVGL_DMA_BUFF_LEN / (EXAMPLE_LCD_H_RES * 2);
    for (int y = 0; y < EXAMPLE_LCD_V_RES; y += rows) {
      const int count = rows < EXAMPLE_LCD_V_RES - y ? rows : EXAMPLE_LCD_V_RES - y;
      const int bytes = EXAMPLE_LCD_H_RES * count * 2;
      memcpy(trans_buf_1, map, bytes);
      if (!panel_byte_order) lv_draw_sw_rgb565_swap(trans_buf_1, bytes / 2);
      ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(job.panel, 0, y,
                       EXAMPLE_LCD_H_RES, y + count, trans_buf_1));
      xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
      map += EXAMPLE_LCD_H_RES * count;
    }
    const int64_t finished = esp_timer_get_time();
    desk_display_report((uint32_t)(rotated - started), (uint32_t)(finished - rotated));
    static bool first_frame_sent = false;
    if (!first_frame_sent) {
      first_frame_sent = true;
      xSemaphoreGive(first_frame_semaphore);
    }
    xSemaphoreGive(frame_done_semaphore);
  }
}

static void display_flush_wait(lv_display_t *disp)
{
  // LVGL calls this once before reusing a frame. Sleep instead of spinning
  // while the display worker finishes; LVGL clears its flushing flag itself.
  xSemaphoreTake(frame_done_semaphore, portMAX_DELAY);
}

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
  static bool dirty_pending = false;
  static lv_area_t dirty;
  if (!dirty_pending) { dirty = *area; dirty_pending = true; }
  else {
    if (area->x1 < dirty.x1) dirty.x1 = area->x1;
    if (area->y1 < dirty.y1) dirty.y1 = area->y1;
    if (area->x2 > dirty.x2) dirty.x2 = area->x2;
    if (area->y2 > dirty.y2) dirty.y2 = area->y2;
  }
  // DIRECT renders dirty rectangles into one full-size frame. Send only after
  // its final rectangle; intermediate callbacks have no pending DMA work.
  if (!lv_display_flush_is_last(disp)) {
    xSemaphoreGive(frame_done_semaphore);
    return;
  }
  display_flush_job_t job = {};
  job.display = disp;
  job.panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
  job.pixels = color_p;
  job.dirty = dirty;
  dirty_pending = false;
  job.width = lv_display_get_horizontal_resolution(disp);
  job.height = lv_display_get_vertical_resolution(disp);
  job.rotation = lv_display_get_rotation(disp);
  lv_area_t rotated_area = {0, 0, job.width - 1, job.height - 1};
  lv_display_rotate_area(disp, &rotated_area);
  job.source_stride = lv_draw_buf_width_to_stride(job.width, LV_COLOR_FORMAT_RGB565);
  job.destination_stride = lv_draw_buf_width_to_stride(lv_area_get_width(&rotated_area),
                                                     LV_COLOR_FORMAT_RGB565);
  // Keep the panel's existing RAMWR/RAMWRC full-frame protocol.
  if (xQueueSend(flush_queue, &job, 0) != pdTRUE) abort();
}
typedef struct {
  bool pressed;
  uint8_t release_samples;
  uint32_t release_started_ms;
  uint32_t last_contact_ms;
} touch_filter_t;

static bool touch_filter_update(touch_filter_t *filter, uint32_t now,
                                bool contact, bool released)
{
  if (contact) {
    filter->release_samples = 0;
    filter->pressed = true;
    filter->last_contact_ms = now;
  } else {
    if (released) {
      if (!filter->release_samples) filter->release_started_ms = now;
      if (filter->release_samples < 2) ++filter->release_samples;
      if (filter->release_samples >= 2 &&
          (uint32_t)(now - filter->release_started_ms) >= 10) {
        filter->pressed = false;
      }
    } else {
      // A failed transaction or malformed point is not evidence of release.
      filter->release_samples = 0;
    }
    if (filter->pressed && (uint32_t)(now - filter->last_contact_ms) >= 150) {
      // Bound a stale point without latching input off. Some idle reads fail;
      // the next valid contact must still be accepted immediately.
      filter->pressed = false;
    }
  }
  return filter->pressed;
}

static void touch_filter_self_test(void)
{
  touch_filter_t filter = {};
  if (!touch_filter_update(&filter, 0, true, false) ||
      !touch_filter_update(&filter, 40, false, false) ||
      !touch_filter_update(&filter, 80, false, true) ||
      !touch_filter_update(&filter, 90, true, false) ||
      !touch_filter_update(&filter, 100, false, true) ||
       touch_filter_update(&filter, 110, false, true) ||
       touch_filter_update(&filter, 120, false, true) ||
      !touch_filter_update(&filter, 130, true, false) ||
       touch_filter_update(&filter, 300, false, false) ||
      !touch_filter_update(&filter, 310, true, false) ||
      !touch_filter_update(&filter, 320, false, true) ||
       touch_filter_update(&filter, 330, false, true) ||
       touch_filter_update(&filter, 340, false, true) ||
      !touch_filter_update(&filter, 350, true, false)) abort();
  filter = (touch_filter_t){};
  if (!touch_filter_update(&filter, UINT32_MAX - 10, true, false) ||
      !touch_filter_update(&filter, 5, false, false) ||
      !touch_filter_update(&filter, 15, false, true) ||
       touch_filter_update(&filter, 25, false, true) ||
       touch_filter_update(&filter, 35, false, true)) abort();
  filter = (touch_filter_t){};
  if (!touch_filter_update(&filter, 0, true, false) ||
      !touch_filter_update(&filter, 10, false, true) ||
       touch_filter_update(&filter, 20, false, true) ||
      !touch_filter_update(&filter, 30, true, false) ||
      !touch_filter_update(&filter, 40, false, true) ||
       touch_filter_update(&filter, 50, false, true) ||
      !touch_filter_update(&filter, 60, true, false)) abort();
  printf("[TOUCH] filter self-test passed (dropout/2-sample release/rapid tap/recovery/tick wrap)\n");
}

static void TouchInputReadCallback(lv_indev_t * indev, lv_indev_data_t *indevData)
{
  static touch_filter_t filter;
  static lv_point_t last_point;
  uint8_t read_touchpad_cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x0, 0x0, 0x0, 0x0e,0x0, 0x0, 0x0};
  uint8_t buff[32] = {0};
  const bool read_ok = i2c_master_touch_write_read(
      disp_touch_dev_handle,read_touchpad_cmd,11,buff,32) == ESP_OK;
  const uint16_t pointX = (((uint16_t)buff[2] & 0x0f) << 8) | (uint16_t)buff[3];
  const uint16_t pointY = (((uint16_t)buff[4] & 0x0f) << 8) | (uint16_t)buff[5];
  const bool contact = read_ok && buff[1] > 0 && buff[1] < 5 &&
                       pointX < EXAMPLE_LCD_V_RES && pointY < EXAMPLE_LCD_H_RES;
  const bool was_pressed = filter.pressed;
  const bool pressed = touch_filter_update(&filter, lv_tick_get(), contact,
                                           read_ok && buff[1] == 0);
  if (contact && pressed) {
    if (!was_pressed) desk_touch_report(pointX, pointY);
    last_point.x = pointY;
    last_point.y = EXAMPLE_LCD_V_RES - 1 - pointX;
    desk_touch_activity();
  }
  indevData->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  if (pressed) indevData->point = last_point;
}

bool lvgl_port_lock(int timeout_ms)
{
  const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
  return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;       
}

void lvgl_port_unlock(void)
{
  assert(lvgl_mux && "bsp_display_start must be called first");
  xSemaphoreGive(lvgl_mux);
}

void example_lvgl_port_task(void *arg)
{
  uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
  for(;;)
  {
    if (lvgl_port_lock(-1)) 
    {
      const int64_t handler_started = esp_timer_get_time();
      task_delay_ms = lv_timer_handler();
      desk_ui_report((uint32_t)(esp_timer_get_time() - handler_started));
      //Release the mutex
      lvgl_port_unlock();
    }
    if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS)
    {
      task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS)
    {
      task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
    }
    vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
  }
}

void lvgl_port_init(void)
{
  touch_filter_self_test();
  display_rotation_self_test();
  flush_done_semaphore = xSemaphoreCreateBinary();
  assert(flush_done_semaphore);
  ESP_LOGI(TAG, "Initialize LCD RESET GPIO");

  gpio_config_t gpio_conf = {};
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask = ((uint64_t)0X01<<EXAMPLE_PIN_NUM_LCD_RST);
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

  ESP_LOGI(TAG, "Initialize QSPI bus");
  spi_bus_config_t buscfg = {};
    buscfg.data0_io_num = EXAMPLE_PIN_NUM_LCD_DATA0;
    buscfg.data1_io_num = EXAMPLE_PIN_NUM_LCD_DATA1;
    buscfg.sclk_io_num = EXAMPLE_PIN_NUM_LCD_PCLK;
    buscfg.data2_io_num = EXAMPLE_PIN_NUM_LCD_DATA2;
    buscfg.data3_io_num = EXAMPLE_PIN_NUM_LCD_DATA3;
    buscfg.max_transfer_sz = LVGL_DMA_BUFF_LEN;
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  ESP_LOGI(TAG, "Install panel IO");
	  esp_lcd_panel_io_handle_t panel_io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    
  esp_lcd_panel_io_spi_config_t io_config = {};
	io_config.cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS;                 
    io_config.dc_gpio_num = -1;          
    io_config.spi_mode = 3;              
    io_config.pclk_hz = 40 * 1000 * 1000;
    io_config.trans_queue_depth = 10;    
    io_config.on_color_trans_done = example_notify_lvgl_flush_ready; 
    //io_config.user_ctx = &disp_drv,         
    io_config.lcd_cmd_bits = 32;         
    io_config.lcd_param_bits = 8;        
    io_config.flags.quad_mode = true;                         
	ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_config, &panel_io));
    
  axs15231b_vendor_config_t vendor_config = {};
    vendor_config.flags.use_qspi_interface = 1;
    vendor_config.init_cmds = lcd_init_cmds;
    vendor_config.init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]);
    
  esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = -1;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = LCD_BIT_PER_PIXEL;
    panel_config.vendor_config = &vendor_config;

  ESP_LOGI(TAG, "Install panel driver");
  ESP_ERROR_CHECK(esp_lcd_new_panel_axs15231b(panel_io, &panel_config, &panel));
    
	ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_LCD_RST,1));
  vTaskDelay(pdMS_TO_TICKS(30));
  ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_LCD_RST,0));
  vTaskDelay(pdMS_TO_TICKS(250));
  ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_LCD_RST,1));
  vTaskDelay(pdMS_TO_TICKS(30));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

  lv_init();

  lv_display_t * disp = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);  /* 以水平和垂直分辨率（像素）进行基本初始化 */
  lv_display_set_flush_cb(disp, example_lvgl_flush_cb);                           /* 设置刷新回调函数以绘制到显示屏 */
  lv_display_set_flush_wait_cb(disp, display_flush_wait);
  // Limit panel traffic to about 30 frames/s; input still polls every 10 ms.
  lv_timer_set_period(lv_display_get_refr_timer(disp), 33);
  
  uint8_t *buffer_1 = NULL;
  uint8_t *buffer_2 = NULL;
  buffer_1 = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
  buffer_2 = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
  assert(buffer_1);
  assert(buffer_2);
	trans_buf_1 = (uint16_t *)heap_caps_malloc(LVGL_DMA_BUFF_LEN, MALLOC_CAP_DMA);
	assert(trans_buf_1);
  lv_display_set_user_data(disp, panel);
#if (Rotated == USER_DISP_ROT_90)
    lvgl_dest = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM); //旋转buf
    assert(lvgl_dest);
    memset(lvgl_dest, 0, BUFF_SIZE);
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
#endif
  // set_buffers() uses the original portrait geometry, even after rotation.
  // Both DIRECT buffers need the logical geometry before the first sync copy.
  const uint32_t width = lv_display_get_horizontal_resolution(disp);
  const uint32_t height = lv_display_get_vertical_resolution(disp);
  const uint32_t stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_RGB565);
  if (lv_draw_buf_init(&display_buffers[0], width, height, LV_COLOR_FORMAT_RGB565,
                       stride, buffer_1, BUFF_SIZE) != LV_RESULT_OK ||
      lv_draw_buf_init(&display_buffers[1], width, height, LV_COLOR_FORMAT_RGB565,
                       stride, buffer_2, BUFF_SIZE) != LV_RESULT_OK) abort();
  // Exercise LVGL's actual full and partial copy paths, including the last row.
  for (uint32_t y = 0; y < height; ++y) {
    uint16_t *row = (uint16_t *)(buffer_1 + y * stride);
    for (uint32_t x = 0; x < width; ++x) row[x] = (uint16_t)(y * width + x + 1);
  }
  memset(buffer_2, 0, BUFF_SIZE);
  lv_draw_buf_copy(&display_buffers[1], NULL, &display_buffers[0], NULL);
  if (memcmp(buffer_1, buffer_2, stride * height) != 0) abort();
  const lv_area_t copy_area = {width - 7, height - 3, width - 1, height - 1};
  for (uint32_t y = height - 3; y < height; ++y) {
    uint16_t *row = (uint16_t *)(buffer_1 + y * stride);
    for (uint32_t x = width - 7; x < width; ++x) row[x] ^= 0xFFFF;
  }
  lv_draw_buf_copy(&display_buffers[1], &copy_area, &display_buffers[0], &copy_area);
  if (memcmp(buffer_1, buffer_2, stride * height) != 0) abort();
  memset(buffer_1, 0, BUFF_SIZE);
  memset(buffer_2, 0, BUFF_SIZE);
  lv_display_set_draw_buffers(disp, &display_buffers[0], &display_buffers[1]);
  lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_DIRECT);
  ESP_LOGI(TAG, "DIRECT buffer selftest passed: %lux%lu stride=%lu both buffers",
           (unsigned long)width, (unsigned long)height, (unsigned long)stride);
  flush_queue = xQueueCreate(1, sizeof(display_flush_job_t));
  frame_done_semaphore = xSemaphoreCreateBinary();
  first_frame_semaphore = xSemaphoreCreateBinary();
  assert(flush_queue);
  assert(frame_done_semaphore);
  assert(first_frame_semaphore);
  if (xTaskCreatePinnedToCore(display_flush_task, "LCD flush", 4096, NULL,
                              3, NULL, 1) != pdPASS) abort();
  /*port indev*/
  lv_indev_t *touch_indev = NULL;
  touch_indev = lv_indev_create();
  lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_display(touch_indev, disp);
  lv_indev_set_read_cb(touch_indev, TouchInputReadCallback);
  lv_timer_set_period(lv_indev_get_read_timer(touch_indev), 10);
  lv_indev_set_gesture_min_distance(touch_indev, 25);
  lv_indev_set_gesture_min_velocity(touch_indev, 2);

  ESP_LOGI(TAG, "Install LVGL tick timer");
  esp_timer_create_args_t lvgl_tick_timer_args = {};
    lvgl_tick_timer_args.callback = &example_increase_lvgl_tick;
    lvgl_tick_timer_args.name = "lvgl_tick";
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer,LVGL_TICK_PERIOD_MS * 1000));

  lvgl_mux = xSemaphoreCreateMutex();
  assert(lvgl_mux);
}

bool lvgl_port_start(int first_frame_timeout_ms)
{
  if (lvgl_task_handle) return false;
  if (xTaskCreatePinnedToCore(example_lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE,
                             NULL, LVGL_TASK_PRIORITY, &lvgl_task_handle, 0) != pdPASS) abort();
  return xSemaphoreTake(first_frame_semaphore, pdMS_TO_TICKS(first_frame_timeout_ms)) == pdTRUE;
}
