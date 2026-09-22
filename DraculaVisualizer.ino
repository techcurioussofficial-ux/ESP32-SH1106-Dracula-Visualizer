#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <math.h>
#include <string.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// ========== ESP32 SPI Pins ==========
#define OLED_MOSI   23
#define OLED_CLK    18
#define OLED_DC     17
#define OLED_CS     5
#define OLED_RESET  16
// ====================================

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT,
                         OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

// =========================================================================
// GLOBAL STATE & VARIABLES
// =========================================================================
enum BackgroundMode {
  BG_BATS = 0,
  BG_MANDALA,
  BG_SYNTHWAVE,
  BG_TUNNEL_EQ,
  BG_VORTEX,
  BG_MATRIX,
  BG_OSCILLOSCOPE,
  BG_LIGHTNING,
  BG_CUBE
};
BackgroundMode currentBg = BG_TUNNEL_EQ;
int lastLyricIdx = -1;
unsigned long startTime = 0;
bool isPlaying = true;
unsigned long TOTAL_LOOP_TIME = 0;

// --- 1. Bat Swarm ---
#define NUM_BATS 15
struct Bat {
  float x, y;
  float vx, vy;
  float framePhase;
  float scale;
} bats[NUM_BATS];

// --- 3. Geometric Mandala ---
float mAngle1 = 0.0, mAngle2 = 0.0, mAngle3 = 0.0;
float expandDist = 0.0;

// --- 4. Synthwave 3D ---
float gridOffset = 0.0;
int swStarsX[15], swStarsY[15];

// --- 5. Tunnel & EQ ---
#define NUM_CIRCLES 7
float circleRadii[NUM_CIRCLES];
#define NUM_BARS 16
int barHeights[NUM_BARS], targetHeights[NUM_BARS];
struct Shockwave { float radius; bool active; } wave;

// --- 6. Vortex ---
float vortexTime = 0.0;

// --- 7. Matrix Rain ---
#define MATRIX_COLS 16
struct MatrixCol { float y; float speed; int x; int trailLen; } matrixCols[MATRIX_COLS];

// --- 8. Oscilloscope ---
float oscPhase = 0.0;

// --- 9. Lightning ---
#define MAX_BOLT_PTS 10
#define MAX_BOLTS 3
struct LightningBolt {
  int bx[MAX_BOLT_PTS], by[MAX_BOLT_PTS];
  int numPts; int framesLeft; bool active;
} bolts[MAX_BOLTS];

// --- 10. 3D Cube ---
float cubeAX = 0.0, cubeAY = 0.0, cubeAZ = 0.0;

// =========================================================================
// EFFECTS & LYRICS
// =========================================================================
enum Effect {
  EFFECT_NONE = 0,
  EFFECT_POP,
  EFFECT_SHAKE,
  EFFECT_INVERT,
  EFFECT_GLITCH,
  EFFECT_ZOOM_IN,
  EFFECT_TYPEWRITER,
  EFFECT_BOUNCE
};

struct Lyric {
  unsigned long delayBeforeMs;
  unsigned long durationMs;
  const char* word;
  Effect effect;
  uint8_t size;
  unsigned long calculatedStartTime;
};

// =========================================================================
// LYRICS TIMELINE
// =========================================================================
Lyric lyrics[] = {
  { 0, 300, "The", EFFECT_NONE, 2, 0 },
  { 0, 300, "morning", EFFECT_POP, 2, 0 },
  { 0, 300, "light", EFFECT_NONE, 2, 0 },
  { 0, 300, "is", EFFECT_NONE, 2, 0 },
  { 0, 300, "turning", EFFECT_NONE, 2, 0 },
  { 100, 500, "BLUE", EFFECT_INVERT, 3, 0 },
  { 0, 300, "the", EFFECT_NONE, 2, 0 },
  { 0, 300, "feeling", EFFECT_BOUNCE, 2, 0 },
  { 0, 300, "is", EFFECT_NONE, 2, 0 },
  { 0, 800, "BIZARRE", EFFECT_TYPEWRITER, 2, 0 },
  { 200, 300, "The", EFFECT_NONE, 2, 0 },
  { 0, 300, "night", EFFECT_POP, 2, 0 },
  { 0, 300, "is", EFFECT_NONE, 2, 0 },
  { 0, 300, "almost", EFFECT_NONE, 2, 0 },
  { 0, 500, "over", EFFECT_NONE, 2, 0 },
  { 200, 300, "I", EFFECT_NONE, 2, 0 },
  { 0, 300, "still", EFFECT_BOUNCE, 2, 0 },
  { 0, 300, "don't", EFFECT_NONE, 2, 0 },
  { 0, 300, "know", EFFECT_NONE, 2, 0 },
  { 0, 200, "where", EFFECT_NONE, 2, 0 },
  { 0, 200, "you", EFFECT_NONE, 2, 0 },
  { 0, 600, "ARE", EFFECT_SHAKE, 3, 0 },
  { 200, 300, "The", EFFECT_NONE, 2, 0 },
  { 0, 400, "shadows", EFFECT_SHAKE, 2, 0 },
  { 0, 300, "yeah", EFFECT_NONE, 2, 0 },
  { 0, 300, "they", EFFECT_NONE, 2, 0 },
  { 0, 300, "keep", EFFECT_NONE, 2, 0 },
  { 0, 300, "me", EFFECT_NONE, 2, 0 },
  { 0, 300, "pretty", EFFECT_POP, 2, 0 },
  { 0, 300, "like", EFFECT_NONE, 2, 0 },
  { 0, 300, "a", EFFECT_NONE, 2, 0 },
  { 0, 300, "movie", EFFECT_NONE, 2, 0 },
  { 0, 800, "STAR", EFFECT_INVERT, 3, 0 },
  { 300, 500, "Daylight", EFFECT_TYPEWRITER, 2, 0 },
  { 0, 300, "makes", EFFECT_NONE, 2, 0 },
  { 0, 300, "me", EFFECT_NONE, 2, 0 },
  { 0, 300, "feel", EFFECT_BOUNCE, 2, 0 },
  { 0, 200, "like", EFFECT_NONE, 2, 0 },
  { 100, 2800, "DRACULA", EFFECT_ZOOM_IN, 3, 0 }
};
const int numLyrics = sizeof(lyrics) / sizeof(Lyric);

