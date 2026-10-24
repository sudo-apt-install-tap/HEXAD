#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GFX.h>

// Settings Def

#define MCP_ADDR 0x27
#define MIDI_CH 1
#define PAD_BASE_NOTE 36
#define PAD_VELOCITY 100
#define ENC_CC_BASE 20
#define ENC_SW_CC_BASE 30
#define ENC_STEP 2
#define ENC_TRANSITIONS_PER_DETENT 4
#define ENC_INVERT false
#define DEBOUNCE_MS 5
#define TFT_ROTATION 1

// Pins

const uint8_t COL_PINS[4]={0, 1, 2, 3};
const uint8_t ROW_PINS[3]={4, 5, 6};
const uint8_t ENC_SW_PINS[6]={7, 10, 11, 12, 16, 17};

// MCP23017 Pins

const uint8_t ENC_A[6]={6, 7, 8, 9, 10, 11};
const uint8_t ENC_B[6]={0, 1, 2, 3, 4, 5};

#define PIN_SDA  8
#define PIN_SCL  9
#define TFT_SCK  18
#define TFT_MOSI 19
#define TFT_CS   14
#define TFT_DC   13
#define TFT_RST  15

// objects
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);
Adafruit_MCP23X17 mcp;
Adafruit_ST7735 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);

volatile uint16_t g_padMask   = 0;
volatile uint8_t  g_encSwMask = 0;
volatile uint8_t  g_encVal[6] = {64, 64, 64, 64, 64, 64};
volatile bool     g_mcpOk     = false;
volatile uint8_t  g_lastKind  = 0;
volatile uint8_t  g_lastNum   = 0;
volatile uint8_t  g_lastVal   = 0;
volatile uint32_t g_msgCount  = 0;

// MIDI helpers
static void noteMessage(uint8_t kind, uint8_t num, uint8_t val) {
  g_lastKind = kind; g_lastNum = num; g_lastVal = val;
  g_msgCount = g_msgCount + 1;
}

static void sendNote(uint8_t note, bool on) {
  if (TinyUSBDevice.mounted()) {
    if (on) MIDI.sendNoteOn(note, PAD_VELOCITY, MIDI_CH);
    else    MIDI.sendNoteOff(note, 0, MIDI_CH);
  }
  noteMessage(1, note, on ? PAD_VELOCITY : 0);
}

static void sendCC(uint8_t cc, uint8_t val) {
  if (TinyUSBDevice.mounted()) MIDI.sendControlChange(cc, val, MIDI_CH);
  noteMessage(2, cc, val);
}

// pad matrix
static void scanPads() {
  static uint16_t stable = 0;
  static uint32_t lastChange[12] = {0};

  uint16_t raw = 0;
  for (uint8_t c = 0; c < 4; c++) {
    digitalWrite(COL_PINS[c], HIGH);
    delayMicroseconds(10);
    for (uint8_t r = 0; r < 3; r++) {
      if (digitalRead(ROW_PINS[r])) raw |= (1u << (r * 4 + c));
    }
    digitalWrite(COL_PINS[c], LOW);
  }

  uint32_t now = millis();
  uint16_t diff = raw ^ stable;
  for (uint8_t i = 0; i < 12; i++) {
    if (!(diff & (1u << i))) continue;
    if (now - lastChange[i] < DEBOUNCE_MS) continue;
    lastChange[i] = now;
    stable ^= (1u << i);
    uint8_t r = i / 4, c = i % 4;
    uint8_t note = PAD_BASE_NOTE + (2 - r) * 4 + c;
    sendNote(note, stable & (1u << i));
  }
  g_padMask = stable;
}

// encoder buttons
static void scanEncSwitches() {
  static uint8_t stable = 0;
  static uint32_t lastChange[6] = {0};
  uint32_t now = millis();
  for (uint8_t n = 0; n < 6; n++) {
    bool pressed = (digitalRead(ENC_SW_PINS[n]) == LOW);
    bool was = stable & (1u << n);
    if (pressed == was) continue;
    if (now - lastChange[n] < DEBOUNCE_MS * 2) continue;
    lastChange[n] = now;
    stable ^= (1u << n);
    sendCC(ENC_SW_CC_BASE + n, pressed ? 127 : 0);
  }
  g_encSwMask = stable;
}

// encoders (MCP23017)

static const int8_t QDEC[16] = {0, -1, 1, 0,  1, 0, 0, -1,  -1, 0, 0, 1,  0, 1, -1, 0};
static uint8_t encState[6];
static int8_t  encAccum[6];

static void encoderStep(uint8_t n, int8_t dir) {
  int v = (int)g_encVal[n] + dir * ENC_STEP;
  if (v < 0) v = 0;
  if (v > 127) v = 127;
  if (v == g_encVal[n]) return;
  g_encVal[n] = v;
  sendCC(ENC_CC_BASE + n, v);
}

static void pollEncoders() {
  if (!g_mcpOk) return;
  uint16_t v = mcp.readGPIOAB();
  for (uint8_t n = 0; n < 6; n++) {
    uint8_t a = (v >> ENC_A[n]) & 1;
    uint8_t b = (v >> ENC_B[n]) & 1;
    uint8_t ns = (a << 1) | b;
    int8_t d = QDEC[(encState[n] << 2) | ns];
    encState[n] = ns;
    if (!d) continue;
    encAccum[n] += ENC_INVERT ? -d : d;
    if (encAccum[n] >= ENC_TRANSITIONS_PER_DETENT)       { encAccum[n] = 0; encoderStep(n, +1); }
    else if (encAccum[n] <= -ENC_TRANSITIONS_PER_DETENT) { encAccum[n] = 0; encoderStep(n, -1); }
  }
}

