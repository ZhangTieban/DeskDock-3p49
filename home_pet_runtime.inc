#include "home_pet_schedule.h"

static constexpr int kPetWidth = 296, kPetHeight = 32;
static constexpr uint32_t kPetTickMs = 80;
static int homePetMinute = -1;
static void setHomeDogTime(int minute) { homePetMinute = minute; }

static void drawPetPixel(lv_draw_buf_t *buffer, int x, int y, uint32_t rgb) {
  if (x < 0 || x >= kPetWidth || y < 0 || y >= kPetHeight) return;
  auto *row = reinterpret_cast<uint16_t *>(buffer->data + y * buffer->header.stride);
  row[x] = ((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F);
}

static void drawPetSprite(lv_draw_buf_t *buffer, unsigned index, int x, int y, bool flip) {
  const auto &sprite = kPetSprites[index];
  for (int row = 0; row < sprite.h; ++row) {
    for (int column = 0; column < sprite.w; ++column) {
      const char c = sprite.pixels[row * sprite.w + (flip ? sprite.w - 1 - column : column)];
      if (c == '.') continue;
      const unsigned color = c <= '9' ? c - '0' : c - 'A' + 10;
      drawPetPixel(buffer, x + column, y + row, kPetPalette[color]);
    }
  }
}

static void drawPetLeash(lv_draw_buf_t *buffer, int x1, int y1, int x2, int y2) {
  const int length = std::max(abs(x2-x1), abs(y2-y1));
  for (int i = 0; i <= length; ++i)
    drawPetPixel(buffer, x1 + (x2-x1)*i/std::max(1,length), y1 + (y2-y1)*i/std::max(1,length), 0x81CBB6);
}

static lv_obj_t *homeSourceIcon = nullptr;
static void refreshHomeAudioSource() {
  const int source = mp3UiState() == 2 ? 2 : radioUiState() == 2 ? 1 : 0;
  static int previous = -1;
  if (!homeSourceIcon || source == previous) return;
  previous = source;
  if (!source) { lv_obj_add_flag(homeSourceIcon, LV_OBJ_FLAG_HIDDEN); return; }
  lv_obj_remove_flag(homeSourceIcon, LV_OBJ_FLAG_HIDDEN);
  static const char *icons[] = {
    "............" ".........1.." "......111..." "...111......"
    "..11111111.." "..1......1.." "..1..11..1.." "..1..11..1.."
    "..1......1.." "..11111111.." "............" "............",
    "............" "....111111.." "....1....1.." "....111111.."
    "....1....1.." "....1....1.." "....1....1.." "..111..111.."
    ".1111.1111.." ".111..111..." "............" "............"
  };
  lv_draw_buf_t *buffer = lv_canvas_get_draw_buf(homeSourceIcon);
  for (int y = 0; y < 12; ++y) {
    auto *row = reinterpret_cast<uint16_t *>(buffer->data + y * buffer->header.stride);
    for (int x = 0; x < 12; ++x) {
      const uint32_t rgb = icons[source-1][y*12+x] == '1' ? 0x81CBB6 : kHomeBackground;
      row[x] = ((rgb>>8)&0xF800) | ((rgb>>5)&0x07E0) | ((rgb>>3)&0x001F);
    }
  }
  lv_obj_invalidate(homeSourceIcon);
}

static void animateHomeDog(lv_timer_t *timer) {
  static uint32_t tick = 0, actionTick = 0, lastTouch = 0, heartUntil = 0;
  if (lv_screen_active() != homeScreen) { lastTouch = homePetTouch.load(); return; }
  refreshHomeAudioSource();
  auto *canvas = static_cast<lv_obj_t *>(lv_timer_get_user_data(timer));
  static unsigned dayIndex = 0;
  static constexpr PetAction daytime[] = {PetAction::Walk, PetAction::Run, PetAction::Ball,
      PetAction::Sit, PetAction::Fetch, PetAction::Pet, PetAction::Frisbee, PetAction::Tug};
  static PetAction previous = PetAction::Walk;
  const PetAction action = scheduledPetAction(homePetMinute, daytime[dayIndex]);
  if (action != previous) { actionTick = 0; previous = action; }
  const unsigned index = static_cast<unsigned>(action);
  const PetFrame &frame = kPetActions[index][actionTick % kPetFrameCounts[index]];
  const uint32_t touch = homePetTouch.load();
  if (touch != lastTouch) { lastTouch = touch; heartUntil = tick + 30; }
  const bool heart = tick < heartUntil;
  static PetFrame rendered = {};
  static bool hasRendered = false, hadHeart = false;
  const bool changed = !hasRendered || memcmp(&rendered, &frame, sizeof(frame)) != 0 || heart || hadHeart;
  if (changed) {
    lv_draw_buf_t *buffer = lv_canvas_get_draw_buf(canvas);
    const uint32_t bg = kHomeBackground;
    const uint16_t color = ((bg >> 8) & 0xF800) | ((bg >> 5) & 0x07E0) | ((bg >> 3) & 0x001F);
    // Compose a complete frame before invalidation; no visible erase/draw phase.
    for (int y = 0; y < kPetHeight; ++y) {
      auto *row = reinterpret_cast<uint16_t *>(buffer->data + y * buffer->header.stride);
      std::fill(row, row + kPetWidth, color);
    }
    if (frame.leash) drawPetLeash(buffer, frame.girlX + (frame.girlFlip ? 3 : 15), 16,
                                 frame.x + (frame.flip ? 8 : 24), 21);
    drawPetSprite(buffer, frame.girlPose + 9, frame.girlX, 0, frame.girlFlip);
    drawPetSprite(buffer, frame.pose, frame.x, frame.y, frame.flip);
    drawPetSprite(buffer, frame.toy, frame.toyX, frame.toyY, false);
    if (heart) drawPetSprite(buffer, 20, frame.x + 12, 4 + (tick / 5) % 2, false);
    lv_obj_invalidate(canvas);
    rendered = frame; hasRendered = true; hadHeart = heart;
  }
  ++tick;
  if (++actionTick >= kPetFrameCounts[index]) {
    actionTick = 0;
    if (action != PetAction::Sleep && action != PetAction::Eat && action != PetAction::Leash)
      dayIndex = (dayIndex + 1) % (sizeof(daytime) / sizeof(daytime[0]));
  }
}

static void buildHomeDog() {
  lv_draw_buf_t *buffer = lv_draw_buf_create(kPetWidth, kPetHeight, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
  if (!buffer) return;
  lv_obj_t *canvas = lv_canvas_create(homeScreen);
  lv_canvas_set_draw_buf(canvas, buffer);
  lv_obj_set_pos(canvas, 8, 139);
  lv_obj_remove_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_canvas_fill_bg(canvas, lv_color_hex(kHomeBackground), LV_OPA_COVER);
  lv_timer_create(animateHomeDog, kPetTickMs, canvas);
  lv_draw_buf_t *iconBuffer = lv_draw_buf_create(12, 12, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
  if (iconBuffer) {
    homeSourceIcon = lv_canvas_create(homeScreen);
    lv_canvas_set_draw_buf(homeSourceIcon, iconBuffer);
    lv_obj_set_pos(homeSourceIcon, 12, 28);
    lv_obj_add_flag(homeSourceIcon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(homeSourceIcon, LV_OBJ_FLAG_CLICKABLE);
  }
}
