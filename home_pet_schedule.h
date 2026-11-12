#pragma once
#include "home_pet_data.h"

// End times are exclusive. An unknown clock never starts a scheduled event.
static constexpr PetAction scheduledPetAction(int minute, PetAction daytime) {
  if (minute < 0) return daytime;
  if (minute >= 1320 || minute < 420 || (minute >= 780 && minute < 840)) return PetAction::Sleep;
  if ((minute >= 480 && minute < 490) || (minute >= 1080 && minute < 1110)) return PetAction::Eat;
  if (minute >= 1050 && minute < 1080) return PetAction::Leash;
  return daytime;
}
