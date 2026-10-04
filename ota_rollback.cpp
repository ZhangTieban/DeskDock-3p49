#include <stdbool.h>

// Arduino ESP32's weak default confirms a new image before setup().
// Keep it pending until DeskDock finishes its own first-boot checks.
extern "C" bool verifyRollbackLater(void) {
  return true;
}
