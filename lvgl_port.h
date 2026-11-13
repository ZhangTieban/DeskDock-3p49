#ifndef LVGL_PORT_H
#define LVGL_PORT_H
#include <stdbool.h>
#include <stdint.h>



#ifdef __cplusplus
extern "C" {
#endif


void lvgl_port_init(void);
void lvgl_port_cancel_pending_touch(void);
void lvgl_port_touch_stats(uint32_t *reads,uint32_t *errors,uint32_t *recoveries,uint32_t *overflows);
typedef struct {
  uint32_t time_ms;
  int32_t error,head_age,latest_age;
  uint16_t x,y;
  uint8_t count,event,pressed,cancel,kind;
} desk_touch_trace_t;
bool lvgl_port_take_touch_trace(desk_touch_trace_t *trace);
bool lvgl_port_start(int first_frame_timeout_ms);
bool lvgl_port_lock(int timeout_ms);
void lvgl_port_unlock(void);


#ifdef __cplusplus
}
#endif



#endif