// Forward declarations
void drawBats(Effect eff, bool lyricActive);
void drawMandalaBackground(Effect eff, bool lyricActive);
void drawSynthwaveBackground(Effect eff, bool lyricActive);
void drawTunnel(Effect eff, bool lyricActive);
void drawEQ(Effect eff, bool lyricActive);
void drawShockwave();
void drawVortex(bool lyricActive);
void drawMatrixRain(Effect eff, bool lyricActive);
void drawOscilloscope(Effect eff, bool lyricActive);
void drawLightning(Effect eff, bool lyricActive);
void drawCube(Effect eff, bool lyricActive);
void drawRotatedTriangle(int cx, int cy, float radius, float a, bool fill);
void drawRotatedSquare(int cx, int cy, float radius, float a);
void generateBolt(int idx);

// =========================================================================
// SETUP
// =========================================================================
void setup() {
  Serial.begin(115200);

  if (!display.begin(0x3C, true)) {
    Serial.println(F("SH1106 allocation failed"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);

  // Init Bats
  for (int i = 0; i < NUM_BATS; i++) {
    bats[i].x = random(0, SCREEN_WIDTH);
    bats[i].y = random(0, SCREEN_HEIGHT);
    bats[i].vx = random(-20, 20) / 10.0;
    if (bats[i].vx == 0) bats[i].vx = 1.0;
    bats[i].vy = random(-10, 10) / 10.0;
    bats[i].framePhase = random(0, 100) / 10.0;
    bats[i].scale = random(10, 25) / 10.0;
  }

  // Init Synthwave Stars
  for (int i = 0; i < 15; i++) {
    swStarsX[i] = random(0, SCREEN_WIDTH);
    swStarsY[i] = random(0, 32);
  }

  // Init Tunnel & EQ
  for (int i = 0; i < NUM_CIRCLES; i++) circleRadii[i] = i * (100.0 / NUM_CIRCLES);
  for (int i = 0; i < NUM_BARS; i++) { barHeights[i] = 0; targetHeights[i] = 0; }
  wave.active = false;

  // Init Matrix Rain
  for (int i = 0; i < MATRIX_COLS; i++) {
    matrixCols[i].x = i * (SCREEN_WIDTH / MATRIX_COLS) + random(0, 4);
    matrixCols[i].y = random(-30, SCREEN_HEIGHT);
    matrixCols[i].speed = random(10, 30) / 10.0;
    matrixCols[i].trailLen = random(8, 22);
  }

  // Init Lightning
  for (int i = 0; i < MAX_BOLTS; i++) bolts[i].active = false;

  // Calculate Timeline
  unsigned long runningTime = 0;
  for (int i = 0; i < numLyrics; i++) {
    runningTime += lyrics[i].delayBeforeMs;
    lyrics[i].calculatedStartTime = runningTime;
    runningTime += lyrics[i].durationMs;
  }
  TOTAL_LOOP_TIME = runningTime + 3000;
  startTime = millis();
}

// =========================================================================
// MAIN LOOP
// =========================================================================
void loop() {
  if (!isPlaying) return;

  unsigned long now = millis() - startTime;
  if (now > TOTAL_LOOP_TIME) {
    startTime = millis();
    now = 0;
    lastLyricIdx = -1;
    currentBg = BG_TUNNEL_EQ;
  }

  display.clearDisplay();

  int currentLyricIdx = -1;
  for (int i = 0; i < numLyrics; i++) {
    if (now >= lyrics[i].calculatedStartTime && now <= (lyrics[i].calculatedStartTime + lyrics[i].durationMs)) {
      currentLyricIdx = i;
      break;
    }
  }

  Effect currentEffect = EFFECT_NONE;
  bool lyricActive = false;
  float progress = 0.0;
  bool invertScreen = false;

  if (currentLyricIdx != -1) {
    lyricActive = true;
    const Lyric& l = lyrics[currentLyricIdx];
    currentEffect = l.effect;
    float elapsedLyric = now - l.calculatedStartTime;
    progress = elapsedLyric / (float)l.durationMs;

    if (currentLyricIdx != lastLyricIdx) {
      if (currentLyricIdx <= 1)                          currentBg = BG_TUNNEL_EQ;
      else if (currentLyricIdx >= 2 && currentLyricIdx <= 4)  currentBg = BG_OSCILLOSCOPE;
      else if (currentLyricIdx == 5)                          currentBg = BG_LIGHTNING;
      else if (currentLyricIdx >= 6 && currentLyricIdx <= 7)  currentBg = BG_MANDALA;
      else if (currentLyricIdx == 8)                          currentBg = BG_CUBE;
      else if (currentLyricIdx == 9)                          currentBg = BG_MATRIX;
      else if (currentLyricIdx >= 10 && currentLyricIdx <= 11) currentBg = BG_SYNTHWAVE;
      else if (currentLyricIdx >= 12 && currentLyricIdx <= 14) currentBg = BG_TUNNEL_EQ;
      else if (currentLyricIdx >= 15 && currentLyricIdx <= 16) currentBg = BG_CUBE;
      else if (currentLyricIdx >= 17 && currentLyricIdx <= 18) currentBg = BG_OSCILLOSCOPE;
      else if (currentLyricIdx >= 19 && currentLyricIdx <= 20) currentBg = BG_VORTEX;
      else if (currentLyricIdx == 21)                          currentBg = BG_LIGHTNING;
      else if (currentLyricIdx >= 22 && currentLyricIdx <= 23) currentBg = BG_BATS;
      else if (currentLyricIdx >= 24 && currentLyricIdx <= 25) currentBg = BG_VORTEX;
      else if (currentLyricIdx >= 26 && currentLyricIdx <= 27) currentBg = BG_MANDALA;
      else if (currentLyricIdx >= 28 && currentLyricIdx <= 29) currentBg = BG_CUBE;
      else if (currentLyricIdx >= 30 && currentLyricIdx <= 31) currentBg = BG_SYNTHWAVE;
      else if (currentLyricIdx == 32)                          currentBg = BG_LIGHTNING;
      else if (currentLyricIdx >= 33 && currentLyricIdx <= 34) currentBg = BG_OSCILLOSCOPE;
      else if (currentLyricIdx >= 35 && currentLyricIdx <= 36) currentBg = BG_BATS;
      else if (currentLyricIdx == 37)                          currentBg = BG_MATRIX;
      else if (currentLyricIdx == 38)                          currentBg = BG_TUNNEL_EQ;

      if (currentBg == BG_TUNNEL_EQ && (l.effect == EFFECT_POP || l.effect == EFFECT_INVERT || l.effect == EFFECT_ZOOM_IN || l.effect == EFFECT_SHAKE)) {
        wave.active = true;
        wave.radius = 5.0;
      }

      if (currentBg == BG_BATS && (l.effect == EFFECT_POP || l.effect == EFFECT_BOUNCE || l.effect == EFFECT_INVERT || l.effect == EFFECT_SHAKE)) {
        for (int i = 0; i < NUM_BATS; i++) {
          bats[i].x = SCREEN_WIDTH / 2 + random(-10, 10);
          bats[i].y = SCREEN_HEIGHT / 2 + random(-10, 10);
          float angle = random(0, 360) * PI / 180.0;
          float spd = random(20, 50) / 10.0;
          bats[i].vx = cos(angle) * spd;
          bats[i].vy = sin(angle) * spd;
        }
      }
      lastLyricIdx = currentLyricIdx;
    }
  } else {
    if (now < lyrics[0].calculatedStartTime) {
      currentBg = BG_TUNNEL_EQ;
    } else if (lastLyricIdx == 38) {
      if (now < lyrics[38].calculatedStartTime + lyrics[38].durationMs + 1500) {
        currentBg = BG_BATS;
      } else {
        currentBg = BG_MATRIX;
      }
    }
  }

  // --- RENDER BACKGROUND ---
  switch (currentBg) {
    case BG_BATS:         drawBats(currentEffect, lyricActive); break;
    case BG_MANDALA:      drawMandalaBackground(currentEffect, lyricActive); break;
    case BG_SYNTHWAVE:    drawSynthwaveBackground(currentEffect, lyricActive); break;
    case BG_TUNNEL_EQ:    drawTunnel(currentEffect, lyricActive); drawShockwave(); drawEQ(currentEffect, lyricActive); break;
    case BG_VORTEX:       drawVortex(lyricActive); break;
    case BG_MATRIX:       drawMatrixRain(currentEffect, lyricActive); break;
    case BG_OSCILLOSCOPE: drawOscilloscope(currentEffect, lyricActive); break;
    case BG_LIGHTNING:    drawLightning(currentEffect, lyricActive); break;
    case BG_CUBE:         drawCube(currentEffect, lyricActive); break;
  }

  // --- RENDER LYRICS ---
  if (currentLyricIdx != -1) {
    const Lyric& l = lyrics[currentLyricIdx];
    int textSize = l.size;
    int offsetX = 0;
    int offsetY = 0;

    char typeBuf[20];
    const char* displayWord = l.word;
    bool showCursor = false;

    if (l.effect == EFFECT_TYPEWRITER) {
      int totalChars = strlen(l.word);
      int charsToShow = 1 + (int)(progress * totalChars);
      if (charsToShow > totalChars) charsToShow = totalChars;
      strncpy(typeBuf, l.word, charsToShow);
      typeBuf[charsToShow] = '\0';
      displayWord = typeBuf;
      showCursor = (charsToShow < totalChars);
    }

    if (l.effect == EFFECT_POP) {
      if (progress < 0.15) textSize = l.size + 1;
    } else if (l.effect == EFFECT_BOUNCE) {
      float amp = 22.0 * exp(-5.0 * progress);
      offsetY = -(int)(amp * abs(cos(progress * PI * 4.0)));
    } else if (l.effect == EFFECT_SHAKE) {
      offsetX = random(-3, 4);
      offsetY = random(-3, 4);
    } else if (l.effect == EFFECT_INVERT) {
      invertScreen = true;
      if (random(10) > 5) { offsetX = random(-2, 3); offsetY = random(-2, 3); }
    } else if (l.effect == EFFECT_GLITCH) {
      if (random(10) > 6) {
        offsetX = random(-6, 6);
        display.fillRect(0, random(SCREEN_HEIGHT), SCREEN_WIDTH, random(2, 8), SH110X_WHITE);
      }
      if (random(10) > 8) invertScreen = true;
    } else if (l.effect == EFFECT_ZOOM_IN) {
      if (progress < 0.05) textSize = l.size > 1 ? l.size - 1 : 1;
      else if (progress < 0.1) textSize = l.size;
      else textSize = l.size + 1;
      if (progress > 0.4) {
        offsetX = random(-4, 5);
        offsetY = random(-4, 5);
      }
      if (progress > 0.3 && random(10) > 7) invertScreen = true;
    }

    display.setTextSize(textSize);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(displayWord, 0, 0, &x1, &y1, &w, &h);

    if (w > SCREEN_WIDTH) {
      textSize = 2;
      display.setTextSize(textSize);
      display.getTextBounds(displayWord, 0, 0, &x1, &y1, &w, &h);
    }

    int drawX = (SCREEN_WIDTH - w) / 2 + offsetX;
    int drawY = (SCREEN_HEIGHT - h) / 2 + offsetY;
    if (currentBg == BG_TUNNEL_EQ) drawY -= 5;

    display.fillRoundRect(drawX - 6, drawY - 5, w + 12, h + 10, 3, SH110X_BLACK);
    display.drawRoundRect(drawX - 6, drawY - 5, w + 12, h + 10, 3, SH110X_WHITE);

    if (l.effect == EFFECT_GLITCH && random(10) > 8) {
      display.setTextColor(SH110X_BLACK, SH110X_WHITE);
      display.fillRoundRect(drawX - 4, drawY - 3, w + 8, h + 6, 2, SH110X_WHITE);
    } else {
      display.setTextColor(SH110X_WHITE);
    }

    display.setCursor(drawX, drawY);
    display.print(displayWord);

    if (showCursor && ((millis() / 150) % 2 == 0)) {
      display.fillRect(drawX + w + 2, drawY, 2, h, SH110X_WHITE);
    }
  }

  display.invertDisplay(invertScreen);
  display.display();
}

// =========================================================================
// BACKGROUND FUNCTIONS
// =========================================================================
void drawBats(Effect eff, bool lyricActive) {
  float speedMult = lyricActive ? 1.5 : 0.8;
  if (eff == EFFECT_ZOOM_IN || eff == EFFECT_SHAKE) speedMult = 3.0;

  for (int i = 0; i < NUM_BATS; i++) {
    bats[i].x += bats[i].vx * speedMult;
    bats[i].y += bats[i].vy * speedMult;
    bats[i].framePhase += 0.3 * speedMult;

    if (bats[i].x < -10) bats[i].x = SCREEN_WIDTH + 10;
    if (bats[i].x > SCREEN_WIDTH + 10) bats[i].x = -10;
    if (bats[i].y < -10) bats[i].y = SCREEN_HEIGHT + 10;
    if (bats[i].y > SCREEN_HEIGHT + 10) bats[i].y = -10;

    int bx = (int)bats[i].x;
    int by = (int)bats[i].y;
    int wingSpan = 4 * bats[i].scale;
    int flap = sin(bats[i].framePhase) * 3 * bats[i].scale;

    display.drawPixel(bx, by, SH110X_WHITE);
    display.drawLine(bx, by, bx - wingSpan, by - flap, SH110X_WHITE);
    display.drawLine(bx, by, bx + wingSpan, by - flap, SH110X_WHITE);
  }
}

void drawRotatedTriangle(int cx, int cy, float radius, float a, bool fill) {
  int x1 = cx + cos(a) * radius;           int y1 = cy + sin(a) * radius;
  int x2 = cx + cos(a + 2.0944) * radius;  int y2 = cy + sin(a + 2.0944) * radius;
  int x3 = cx + cos(a + 4.1888) * radius;  int y3 = cy + sin(a + 4.1888) * radius;
  if (fill) display.fillTriangle(x1, y1, x2, y2, x3, y3, SH110X_WHITE);
  else display.drawTriangle(x1, y1, x2, y2, x3, y3, SH110X_WHITE);
}

void drawRotatedSquare(int cx, int cy, float radius, float a) {
  int sx1 = cx + cos(a) * radius;          int sy1 = cy + sin(a) * radius;
  int sx2 = cx + cos(a + 1.5708) * radius; int sy2 = cy + sin(a + 1.5708) * radius;
  int sx3 = cx + cos(a + 3.1416) * radius; int sy3 = cy + sin(a + 3.1416) * radius;
  int sx4 = cx + cos(a + 4.7124) * radius; int sy4 = cy + sin(a + 4.7124) * radius;
  display.drawLine(sx1, sy1, sx2, sy2, SH110X_WHITE);
  display.drawLine(sx2, sy2, sx3, sy3, SH110X_WHITE);
  display.drawLine(sx3, sy3, sx4, sy4, SH110X_WHITE);
  display.drawLine(sx4, sy4, sx1, sy1, SH110X_WHITE);
}

void drawMandalaBackground(Effect eff, bool lyricActive) {
  int cx = SCREEN_WIDTH / 2, cy = SCREEN_HEIGHT / 2;
  float sm = (eff == EFFECT_ZOOM_IN) ? 8.0 : (eff == EFFECT_SHAKE || eff == EFFECT_INVERT) ? 4.0 : lyricActive ? 1.5 : 0.4;
  mAngle1 += 0.02 * sm; mAngle2 -= 0.03 * sm; mAngle3 += 0.05 * sm;
  if (mAngle1 > 2 * PI) mAngle1 -= 2 * PI;
  if (mAngle2 < -2 * PI) mAngle2 += 2 * PI;
  if (mAngle3 > 2 * PI) mAngle3 -= 2 * PI;

  float outerSz = (eff == EFFECT_POP || eff == EFFECT_BOUNCE) ? 40.0 : 35.0;
  drawRotatedTriangle(cx, cy, outerSz, mAngle1, false);
  drawRotatedTriangle(cx, cy, outerSz, mAngle1 + PI, false);
  drawRotatedSquare(cx, cy, 22.0, mAngle2);
  if (lyricActive) drawRotatedSquare(cx, cy, 22.0, mAngle2 + (PI / 4));
  drawRotatedTriangle(cx, cy, 12.0, mAngle3, false);
  drawRotatedTriangle(cx, cy, 12.0, mAngle3 + PI, false);

  expandDist += 0.5 * sm;
  if (expandDist > 60.0) expandDist = 0.0;
  for (int i = 0; i < 8; i++) {
    float a = (PI / 4) * i + (mAngle1 * 0.5);
    int x = cx + cos(a) * (expandDist + 10);
    int y = cy + sin(a) * (expandDist + 10);
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
      display.drawPixel(x, y, SH110X_WHITE);
      if (sm > 2.0) {
        int tx = cx + cos(a) * (expandDist + 8);
        int ty = cy + sin(a) * (expandDist + 8);
        display.drawPixel(tx, ty, SH110X_WHITE);
      }
    }
  }
}

void drawSynthwaveBackground(Effect eff, bool lyricActive) {
  int cy = 34, cx = SCREEN_WIDTH / 2;
  float speed = (eff == EFFECT_ZOOM_IN || eff == EFFECT_SHAKE) ? 0.15 : (!lyricActive) ? 0.02 : 0.04;
  gridOffset += speed;
  if (gridOffset >= 1.0) gridOffset -= 1.0;

  for (int i = 0; i < 15; i++) if (random(10) > 2) display.drawPixel(swStarsX[i], swStarsY[i], SH110X_WHITE);

  display.fillCircle(cx, cy, 22, SH110X_WHITE);
  display.fillRect(0, cy, SCREEN_WIDTH, SCREEN_HEIGHT - cy, SH110X_BLACK);
  display.drawFastHLine(cx - 22, cy - 3, 44, SH110X_BLACK);
  display.fillRect(cx - 22, cy - 8, 44, 2, SH110X_BLACK);
  display.fillRect(cx - 22, cy - 15, 44, 3, SH110X_BLACK);

  display.drawLine(0, cy, 18, cy - 8, SH110X_WHITE);
  display.drawLine(18, cy - 8, 30, cy - 2, SH110X_WHITE);
  display.drawLine(30, cy - 2, 42, cy, SH110X_WHITE);
  display.drawLine(SCREEN_WIDTH, cy, 110, cy - 10, SH110X_WHITE);
  display.drawLine(110, cy - 10, 95, cy - 3, SH110X_WHITE);
  display.drawLine(95, cy - 3, 85, cy, SH110X_WHITE);
  display.drawFastHLine(0, cy, SCREEN_WIDTH, SH110X_WHITE);

  for (float i = 0; i < 8; i++) {
    float z = i - gridOffset;
    if (z < 0.1) continue;
    int y = cy + (35.0 / z);
    if (y <= SCREEN_HEIGHT && y > cy) display.drawFastHLine(0, y, SCREEN_WIDTH, SH110X_WHITE);
  }

  for (float i = -10; i <= 10; i++) {
    int xTop = cx + (i * 8);
    int xBot = cx + (i * 45);
    display.drawLine(xTop, cy, xBot, SCREEN_HEIGHT, SH110X_WHITE);
  }
}

void drawTunnel(Effect eff, bool lyricActive) {
  float speed = (eff == EFFECT_ZOOM_IN) ? 8.0 : (eff == EFFECT_SHAKE || eff == EFFECT_GLITCH) ? 4.0 : (!lyricActive) ? 0.5 : 1.0;
  int cx = SCREEN_WIDTH / 2, cy = SCREEN_HEIGHT / 2;
  if (eff == EFFECT_SHAKE || eff == EFFECT_ZOOM_IN) { cx += random(-3, 4); cy += random(-3, 4); }

  for (float a = 0; a < 2 * PI; a += PI / 4) {
    display.drawLine(cx + cos(a) * 5, cy + sin(a) * 5, cx + cos(a) * 120, cy + sin(a) * 120, SH110X_WHITE);
  }

  for (int i = 0; i < NUM_CIRCLES; i++) {
    circleRadii[i] += speed;
    if (circleRadii[i] > 120) circleRadii[i] = 1;
    if (circleRadii[i] > 2) display.drawCircle(cx, cy, circleRadii[i], SH110X_WHITE);
  }
}

void drawEQ(Effect eff, bool lyricActive) {
  int barW = SCREEN_WIDTH / NUM_BARS;
  for (int i = 0; i < NUM_BARS; i++) {
    if (random(10) > 4) {
      if (lyricActive) {
        if (eff == EFFECT_ZOOM_IN) targetHeights[i] = random(15, 35);
        else if (eff == EFFECT_SHAKE) targetHeights[i] = random(10, 25);
        else targetHeights[i] = random(5, 15);
      } else targetHeights[i] = random(1, 6);
    }
    if (barHeights[i] < targetHeights[i]) barHeights[i] += 4;
    else if (barHeights[i] > targetHeights[i]) barHeights[i] -= 3;
    if (barHeights[i] < 1) barHeights[i] = 1;
    display.fillRect(i * barW + 1, SCREEN_HEIGHT - barHeights[i], barW - 1, barHeights[i], SH110X_WHITE);
  }
}

void drawShockwave() {
  if (wave.active) {
    display.drawCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, wave.radius, SH110X_WHITE);
    display.drawCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, wave.radius - 1, SH110X_WHITE);
    wave.radius += 8.0;
    if (wave.radius > 140) wave.active = false;
  }
}