// core 0
void setup() {
  TinyUSBDevice.setManufacturerDescriptor("PicoPad");
  TinyUSBDevice.setProductDescriptor("PicoPad MIDI");
  usb_midi.setStringDescriptor("PicoPad MIDI");
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.turnThruOff();

  for (uint8_t c = 0; c < 4; c++) { pinMode(COL_PINS[c], OUTPUT); digitalWrite(COL_PINS[c], LOW); }
  for (uint8_t r = 0; r < 3; r++) pinMode(ROW_PINS[r], INPUT_PULLDOWN);
  for (uint8_t n = 0; n < 6; n++) pinMode(ENC_SW_PINS[n], INPUT_PULLUP);

  Wire.setSDA(PIN_SDA);
  Wire.setSCL(PIN_SCL);
  Wire.begin();
  Wire.setClock(400000);

  if (mcp.begin_I2C(MCP_ADDR, &Wire)) {
    for (uint8_t i = 0; i < 16; i++) mcp.pinMode(i, INPUT_PULLUP);  // also parks the spare GPB4-7 pins
    uint16_t v = mcp.readGPIOAB();
    for (uint8_t n = 0; n < 6; n++) {
      encState[n] = (((v >> ENC_A[n]) & 1) << 1) | ((v >> ENC_B[n]) & 1);
      encAccum[n] = 0;
    }
    g_mcpOk = true;
  }
}

void loop() {
  static uint32_t lastScan = 0;
  uint32_t now = micros();
  if (now - lastScan >= 1000) {          // 1 kHz
    lastScan = now;
    scanPads();
    scanEncSwitches();
    pollEncoders();
  }
}

// display
static const uint16_t COL_BG   = ST77XX_BLACK;
static const uint16_t COL_IDLE = 0x2104;   // dark grey
static uint16_t colHit, colBar;

static void drawPad(uint8_t i, bool on) {
  uint8_t r = i / 4, c = i % 4;
  tft.fillRect(4 + c * 24, 4 + r * 24, 22, 22, on ? colHit : COL_IDLE);
}

static void drawEnc(uint8_t n, uint8_t val, bool pressed) {
  int x = 4 + n * 26, y = 88;
  tft.drawRect(x, y, 22, 36, pressed ? ST77XX_YELLOW : ST77XX_WHITE);
  tft.fillRect(x + 1, y + 1, 20, 34, COL_BG);
  int h = (val * 34) / 127;
  tft.fillRect(x + 1, y + 35 - h, 20, h, colBar);
}

void setup1() {
  SPI.setSCK(TFT_SCK);
  SPI.setTX(TFT_MOSI);
  SPI.setRX(20);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(TFT_ROTATION);
  colHit = tft.color565(0, 220, 90);
  colBar = tft.color565(0, 160, 255);
  tft.fillScreen(COL_BG);
  for (uint8_t i = 0; i < 12; i++) drawPad(i, false);
  for (uint8_t n = 0; n < 6; n++) drawEnc(n, 64, false);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, COL_BG);
  tft.setCursor(108, 4);
  tft.print("PICOPAD");
}

void loop1() {
  static uint16_t shownPads = 0;
  static uint8_t  shownEnc[6] = {64, 64, 64, 64, 64, 64};
  static uint8_t  shownSw = 0;
  static uint32_t shownMsg = 0;
  static bool     errShown = false;

  if (!g_mcpOk && !errShown) {
    errShown = true;
    tft.setTextColor(ST77XX_RED, COL_BG);
    tft.setCursor(104, 70);
    tft.print("NO I2C");
    tft.setCursor(104, 80);
    tft.print("EXPANDER");
    tft.setTextColor(ST77XX_WHITE, COL_BG);
  }

  uint16_t pads = g_padMask;
  uint16_t pd = pads ^ shownPads;
  for (uint8_t i = 0; i < 12; i++) {
    if (pd & (1u << i)) drawPad(i, pads & (1u << i));
  }
  shownPads = pads;

  uint8_t sw = g_encSwMask;
  for (uint8_t n = 0; n < 6; n++) {
    uint8_t v = g_encVal[n];
    bool swChanged = ((sw ^ shownSw) >> n) & 1;
    if (v != shownEnc[n] || swChanged) {
      drawEnc(n, v, sw & (1u << n));
      shownEnc[n] = v;
    }
  }
  shownSw = sw;

  uint32_t mc = g_msgCount;
  if (mc != shownMsg) {
    shownMsg = mc;
    char buf[16];
    tft.setCursor(108, 24);
    tft.print(g_lastKind == 1 ? "NOTE" : "CC  ");
    snprintf(buf, sizeof(buf), "%3u   ", (unsigned)g_lastNum);
    tft.setCursor(108, 36);
    tft.print(buf);
    snprintf(buf, sizeof(buf), "v%3u  ", (unsigned)g_lastVal);
    tft.setCursor(108, 48);
    tft.print(buf);
  }

  delay(20);
}