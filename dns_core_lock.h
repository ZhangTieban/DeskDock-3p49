#pragma once

// DNS cache clearing invokes pending DNS callbacks synchronously, including SNTP.
// Preserve a lock already owned by the caller. Blocking connects stay outside.
template<class Held, class Lock, class Unlock, class Clear>
constexpr void deskClearDnsWithCoreLock(Held held, Lock lock, Unlock unlock, Clear clear) {
  const bool acquire = !held();
  if (acquire) lock();
  clear();
  if (acquire) unlock();
}