void drawVortex(bool lyricActive) {
  vortexTime += lyricActive ? 0.12 : 0.06;
  int cx = SCREEN_WIDTH / 2, cy = SCREEN_HEIGHT / 2;

  for (int i = 0; i < 8; i++) {
    int prevX = -1, prevY = -1;
    for (int r = 2; r <= 80; r += 6) {
      float twist = r * 0.04;
      float a = (i / 8.0) * PI * 2.0 + twist + vortexTime;
      int x = cx + cos(a) * r, y = cy + sin(a) * r;
      if (prevX != -1) display.drawLine(prevX, prevY, x, y, SH110X_WHITE);
      prevX = x; prevY = y;
    }
  }

  for (int baseR = 10; baseR <= 80; baseR += 16) {
    int r = baseR + ((int)(vortexTime * 10) % 16);
    float twist = r * 0.04;
    int prevX = -1, prevY = -1, firstX = -1, firstY = -1;
    for (float a = 0; a <= PI * 2.0; a += 0.4) {
      float ang = a + twist + vortexTime;
      int x = cx + cos(ang) * r, y = cy + sin(ang) * r;
      if (prevX != -1) display.drawLine(prevX, prevY, x, y, SH110X_WHITE);
      else { firstX = x; firstY = y; }
      prevX = x; prevY = y;
    }
    display.drawLine(prevX, prevY, firstX, firstY, SH110X_WHITE);
  }
}

