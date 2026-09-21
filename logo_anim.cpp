#include "logo_anim.h"

#include <math.h>
#include <stdlib.h>

#ifndef EXAMPLE_LCD_WIDTH
#define EXAMPLE_LCD_WIDTH 360
#endif

#ifndef EXAMPLE_LCD_HEIGHT
#define EXAMPLE_LCD_HEIGHT 360
#endif

#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF

struct Line { int16_t x0, y0, x1, y1; };
struct Arc  { int16_t x, y, r; float startDeg, endDeg; };
struct Circle { int16_t x, y, r; };

// Coordinates are converted from logo_gear.dxf to 360x360 pixels.
// The DXF border rectangle, annotation and CENTERMARKS layer were omitted.
static const Line kLines[] = {
  {136,271,224,298}, {224,298,224,291}, {136,264,224,291}, {136,264,136,271},
  {224,286,224,259}, {136,259,224,259}, {136,259,224,286}, {245,134,260,117},
  {232,94,260,117}, {217,111,232,94}, {198,104,198,82}, {162,82,198,82},
  {162,104,162,82}, {143,111,129,94}, {101,117,129,94}, {115,134,101,117},
  {105,152,83,148}, {77,184,83,148}, {99,188,77,184}, {102,207,83,219},
  {101,250,83,219}, {121,239,101,250}, {136,252,224,252}, {240,239,259,250},
  {278,219,259,250}, {258,207,278,219}, {262,188,284,184}, {277,148,284,184},
  {255,152,277,148}, {136,283,224,309}, {224,309,224,303}, {136,276,224,303},
  {136,276,136,283}, {136,287,136,315}, {136,315,167,326}, {194,326,224,314},
  {224,314,136,287}, {188,154,200,167}, {200,167,190,230}, {146,141,152,147},
  {152,147,151,181}, {149,148,148,180}, {144,142,149,148}, {152,220,171,200},
  {155,222,174,202}, {196,168,186,228}, {196,168,186,157}, {219,174,214,192},
  {214,173,208,190}
};

static const Arc kArcs[] = {
  {180,184,81,62.938f,77.058f}, {180,184,81,102.935f,117.062f},
  {180,184,81,142.940f,157.062f}, {180,184,81,182.939f,196.815f},
  {180,184,81,222.937f,237.059f}, {180,184,81,302.938f,317.062f},
  {180,184,81,342.938f,357.062f}, {180,184,81,22.938f,37.062f},
  {180,317,16,213.845f,326.162f}, {180,150,9,332.544f,307.210f},
  {185,237,9,334.997f,58.768f}, {180,184,59,282.863f,122.074f},
  {140,134,9,315.208f,5.822f}, {148,189,9,90.644f,69.025f},
  {140,134,9,253.047f,297.060f}, {180,184,59,136.796f,172.703f},
  {116,184,9,303.677f,56.452f}, {180,184,59,187.425f,226.990f},
  {149,229,9,68.248f,168.705f}, {179,195,9,235.768f,210.972f},
  {149,229,9,301.483f,43.201f}, {185,237,9,84.776f,215.451f},
  {180,184,59,243.198f,267.585f}, {180,184,64,188.256f,232.796f},
  {149,229,9,218.794f,251.394f}, {180,184,64,237.392f,121.243f},
  {140,134,9,35.339f,223.531f}, {180,184,64,137.627f,171.872f},
  {116,184,9,85.968f,274.160f}, {218,165,9,276.643f,241.683f},
  {208,199,9,87.462f,51.694f}
};

static const Circle kCircles[] = {
  {139,68,5}, {132,49,5}, {145,86,5}, {126,31,5}, {223,68,5}, {230,50,5},
  {216,86,5}, {237,32,5}, {287,123,5}, {304,113,5}, {270,132,5}, {321,104,5},
  {301,206,5}, {320,209,5}, {282,202,5}, {339,213,5}, {59,204,5}, {40,207,5},
  {78,201,5}, {21,210,5}, {41,101,5}, {57,111,5}, {74,121,5}, {91,131,5}
};

static inline void pixel(uint16_t *fb, int16_t x, int16_t y, uint16_t color) {
  if (x >= 0 && x < EXAMPLE_LCD_WIDTH && y >= 0 && y < EXAMPLE_LCD_HEIGHT)
    fb[y * EXAMPLE_LCD_WIDTH + x] = color;
}

static void line(uint16_t *fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;
  while (true) {
    pixel(fb, x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int16_t e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

static void circle(uint16_t *fb, int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  int16_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
  pixel(fb,x0,y0+r,color); pixel(fb,x0,y0-r,color);
  pixel(fb,x0+r,y0,color); pixel(fb,x0-r,y0,color);
  while (x < y) {
    if (f >= 0) { --y; ddy += 2; f += ddy; }
    ++x; ddx += 2; f += ddx;
    pixel(fb,x0+x,y0+y,color); pixel(fb,x0-x,y0+y,color);
    pixel(fb,x0+x,y0-y,color); pixel(fb,x0-x,y0-y,color);
    pixel(fb,x0+y,y0+x,color); pixel(fb,x0-y,y0+x,color);
    pixel(fb,x0+y,y0-x,color); pixel(fb,x0-y,y0-x,color);
  }
}

// DXF angles are measured counter-clockwise with Y upwards.  The minus before
// sinf() accounts for the display's Y axis, which points downwards.
static void arc(uint16_t *fb, const Arc &a, uint16_t color) {
  float end = a.endDeg;
  if (end < a.startDeg) end += 360.0f;
  const float degToRad = 0.01745329252f;
  const int steps = (int)((end - a.startDeg) * degToRad * a.r) + 1;
  int16_t px = a.x + (int16_t)(cosf(a.startDeg * degToRad) * a.r);
  int16_t py = a.y - (int16_t)(sinf(a.startDeg * degToRad) * a.r);
  for (int i = 1; i <= steps; ++i) {
    float d = a.startDeg + (end - a.startDeg) * i / steps;
    int16_t x = a.x + (int16_t)(cosf(d * degToRad) * a.r);
    int16_t y = a.y - (int16_t)(sinf(d * degToRad) * a.r);
    line(fb, px, py, x, y, color);
    px = x; py = y;
  }
}

void logo_draw_bitmap(esp_lcd_panel_handle_t panel_handle) {
  const size_t size = EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT * sizeof(uint16_t);
  uint16_t *fb = (uint16_t *)calloc(1, size);
  if (!fb) return;

  for (size_t i = 0; i < EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT; ++i) fb[i] = COLOR_BLACK;
  for (size_t i = 0; i < sizeof(kLines) / sizeof(kLines[0]); ++i)
    line(fb, kLines[i].x0, kLines[i].y0, kLines[i].x1, kLines[i].y1, COLOR_WHITE);
  for (size_t i = 0; i < sizeof(kArcs) / sizeof(kArcs[0]); ++i) arc(fb, kArcs[i], COLOR_WHITE);
  for (size_t i = 0; i < sizeof(kCircles) / sizeof(kCircles[0]); ++i)
    circle(fb, kCircles[i].x, kCircles[i].y, kCircles[i].r, COLOR_WHITE);

  esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, EXAMPLE_LCD_WIDTH, EXAMPLE_LCD_HEIGHT, fb);
  free(fb);
}