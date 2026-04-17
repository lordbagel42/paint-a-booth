/*
 * Project myProject
 * Author: Your Name
 * Date:
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

// Include Particle Device OS APIs
#include "Particle.h"
#include "neopixel.h"

// Let Device OS manage the connection to the Particle Cloud
SYSTEM_MODE(AUTOMATIC);

// Run the application and system concurrently in separate threads
SYSTEM_THREAD(ENABLED);

// Show system, cloud connectivity, and application logs over USB
// View logs with CLI using 'particle serial monitor --follow'
SerialLogHandler logHandler(LOG_LEVEL_INFO);

// --- LED matrix configuration ---
#define PIXEL_PIN D2
#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8
#define PIXEL_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)
#define PIXEL_TYPE WS2812B
// 1 = parse from MOCK_GRID_PAYLOAD, 0 = parse from live HTTP response body (TODO path below)
#define USE_MOCK_GRID 1

// Runtime defaults before ledger sync
const bool DEFAULT_MIRROR_X = false;
const bool DEFAULT_MIRROR_Y = true;
const uint8_t DEFAULT_BRIGHTNESS = 64;
const uint8_t MIN_BRIGHTNESS = 0;
const uint8_t MAX_BRIGHTNESS = 255;
const char *DEFAULT_GRID_ENDPOINT_URL = "https://rmrrf.raygen.dev/api/grid";
const uint32_t DEFAULT_GRID_POLL_INTERVAL_MS = 1000;
const uint32_t MIN_GRID_POLL_INTERVAL_MS = 100;
const uint32_t MAX_GRID_POLL_INTERVAL_MS = 60000;

const size_t GRID_ENDPOINT_URL_MAX_LEN = 128;

const uint8_t RGB_TRIPLET_LEN = 11; // RRR.GGG.BBB
const uint8_t PIXEL_SLOT_LEN = 12;  // RRR.GGG.BBB,

// Mock payload for now (replace with HTTP response body later).
// Order is visual row-major: x=0..7, y=0..7.
const char *MOCK_GRID_PAYLOAD =
    "255.255.255,255.255.255,255.255.255,255.128.000,255.128.000,255.255.255,255.255.255,128.000.128,"
    "255.255.255,255.255.255,255.255.255,255.000.000,255.000.000,255.255.255,255.255.255,255.255.255,"
    "255.255.255,255.255.255,255.000.000,255.000.000,255.000.000,255.000.000,255.255.255,255.255.255,"
    "255.255.255,255.255.255,255.000.000,255.255.255,255.255.255,255.000.000,255.255.255,255.255.255,"
    "255.255.255,255.255.255,255.000.000,255.255.255,255.255.255,255.000.000,255.255.255,255.255.255,"
    "255.255.255,255.255.255,255.000.000,255.000.000,255.000.000,255.000.000,255.255.255,255.255.255,"
    "255.255.255,255.255.255,255.255.255,255.255.255,255.255.255,255.255.255,255.255.255,255.255.255,"
    "000.000.255,255.255.255,255.255.255,255.255.255,255.255.255,255.255.255,255.255.255,000.255.000";

uint32_t Wheel(byte WheelPos);
uint16_t xyToIndex(uint8_t x, uint8_t y);
void drawTargetPattern();
void pollGridStateOncePerSecond();
bool applyGridPayload(const char *payload);
int parse3Digits(const char *digits);
void refreshMirrorSettingsFromLedger();
void applyBrightness(uint8_t brightnessValue);

Ledger ledSettings;
bool mirrorXEnabled = DEFAULT_MIRROR_X;
bool mirrorYEnabled = DEFAULT_MIRROR_Y;
int mirrorXState = DEFAULT_MIRROR_X ? 1 : 0;
int mirrorYState = DEFAULT_MIRROR_Y ? 1 : 0;
char gridEndpointUrl[GRID_ENDPOINT_URL_MAX_LEN] = "https://rmrrf.raygen.dev/api/grid";
uint32_t gridPollIntervalMs = DEFAULT_GRID_POLL_INTERVAL_MS;
int gridPollIntervalState = (int)DEFAULT_GRID_POLL_INTERVAL_MS;
uint8_t brightnessValue = DEFAULT_BRIGHTNESS;
int brightnessState = DEFAULT_BRIGHTNESS;

// Adafruit_NeoPixel instance (Particle neopixel library exposes Adafruit_NeoPixel)
Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

// setup() runs once, when the device is first turned on
void setup()
{
  // Initialize logging and hardware here
  // Initialize NeoPixel strip
  strip.begin();
  applyBrightness(brightnessValue);
  // start with all LEDs off
  for (int i = 0; i < PIXEL_COUNT; ++i)
  {
    strip.setPixelColor(i, 0); // off
  }
  strip.show();

  // Optional startup indicator until the first poll lands.
  drawTargetPattern();

  // Cloud-to-device configuration ledger (keys: MIRROR_X, MIRROR_Y as 0/1)
  ledSettings = Particle.ledger("led-settings");

  // Expose applied mirror orientation states as cloud variables (0 or 1)
  Particle.variable("mirrorXState", mirrorXState);
  Particle.variable("mirrorYState", mirrorYState);
  Particle.variable("pollMsState", gridPollIntervalState);
  Particle.variable("brightnessState", brightnessState);

  // Ensure defaults are copied from constants into runtime fields.
  strncpy(gridEndpointUrl, DEFAULT_GRID_ENDPOINT_URL, sizeof(gridEndpointUrl) - 1);
  gridEndpointUrl[sizeof(gridEndpointUrl) - 1] = '\0';
}

// loop() runs over and over again, as quickly as it can execute.
void loop()
{
  pollGridStateOncePerSecond();
  delay(5);
}

void pollGridStateOncePerSecond()
{
  static uint32_t lastPollMs = 0;
  const uint32_t now = millis();

  if (now - lastPollMs < gridPollIntervalMs)
  {
    return;
  }
  lastPollMs = now;

  // Refresh orientation flags from cloud-to-device ledger.
  refreshMirrorSettingsFromLedger();

  const char *payload = NULL;
#if USE_MOCK_GRID
  payload = MOCK_GRID_PAYLOAD;
#else
  // TODO: perform HTTP GET to gridEndpointUrl and set payload to the response body.
  // Example target response format: RRR.GGG.BBB,RRR.GGG.BBB,...
  payload = NULL;
#endif

  const bool parsedOk = applyGridPayload(payload);

  if (!parsedOk)
  {
#if USE_MOCK_GRID
    Log.warn("Grid payload parse failed (source: mock for %s)", gridEndpointUrl);
#else
    Log.warn("Grid payload parse failed (source: live HTTP for %s)", gridEndpointUrl);
#endif
  }
}

void refreshMirrorSettingsFromLedger()
{
  LedgerData settings = ledSettings.get();

  bool newMirrorX = mirrorXEnabled;
  bool newMirrorY = mirrorYEnabled;

  if (settings.has("MIRROR_X"))
  {
    newMirrorX = (settings.get("MIRROR_X").toInt() != 0);
  }

  if (settings.has("MIRROR_Y"))
  {
    newMirrorY = (settings.get("MIRROR_Y").toInt() != 0);
  }

  if (settings.has("GRID_ENDPOINT_URL"))
  {
    String endpointValue = settings.get("GRID_ENDPOINT_URL").toString();
    if (endpointValue.length() > 0)
    {
      strncpy(gridEndpointUrl, endpointValue.c_str(), sizeof(gridEndpointUrl) - 1);
      gridEndpointUrl[sizeof(gridEndpointUrl) - 1] = '\0';
    }
  }

  if (settings.has("GRID_POLL_INTERVAL_MS"))
  {
    int newPoll = settings.get("GRID_POLL_INTERVAL_MS").toInt();
    if (newPoll >= (int)MIN_GRID_POLL_INTERVAL_MS && newPoll <= (int)MAX_GRID_POLL_INTERVAL_MS)
    {
      gridPollIntervalMs = (uint32_t)newPoll;
      gridPollIntervalState = newPoll;
    }
  }

  if (settings.has("BRIGHTNESS"))
  {
    int newBrightness = settings.get("BRIGHTNESS").toInt();
    if (newBrightness >= (int)MIN_BRIGHTNESS && newBrightness <= (int)MAX_BRIGHTNESS)
    {
      if (brightnessValue != (uint8_t)newBrightness)
      {
        applyBrightness((uint8_t)newBrightness);
        Log.info("Brightness updated from ledger: %d", brightnessState);
      }
    }
  }

  if (newMirrorX != mirrorXEnabled || newMirrorY != mirrorYEnabled)
  {
    mirrorXEnabled = newMirrorX;
    mirrorYEnabled = newMirrorY;
    mirrorXState = mirrorXEnabled ? 1 : 0;
    mirrorYState = mirrorYEnabled ? 1 : 0;
    Log.info("Mirror updated from ledger: X=%d Y=%d", mirrorXEnabled ? 1 : 0, mirrorYEnabled ? 1 : 0);
  }
}

void applyBrightness(uint8_t newBrightnessValue)
{
  if (newBrightnessValue > MAX_BRIGHTNESS)
  {
    newBrightnessValue = MAX_BRIGHTNESS;
  }

  brightnessValue = newBrightnessValue;
  brightnessState = (int)newBrightnessValue;

  // Brightness is now directly 0-255.
  strip.setBrightness(newBrightnessValue);
}

bool applyGridPayload(const char *payload)
{
  if (payload == NULL || payload[0] == '\0')
  {
    return false;
  }

  // Default to black if payload has fewer than PIXEL_COUNT entries.
  for (int i = 0; i < PIXEL_COUNT; ++i)
  {
    strip.setPixelColor(i, 0);
  }

  uint16_t visualPixel = 0;
  uint16_t index = 0;

  while (visualPixel < PIXEL_COUNT && payload[index] != '\0')
  {
    // Validate fixed format: RRR.GGG.BBB
    if (payload[index + 3] == '\0' || payload[index + 7] == '\0' || payload[index + 10] == '\0')
    {
      return false;
    }
    if (payload[index + 3] != '.' || payload[index + 7] != '.')
    {
      return false;
    }

    const int r = parse3Digits(&payload[index]);
    const int g = parse3Digits(&payload[index + 4]);
    const int b = parse3Digits(&payload[index + 8]);
    if (r < 0 || g < 0 || b < 0)
    {
      return false;
    }

    const uint8_t x = visualPixel % MATRIX_WIDTH;
    const uint8_t y = visualPixel / MATRIX_WIDTH;
    strip.setPixelColor(xyToIndex(x, y), strip.Color((uint8_t)r, (uint8_t)g, (uint8_t)b));

    visualPixel++;

    // Advance to next slot. Slot is 12 bytes when comma is present.
    if (payload[index + RGB_TRIPLET_LEN] == ',')
    {
      index += PIXEL_SLOT_LEN;
    }
    else
    {
      index += RGB_TRIPLET_LEN;
      break;
    }
  }

  strip.show();
  return true;
}

int parse3Digits(const char *digits)
{
  if (digits == NULL)
  {
    return -1;
  }

  const char d0 = digits[0];
  const char d1 = digits[1];
  const char d2 = digits[2];

  if (d0 < '0' || d0 > '9' || d1 < '0' || d1 > '9' || d2 < '0' || d2 > '9')
  {
    return -1;
  }

  return (d0 - '0') * 100 + (d1 - '0') * 10 + (d2 - '0');
}

void drawTargetPattern()
{
  for (uint8_t y = 0; y < MATRIX_HEIGHT; ++y)
  {
    for (uint8_t x = 0; x < MATRIX_WIDTH; ++x)
    {
      // Ring index from center for even-sized matrix (center is a 2x2 block).
      int dx = min(abs((int)x - 3), abs((int)x - 4));
      int dy = min(abs((int)y - 3), abs((int)y - 4));
      int ring = max(dx, dy); // 0=center, then 1,2,3 outward

      uint32_t color = (ring % 2 == 0)
                           ? strip.Color(255, 0, 0)      // red
                           : strip.Color(255, 255, 255); // white

      strip.setPixelColor(xyToIndex(x, y), color);
    }
  }
  strip.show();
}

// Convert visual XY coordinates to physical LED index for a horizontal serpentine matrix.
// Assumes row 0 starts left->right, row 1 right->left, alternating per row.
uint16_t xyToIndex(uint8_t x, uint8_t y)
{
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT)
  {
    return 0;
  }

  if (mirrorXEnabled)
  {
    x = (MATRIX_WIDTH - 1) - x;
  }

  if (mirrorYEnabled)
  {
    y = (MATRIX_HEIGHT - 1) - y;
  }

  uint16_t rowStart = y * MATRIX_WIDTH;
  if (y & 0x01)
  {
    return rowStart + (MATRIX_WIDTH - 1 - x);
  }
  return rowStart + x;
}

// Standard 0-255 to RGB wheel (returns 32-bit color compatible with Adafruit_NeoPixel)
uint32_t Wheel(byte WheelPos)
{
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85)
  {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170)
  {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