void drawMatrixRain(Effect eff, bool lyricActive) {
  float sm = lyricActive ? 2.0 : 1.0;
  if (eff == EFFECT_ZOOM_IN || eff == EFFECT_SHAKE) sm = 4.0;
  else if (eff == EFFECT_TYPEWRITER || eff == EFFECT_GLITCH) sm = 2.5;

  for (int i = 0; i < MATRIX_COLS; i++) {
    matrixCols[i].y += matrixCols[i].speed * sm;
    if (matrixCols[i].y > SCREEN_HEIGHT + matrixCols[i].trailLen * 3) {
      matrixCols[i].y = random(-25, -5);
      matrixCols[i].speed = random(10, 30) / 10.0;
      matrixCols[i].trailLen = random(8, 22);
    }

    int headY = (int)matrixCols[i].y;
    int x = matrixCols[i].x;

    if (headY >= 0 && headY < SCREEN_HEIGHT) {
      display.fillRect(x, headY, 3, 3, SH110X_WHITE);
    }

    for (int j = 1; j < matrixCols[i].trailLen; j++) {
      int ty = headY - j * 3;
      if (ty >= 0 && ty < SCREEN_HEIGHT) {
        if (j < matrixCols[i].trailLen / 3) {
          display.fillRect(x, ty, 2, 2, SH110X_WHITE);
        } else {
          display.drawPixel(x + (j % 2), ty, SH110X_WHITE);
        }
      }
    }
  }
}

