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
bool lvgl_port_start(int first_frame_timeout_ms);
bool lvgl_port_lock(int timeout_ms);
void lvgl_port_unlock(void);


#ifdef __cplusplus
}
#endif



#endif








