#include "../touch_input.h"

constexpr bool filter_cases() {
  desk_touch_filter_t f{};
  if (desk_touch_update(&f,0,true,false,10,20).pressed) return false;
  // A lone noisy contact must not become a tap.
  if (desk_touch_update(&f,10,false,true,0,0).pressed) return false;
  if (desk_touch_update(&f,20,true,false,10,20).pressed) return false;
  if (!desk_touch_update(&f,30,true,false,11,21).pressed) return false;
  // A failed read must not release a live drag.
  if (!desk_touch_update(&f,40,false,false,0,0).pressed) return false;
  if (!desk_touch_update(&f,50,true,false,40,50).pressed) return false;
  if (!desk_touch_update(&f,60,false,true,0,0).pressed) return false;
  auto up=desk_touch_update(&f,70,false,true,0,0);
  if (up.pressed || up.cancel || up.x!=40 || up.y!=50) return false;
  desk_touch_update(&f,80,true,false,10,20);
  desk_touch_update(&f,90,true,false,10,20);
  if (!desk_touch_update(&f,239,false,false,0,0).pressed) return false;
  auto lost=desk_touch_update(&f,240,false,false,0,0);
  return !lost.pressed && lost.cancel;
}
constexpr bool movement_and_wrap() {
  desk_touch_filter_t f{};
  desk_touch_update(&f,0xfffffff0u,true,false,10,20);
  if (desk_touch_update(&f,0xfffffffau,true,false,100,120).pressed) return false;
  if (!desk_touch_update(&f,4,true,false,101,121).pressed) return false;
  desk_touch_update(&f,14,false,true,0,0);
  return !desk_touch_update(&f,24,false,true,0,0).pressed;
}
constexpr bool queue_tap_and_drag() {
  desk_touch_queue_t q{};
  desk_touch_push(&q,{10,20,true,false,10});
  desk_touch_push(&q,{20,30,true,false,20});
  desk_touch_push(&q,{20,30,false,false,30});
  auto a=desk_touch_pop(&q,40),b=desk_touch_pop(&q,40),c=desk_touch_pop(&q,40);
  return a.pressed && a.x==10 && b.pressed && b.x==20 && !c.pressed && !c.cancel && !q.count;
}
constexpr bool cancellations() {
  desk_touch_queue_t q{};
  desk_touch_push(&q,{10,20,true,false,10});
  if (!desk_touch_pop(&q,20).pressed) return false;
  // Modal/screen switches cancel and suppress the held finger until released.
  desk_touch_cancel_queue(&q);
  if (!desk_touch_pop(&q,20).cancel) return false;
  // A second cancellation must not re-enable an already suppressed finger.
  desk_touch_cancel_queue(&q);
  if (!desk_touch_pop(&q,20).cancel) return false;
  desk_touch_push(&q,{20,30,true,false,30});
  if (desk_touch_pop(&q,30).pressed) return false;
  desk_touch_push(&q,{20,30,false,false,40});
  desk_touch_push(&q,{10,20,true,false,50});
  if (!desk_touch_pop(&q,50).pressed) return false;
  auto stale=desk_touch_pop(&q,200);
  if (!stale.cancel || stale.pressed) return false;
  // A completed but stale queued tap must not click a newly visible control.
  desk_touch_queue_t old{};
  desk_touch_push(&old,{10,20,true,false,10});
  desk_touch_push(&old,{10,20,false,false,30});
  auto discarded=desk_touch_pop(&old,400);
  return discarded.cancel && !discarded.pressed && !desk_touch_pop(&old,400).cancel;
}
constexpr bool overflow_and_wrap_queue() {
  desk_touch_queue_t q{};
  for (int i=0;i<33;++i) desk_touch_push(&q,{(int16_t)(i*3),20,true,false,(uint32_t)i});
  auto cancelled=desk_touch_pop(&q,40);
  if (q.overflows!=1 || !cancelled.cancel || cancelled.pressed) return false;
  desk_touch_push(&q,{100,20,true,false,50});
  if (desk_touch_pop(&q,50).pressed) return false;
  desk_touch_push(&q,{100,20,false,false,60});
  desk_touch_push(&q,{10,20,true,false,0xfffffff0u});
  return desk_touch_pop(&q,10).pressed;
}
static_assert(filter_cases(),"contact qualification, drag and release");
static_assert(movement_and_wrap(),"coordinate noise and clock wrap");
static_assert(queue_tap_and_drag(),"queued tap/drag ordering");
static_assert(cancellations(),"modal, held touch and stale input cancellation");
static_assert(overflow_and_wrap_queue(),"overflow cancellation and clock wrap");
int main() { return 0; }