void drawOscilloscope(Effect eff, bool lyricActive) {
  int cy = SCREEN_HEIGHT / 2;

  for (int gx = 0; gx < SCREEN_WIDTH; gx += 16) {
    for (int gy = 0; gy < SCREEN_HEIGHT; gy += 16) {
      display.drawPixel(gx, gy, SH110X_WHITE);
    }
  }

  for (int x = 0; x < SCREEN_WIDTH; x += 4) display.drawPixel(x, cy, SH110X_WHITE);

  float amplitude = lyricActive ? 18.0 : 8.0;
  float frequency = lyricActive ? 2.5 : 1.5;
  if (eff == EFFECT_ZOOM_IN) { amplitude = 28.0; frequency = 5.0; }
  else if (eff == EFFECT_SHAKE) { amplitude = 22.0; frequency = 3.5; }
  else if (eff == EFFECT_BOUNCE || eff == EFFECT_TYPEWRITER) { amplitude = 15.0; frequency = 2.0; }

  oscPhase += lyricActive ? 0.15 : 0.05;

  int prevY1 = cy;
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    float t = (float)x / SCREEN_WIDTH;
    float y = cy + amplitude * sin(t * frequency * PI * 2.0 + oscPhase);
    y += (amplitude * 0.3) * sin(t * frequency * 2.0 * PI * 2.0 + oscPhase * 1.7);
    if (eff == EFFECT_SHAKE || eff == EFFECT_ZOOM_IN) y += random(-2, 3);
    int iy = constrain((int)y, 0, SCREEN_HEIGHT - 1);
    if (x > 0) display.drawLine(x - 1, prevY1, x, iy, SH110X_WHITE);
    prevY1 = iy;
  }

  int prevY2 = cy;
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    float t = (float)x / SCREEN_WIDTH;
    float y = cy + (amplitude * 0.5) * sin(t * frequency * PI * 2.0 - oscPhase * 0.8 + PI / 3.0);
    int iy = constrain((int)y, 0, SCREEN_HEIGHT - 1);
    if (x > 0) display.drawLine(x - 1, prevY2, x, iy, SH110X_WHITE);
    prevY2 = iy;
  }
}

