#pragma once
#include <stddef.h>
#include <stdint.h>

#define DESKDOCK_ROLLBACK_ID_PREFIX "DeskDock-3p49|rollback1|"

// Numeric comparison, bounded fields, exact separators; no suffixes or partial matches.
static constexpr bool rollbackVersion(const char *text, uint16_t (&parts)[3]) {
  if (!text) return false;
  unsigned pos = 0;
  for (unsigned field = 0; field < 3; ++field) {
    unsigned value = 0, count = 0;
    while (text[pos] >= '0' && text[pos] <= '9') {
      value = value * 10 + unsigned(text[pos++] - '0');
      if (++count > 5 || value > 65535) return false;
    }
    if (!count || (field != 2 ? text[pos++] != '.' : text[pos] != '\0')) return false;
    parts[field] = static_cast<uint16_t>(value);
  }
  return true;
}
static constexpr bool rollbackOlder(const char *candidate, const char *current) {
  uint16_t a[3] = {}, b[3] = {};
  if (!rollbackVersion(candidate,a) || !rollbackVersion(current,b)) return false;
  for (unsigned i = 0; i < 3; ++i) if (a[i] != b[i]) return a[i] < b[i];
  return false;
}

static constexpr bool rollbackMarkerVersion(const char *data, size_t available, char (&version)[24]) {
  constexpr char prefix[] = DESKDOCK_ROLLBACK_ID_PREFIX;
  const size_t start = sizeof(prefix)-1;
  if (available <= start+1) return false;
  for (size_t i = 0; i < start; ++i) if (data[i] != prefix[i]) return false;
  size_t n = 0;
  while (n < 23 && start+n < available && data[start+n] != '|' && data[start+n]) {
    version[n] = data[start+n]; ++n;
  }
  version[n] = '\0';
  uint16_t parts[3] = {};
  return start+n+1 < available && data[start+n] == '|' && data[start+n+1] == '\0' &&
         rollbackVersion(version,parts);
}
