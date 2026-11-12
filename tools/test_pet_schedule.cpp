#include "../home_pet_schedule.h"
constexpr bool everyMinuteMatches() {
  for (int m = 0; m < 1440; ++m) {
    const bool sleep = m < 420 || m >= 1320 || (m >= 780 && m < 840);
    const bool eat = (m >= 480 && m < 490) || (m >= 1080 && m < 1110);
    const bool leash = m >= 1050 && m < 1080;
    const PetAction expected = sleep ? PetAction::Sleep : eat ? PetAction::Eat :
                               leash ? PetAction::Leash : PetAction::Run;
    if (scheduledPetAction(m, PetAction::Run) != expected) return false;
  }
  return scheduledPetAction(-1, PetAction::Walk) == PetAction::Walk;
}
static_assert(everyMinuteMatches(), "Pet schedule must match all 1440 minutes");