void generateBolt(int idx) {
  bolts[idx].numPts = MAX_BOLT_PTS;
  bolts[idx].bx[0] = SCREEN_WIDTH / 2 + random(-25, 26);
  bolts[idx].by[0] = 0;
  for (int i = 1; i < MAX_BOLT_PTS; i++) {
    bolts[idx].by[i] = bolts[idx].by[i - 1] + (SCREEN_HEIGHT / (MAX_BOLT_PTS - 1));
    bolts[idx].bx[i] = bolts[idx].bx[i - 1] + random(-14, 15);
    if (bolts[idx].bx[i] < 3) bolts[idx].bx[i] = 3;
    if (bolts[idx].bx[i] > SCREEN_WIDTH - 3) bolts[idx].bx[i] = SCREEN_WIDTH - 3;
  }
  bolts[idx].framesLeft = random(3, 8);
  bolts[idx].active = true;
}

void drawLightning(Effect eff, bool lyricActive) {
  int spawnChance = lyricActive ? 35 : 8;
  if (eff == EFFECT_SHAKE || eff == EFFECT_INVERT || eff == EFFECT_ZOOM_IN) spawnChance = 65;
  if (random(100) < spawnChance) {
    for (int i = 0; i < MAX_BOLTS; i++) {
      if (!bolts[i].active) { generateBolt(i); break; }
    }
  }

  for (int i = 0; i < MAX_BOLTS; i++) {
    if (bolts[i].active) {
      for (int j = 0; j < bolts[i].numPts - 1; j++) {
        display.drawLine(bolts[i].bx[j], bolts[i].by[j], bolts[i].bx[j + 1], bolts[i].by[j + 1], SH110X_WHITE);
        display.drawLine(bolts[i].bx[j] + 1, bolts[i].by[j], bolts[i].bx[j + 1] + 1, bolts[i].by[j + 1], SH110X_WHITE);
      }

      if (bolts[i].numPts > 4) {
        int bi = random(2, bolts[i].numPts - 2);
        int brX = bolts[i].bx[bi] + random(-22, 23);
        int brY = bolts[i].by[bi] + random(5, 15);
        display.drawLine(bolts[i].bx[bi], bolts[i].by[bi], brX, brY, SH110X_WHITE);

        int b2i = random(1, bolts[i].numPts - 1);
        int br2X = bolts[i].bx[b2i] + random(-18, 19);
        int br2Y = bolts[i].by[b2i] + random(3, 12);
        display.drawLine(bolts[i].bx[b2i], bolts[i].by[b2i], br2X, br2Y, SH110X_WHITE);
      }

      bolts[i].framesLeft--;
      if (bolts[i].framesLeft <= 0) bolts[i].active = false;
    }
  }

  int sparkles = lyricActive ? 8 : 3;
  for (int i = 0; i < sparkles; i++) {
    display.drawPixel(random(SCREEN_WIDTH), random(SCREEN_HEIGHT), SH110X_WHITE);
  }
}

