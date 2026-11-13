#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
#define DESK_TOUCH_FN constexpr
#else
#define DESK_TOUCH_FN
#endif

typedef struct {
  int16_t x,y;
  bool pressed,cancel;
  uint32_t time_ms;
} desk_touch_sample_t;
typedef struct {
  bool pressed;
  uint8_t down_count,up_count;
  int16_t candidate_x,candidate_y,x,y;
  uint32_t down_ms,up_ms,last_contact_ms;
} desk_touch_filter_t;

static DESK_TOUCH_FN int desk_touch_abs(int x) { return x<0?-x:x; }
static DESK_TOUCH_FN desk_touch_sample_t desk_touch_update(desk_touch_filter_t *f,
    uint32_t now,bool contact,bool released,int16_t x,int16_t y) {
  desk_touch_sample_t result={0};
  result.time_ms=now;
  if (contact) {
    f->up_count=0;
    if (!f->pressed) {
      if (!f->down_count || desk_touch_abs(x-f->candidate_x)>12 ||
          desk_touch_abs(y-f->candidate_y)>12) {
        f->down_count=1;f->down_ms=now;
      } else if (f->down_count<2) ++f->down_count;
      f->candidate_x=x;f->candidate_y=y;
      if (f->down_count>=2 && (uint32_t)(now-f->down_ms)>=8) f->pressed=true;
    }
    if (f->pressed) { f->x=x;f->y=y;f->last_contact_ms=now; }
  } else {
    f->down_count=0;
    if (released) {
      if (!f->up_count) f->up_ms=now;
      if (f->up_count<2) ++f->up_count;
      if (f->up_count>=2 && (uint32_t)(now-f->up_ms)>=10) f->pressed=false;
    } else f->up_count=0;
    if (f->pressed && (uint32_t)(now-f->last_contact_ms)>=150) {
      f->pressed=false;result.cancel=true;
    }
  }
  result.pressed=f->pressed;result.x=f->x;result.y=f->y;
  return result;
}

#define DESK_TOUCH_QUEUE_CAPACITY 32
typedef struct {
  desk_touch_sample_t events[DESK_TOUCH_QUEUE_CAPACITY],latest;
  uint8_t head,count;
  bool suppress;
  uint32_t overflows;
} desk_touch_queue_t;

// Call with the short queue critical section held; no LVGL or I2C operations.
static DESK_TOUCH_FN void desk_touch_cancel_queue(desk_touch_queue_t *q) {
  desk_touch_sample_t cancelled=q->latest;
  q->suppress=q->suppress || cancelled.pressed;
  cancelled.pressed=false;cancelled.cancel=true;
  q->head=0;q->count=1;q->events[0]=cancelled;
  q->latest.pressed=false;q->latest.cancel=false;
}
static DESK_TOUCH_FN void desk_touch_push(desk_touch_queue_t *q,desk_touch_sample_t sample) {
  if (q->suppress) {
    if (!sample.pressed) q->suppress=false;
    q->latest.time_ms=sample.time_ms;
    return;
  }
  const bool changed=sample.cancel || sample.pressed!=q->latest.pressed ||
      (sample.pressed && (desk_touch_abs(sample.x-q->latest.x)>=2 ||
                          desk_touch_abs(sample.y-q->latest.y)>=2));
  if (changed && q->count==DESK_TOUCH_QUEUE_CAPACITY) {
    q->latest=sample;desk_touch_cancel_queue(q);++q->overflows;
    return;
  }
  if (changed) {
    q->events[(q->head+q->count)%DESK_TOUCH_QUEUE_CAPACITY]=sample;++q->count;
  }
  q->latest=sample;q->latest.cancel=false;
}
static DESK_TOUCH_FN desk_touch_sample_t desk_touch_pop(desk_touch_queue_t *q,uint32_t now) {
  if ((q->count && (uint32_t)(now-q->events[q->head].time_ms)>300) ||
      (q->latest.pressed && (uint32_t)(now-q->latest.time_ms)>=150)) {
    desk_touch_cancel_queue(q);
    q->events[0].time_ms=now;
  }
  if (!q->count) return q->latest;
  desk_touch_sample_t result=q->events[q->head];
  q->head=(q->head+1)%DESK_TOUCH_QUEUE_CAPACITY;--q->count;
  return result;
}
#undef DESK_TOUCH_FN
