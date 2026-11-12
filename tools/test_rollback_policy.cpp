#include "../ota_rollback_policy.h"
static_assert(rollbackOlder("0.2.03","0.2.04"));
static_assert(rollbackOlder("0.1.27","0.2.04"));
static_assert(rollbackOlder("0.9.99","0.10.0"));
static_assert(!rollbackOlder("0.2.04","0.2.04"));
static_assert(!rollbackOlder("0.2.05","0.2.04"));
static_assert(!rollbackOlder("0.2.03junk","0.2.04"));
static_assert(!rollbackOlder("0.2","0.2.04"));
static_assert(!rollbackOlder(".2.3","0.2.04"));
static_assert(!rollbackOlder("0..3","0.2.04"));
static_assert(!rollbackOlder("0.2.-1","0.2.04"));
static_assert(!rollbackOlder("65536.2.3","65535.9.9"));
static_assert(!rollbackOlder("000000.2.3","1.0.0"));
static_assert(!rollbackOlder(nullptr,"0.2.04"));
constexpr bool markerTests() {
  char version[24] = {};
  constexpr char valid[] = DESKDOCK_ROLLBACK_ID_PREFIX "0.2.04|";
  constexpr char suffix[] = DESKDOCK_ROLLBACK_ID_PREFIX "0.2.04rc1|";
  constexpr char other[] = "OtherBoard|rollback1|0.2.03|";
  if (!rollbackMarkerVersion(valid,sizeof(valid),version) || version[5] != '4') return false;
  if (rollbackMarkerVersion(valid,sizeof(valid)-1,version)) return false;
  if (rollbackMarkerVersion(suffix,sizeof(suffix),version)) return false;
  if (rollbackMarkerVersion(other,sizeof(other),version)) return false;
  return true;
}
static_assert(markerTests(), "Image identity marker parser");
