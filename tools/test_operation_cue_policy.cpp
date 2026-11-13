#include "../operation_cue_policy.h"

// Paused decoders keep their position but must release the output to cues.
static_assert(!deskCueBlocked(0,3));
static_assert(deskCueNeedsRestore(true,true,0,3));
static_assert(!deskCueNeedsRestore(false,true,0,3));
// Connection/opening/playback still owns I2S and must never be interrupted.
static_assert(deskCueBlocked(1,0) && deskCueBlocked(2,0));
static_assert(deskCueBlocked(0,1) && deskCueBlocked(0,2));
static_assert(!deskCueNeedsRestore(true,true,2,0));
static_assert(!deskCueNeedsRestore(true,true,0,2));
// Failed restoration remains retryable after stop, failed open and EOF.
static_assert(deskCueNeedsRestore(true,false,0,0));
static_assert(deskCueNeedsRestore(true,false,3,0));
static_assert(deskCueNeedsRestore(true,false,0,4));
static_assert(deskCueNeedsRestore(true,false,0,5));
static_assert(!deskCueNeedsRestore(false,false,0,0));
static_assert(!deskCueNeedsRestore(true,true,3,0));
int main() {}
