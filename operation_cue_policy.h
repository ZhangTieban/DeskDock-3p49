#pragma once

constexpr bool deskCueBlocked(int radio, int mp3) {
  return radio == 1 || radio == 2 || mp3 == 1 || mp3 == 2;
}
constexpr bool deskCueNeedsRestore(bool switched, bool decoder, int radio, int mp3) {
  return switched && !deskCueBlocked(radio, mp3) && (!decoder || mp3 == 3);
}