void drawCube(Effect eff, bool lyricActive) {
  float sm = lyricActive ? 1.5 : 0.5;
  if (eff == EFFECT_ZOOM_IN) sm = 5.0;
  else if (eff == EFFECT_SHAKE || eff == EFFECT_INVERT) sm = 3.0;
  else if (eff == EFFECT_BOUNCE || eff == EFFECT_POP) sm = 2.0;

  cubeAX += 0.02 * sm;
  cubeAY += 0.03 * sm;
  cubeAZ += 0.01 * sm;
  if (cubeAX > 2 * PI) cubeAX -= 2 * PI;
  if (cubeAY > 2 * PI) cubeAY -= 2 * PI;
  if (cubeAZ > 2 * PI) cubeAZ -= 2 * PI;

  float sz = (eff == EFFECT_POP || eff == EFFECT_BOUNCE) ? 22.0 : 18.0;
  int cx = SCREEN_WIDTH / 2, cy = SCREEN_HEIGHT / 2;

  float verts[8][3] = {
    {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
    {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
  };

  int proj[8][2];
  for (int i = 0; i < 8; i++) {
    float x = verts[i][0] * sz;
    float y = verts[i][1] * sz;
    float z = verts[i][2] * sz;

    float y1 = y * cos(cubeAX) - z * sin(cubeAX);
    float z1 = y * sin(cubeAX) + z * cos(cubeAX);
    y = y1; z = z1;

    float x1 = x * cos(cubeAY) + z * sin(cubeAY);
    z1 = -x * sin(cubeAY) + z * cos(cubeAY);
    x = x1; z = z1;

    x1 = x * cos(cubeAZ) - y * sin(cubeAZ);
    y1 = x * sin(cubeAZ) + y * cos(cubeAZ);
    x = x1; y = y1;

    float fov = 60.0;
    float scale = fov / (fov + z);
    proj[i][0] = cx + (int)(x * scale);
    proj[i][1] = cy + (int)(y * scale);
  }

  int edges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
  };

  for (int i = 0; i < 12; i++) {
    display.drawLine(proj[edges[i][0]][0], proj[edges[i][0]][1],
                     proj[edges[i][1]][0], proj[edges[i][1]][1], SH110X_WHITE);
  }

  for (int i = 0; i < 8; i++) {
    display.fillCircle(proj[i][0], proj[i][1], 2, SH110X_WHITE);
  }
}
