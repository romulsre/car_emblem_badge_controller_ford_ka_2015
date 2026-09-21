#include "eye_animation.h"
#include <stdlib.h>

#ifndef EXAMPLE_LCD_WIDTH
#define EXAMPLE_LCD_WIDTH 360
#endif

#ifndef EXAMPLE_LCD_HEIGHT
#define EXAMPLE_LCD_HEIGHT 360
#endif

#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF

// All geometry below is in display pixels, already adapted for 360 x 360.
// The sclera occupies x=50..310 and y=105..255, leaving at least 50 px margin.
static const int16_t kEyeX = 180;
static const int16_t kEyeY = 180;
static const int16_t kEyeRadiusX = 130;
static const int16_t kEyeRadiusY = 75;
static const int16_t kIrisRadius = 58;
static const int16_t kPupilRadius = 32;

struct EyeAnimationState {
  uint16_t *framebuffer;
  uint16_t irisColor;
  int16_t fromX, fromY;
  int16_t targetX, targetY;
  uint32_t moveStartedAt;
  uint32_t moveDuration;
  uint32_t holdUntil;
  uint32_t lastFrameAt;
  uint32_t randomState;
  bool active;
};

static EyeAnimationState s_eye = {};

static inline void put_pixel(uint16_t *fb, int16_t x, int16_t y, uint16_t color) {
  if (x >= 0 && x < EXAMPLE_LCD_WIDTH && y >= 0 && y < EXAMPLE_LCD_HEIGHT)
    fb[y * EXAMPLE_LCD_WIDTH + x] = color;
}

static inline bool inside_eye(int16_t x, int16_t y) {
  int32_t dx = x - kEyeX;
  int32_t dy = y - kEyeY;
  return dx * dx * kEyeRadiusY * kEyeRadiusY + dy * dy * kEyeRadiusX * kEyeRadiusX
      <= (int32_t)kEyeRadiusX * kEyeRadiusX * kEyeRadiusY * kEyeRadiusY;
}

static void fill_screen(uint16_t *fb, uint16_t color) {
  for (uint32_t i = 0; i < EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT; ++i) fb[i] = color;
}

static void fill_eye_white(uint16_t *fb) {
  for (int16_t y = kEyeY - kEyeRadiusY; y <= kEyeY + kEyeRadiusY; ++y) {
    for (int16_t x = kEyeX - kEyeRadiusX; x <= kEyeX + kEyeRadiusX; ++x) {
      if (inside_eye(x, y)) put_pixel(fb, x, y, COLOR_WHITE);
    }
  }
}

static void fill_clipped_circle(uint16_t *fb, int16_t cx, int16_t cy, int16_t radius,
                                uint16_t color) {
  int32_t rr = (int32_t)radius * radius;
  for (int16_t y = cy - radius; y <= cy + radius; ++y) {
    for (int16_t x = cx - radius; x <= cx + radius; ++x) {
      int32_t dx = x - cx, dy = y - cy;
      if (dx * dx + dy * dy <= rr && inside_eye(x, y)) put_pixel(fb, x, y, color);
    }
  }
}

static uint32_t next_random() {
  s_eye.randomState = s_eye.randomState * 1664525UL + 1013904223UL;
  return s_eye.randomState;
}

static int16_t random_between(int16_t minValue, int16_t maxValue) {
  return minValue + (int16_t)(next_random() % (uint32_t)(maxValue - minValue + 1));
}

static void choose_next_target(uint32_t now_ms) {
  s_eye.fromX = s_eye.targetX;
  s_eye.fromY = s_eye.targetY;

  // Conservative range makes the iris remain mostly visible; its edge is
  // naturally clipped by the white ellipse near the eyelids.
  s_eye.targetX = kEyeX + random_between(-46, 46);
  s_eye.targetY = kEyeY + random_between(-20, 20);
  s_eye.moveStartedAt = now_ms;
  s_eye.moveDuration = 650U + next_random() % 550U;
  s_eye.holdUntil = now_ms + s_eye.moveDuration + 350U + next_random() % 900U;
}

static void draw_frame(esp_lcd_panel_handle_t panel_handle, int16_t irisX, int16_t irisY) {
  uint16_t *fb = s_eye.framebuffer;
  fill_screen(fb, COLOR_BLACK);
  fill_eye_white(fb);
  fill_clipped_circle(fb, irisX, irisY, kIrisRadius, s_eye.irisColor);
  fill_clipped_circle(fb, irisX, irisY, kPupilRadius, COLOR_BLACK);

  // Reflection from the source image.  It moves together with the pupil.
  fill_clipped_circle(fb, irisX - 20, irisY - 21, 5, COLOR_WHITE);
  for (int16_t y = -38; y <= -20; ++y)
    for (int16_t x = -31; x <= -14; ++x)
      if (x + y >= -55 && x + y <= -48)
        if (inside_eye(irisX + x, irisY + y))
          put_pixel(fb, irisX + x, irisY + y, COLOR_WHITE);

  esp_lcd_panel_draw_bitmap(panel_handle, 0, 0,
                            EXAMPLE_LCD_WIDTH, EXAMPLE_LCD_HEIGHT, fb);
}

bool eye_animation_begin(esp_lcd_panel_handle_t panel_handle, uint16_t iris_rgb565) {
  eye_animation_end();
  const size_t framebufferSize = EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT * sizeof(uint16_t);
  s_eye.framebuffer = (uint16_t *)calloc(1, framebufferSize);
  if (!s_eye.framebuffer) return false;

  s_eye.irisColor = iris_rgb565;
  s_eye.fromX = s_eye.targetX = kEyeX;
  s_eye.fromY = s_eye.targetY = kEyeY;
  s_eye.randomState = 0x6D2B79F5UL;
  s_eye.active = true;
  draw_frame(panel_handle, kEyeX, kEyeY);
  return true;
}

void eye_animation_set_iris_color(uint16_t iris_rgb565) {
  s_eye.irisColor = iris_rgb565;
}

void eye_animation_update(esp_lcd_panel_handle_t panel_handle, uint32_t now_ms) {
  if (!s_eye.active || !s_eye.framebuffer) return;
  if ((uint32_t)(now_ms - s_eye.lastFrameAt) < 25U) return;  // maximum 40 FPS
  s_eye.lastFrameAt = now_ms;

  if (s_eye.holdUntil == 0 || (int32_t)(now_ms - s_eye.holdUntil) >= 0)
    choose_next_target(now_ms);

  uint32_t elapsed = now_ms - s_eye.moveStartedAt;
  float t = elapsed >= s_eye.moveDuration ? 1.0f : (float)elapsed / s_eye.moveDuration;
  // Smoothstep: zero acceleration at the beginning and end of each glance.
  t = t * t * (3.0f - 2.0f * t);
  int16_t irisX = s_eye.fromX + (int16_t)((s_eye.targetX - s_eye.fromX) * t);
  int16_t irisY = s_eye.fromY + (int16_t)((s_eye.targetY - s_eye.fromY) * t);
  draw_frame(panel_handle, irisX, irisY);
}

void eye_animation_end() {
  if (s_eye.framebuffer) free(s_eye.framebuffer);
  s_eye = {};
}
