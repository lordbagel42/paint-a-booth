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
#define USE_MOCK_GRID 0

// Runtime defaults before ledger sync
const bool DEFAULT_MIRROR_X = false;
const bool DEFAULT_MIRROR_Y = true;
// Serpentine wiring direction for visual row 0 (top row before mirror transforms).
// false = right-to-left first row, true = left-to-right first row.
const bool SERPENTINE_ROW0_LEFT_TO_RIGHT = true;
// Set to false for progressive row wiring (each row left->right), true for zig-zag/serpentine.
const bool MATRIX_IS_SERPENTINE = false;
const uint8_t DEFAULT_BRIGHTNESS = 64;
const uint8_t MIN_BRIGHTNESS = 0;
const uint8_t MAX_BRIGHTNESS = 255;
const char *DEFAULT_GRID_ENDPOINT_URL = "https://rmrrf.raygen.dev/api/grid";
const uint32_t DEFAULT_GRID_POLL_INTERVAL_MS = 1000;
const uint32_t MIN_GRID_POLL_INTERVAL_MS = 100;
const uint32_t MAX_GRID_POLL_INTERVAL_MS = 60000;
const uint32_t SETTINGS_REFRESH_INTERVAL_MS = 20;
const uint32_t WEBHOOK_RESPONSE_TIMEOUT_MS = 8000;
const uint32_t WEBHOOK_CHUNK_GAP_MS = 200;
const uint32_t GRID_COLOR_FADE_MS = 1000;

const size_t GRID_ENDPOINT_URL_MAX_LEN = 128;
const size_t GRID_PAYLOAD_MAX_LEN = 1024;
const char *GRID_WEBHOOK_EVENT = "grid-fetch";

const uint8_t RGB_TRIPLET_LEN = 11; // RRR.GGG.BBB
const uint8_t PIXEL_SLOT_LEN = 12;  // RRR.GGG.BBB,

// Mock payload for now (replace with HTTP response body later).
// Order is visual row-major: x=0..7, y=0..7.
const char *MOCK_GRID_PAYLOAD =
    "000.000.000,000.000.000,000.000.000,000.000.000,000.000.000,000.000.000,000.000.000,000.000.000,"
    "000.000.000,000.000.000,236.055.080,000.000.000,000.000.000,000.000.000,000.000.000,000.000.000,"
    "000.000.000,000.000.000,236.055.080,000.000.000,000.000.000,000.000.000,000.000.000,000.000.000,"
    "000.000.000,000.000.000,236.055.080,000.000.000,000.000.000,000.000.000,000.000.000,000.000.000,"
    "000.000.000,000.000.000,236.055.080,236.055.080,236.055.080,236.055.080,000.000.000,000.000.000,"
    "000.000.000,000.000.000,236.055.080,000.000.000,000.000.000,236.055.080,000.000.000,000.000.000,"
    "000.000.000,000.000.000,236.055.080,000.000.000,000.000.000,236.055.080,000.000.000,000.000.000,"
    "000.000.000,000.000.000,000.000.000,000.000.000,000.000.000,000.000.000,000.000.000,000.000.000";

uint32_t Wheel(byte WheelPos);
uint16_t xyToIndex(uint8_t x, uint8_t y);
void drawTargetPattern();
void pollGridStateOncePerSecond();
bool applyGridPayload(const char *payload);
int parse3Digits(const char *digits);
void refreshMirrorSettingsFromLedger();
void applyBrightness(uint8_t brightnessValue);
bool fetchGridPayloadHttp(const char *url, char *outPayload, size_t outPayloadSize);
void onGridWebhookResponse(const char *event, const char *data);
void finalizeWebhookPayloadIfComplete();
void renderLoadingSpinner();
void renderGridFadeTransition();

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

char webhookPayloadReadyBuffer[GRID_PAYLOAD_MAX_LEN] = {0};
char webhookAssembleBuffer[GRID_PAYLOAD_MAX_LEN] = {0};
size_t webhookAssembleLen = 0;
bool webhookPayloadReady = false;
bool webhookRequestInFlight = false;
uint32_t webhookRequestMs = 0;
uint32_t webhookLastChunkMs = 0;
uint32_t lastSettingsRefreshMs = 0;
bool hasReceivedGridPayload = false;
uint32_t lastSpinnerFrameMs = 0;
uint8_t spinnerFrame = 0;

uint8_t gridCurrentR[PIXEL_COUNT] = {0};
uint8_t gridCurrentG[PIXEL_COUNT] = {0};
uint8_t gridCurrentB[PIXEL_COUNT] = {0};
uint8_t gridStartR[PIXEL_COUNT] = {0};
uint8_t gridStartG[PIXEL_COUNT] = {0};
uint8_t gridStartB[PIXEL_COUNT] = {0};
uint8_t gridTargetR[PIXEL_COUNT] = {0};
uint8_t gridTargetG[PIXEL_COUNT] = {0};
uint8_t gridTargetB[PIXEL_COUNT] = {0};
bool gridFadeActive = false;
uint32_t gridFadeStartMs = 0;

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

  // Cloud-to-device configuration ledger (keys: MIRROR_X, MIRROR_Y as 0/1)
  ledSettings = Particle.ledger("led-settings");

  // Expose applied mirror orientation states as cloud variables (0 or 1)
  Particle.variable("mirrorXState", mirrorXState);
  Particle.variable("mirrorYState", mirrorYState);
  Particle.variable("pollMsState", gridPollIntervalState);
  Particle.variable("brightnessState", brightnessState);

  // Webhook response listener for HTTPS polling path.
  Particle.subscribe("hook-response/grid-fetch", onGridWebhookResponse, MY_DEVICES);

  // Ensure defaults are copied from constants into runtime fields.
  strncpy(gridEndpointUrl, DEFAULT_GRID_ENDPOINT_URL, sizeof(gridEndpointUrl) - 1);
  gridEndpointUrl[sizeof(gridEndpointUrl) - 1] = '\0';
}

// loop() runs over and over again, as quickly as it can execute.
void loop()
{
  const uint32_t now = millis();
  if (now - lastSettingsRefreshMs >= SETTINGS_REFRESH_INTERVAL_MS)
  {
    refreshMirrorSettingsFromLedger();
    lastSettingsRefreshMs = now;
  }

  if (!hasReceivedGridPayload)
  {
    renderLoadingSpinner();
  }
  else
  {
    renderGridFadeTransition();
  }

  pollGridStateOncePerSecond();
  delay(5);
}

void renderLoadingSpinner()
{
  const uint32_t now = millis();
  const uint32_t frameIntervalMs = 70;
  if (now - lastSpinnerFrameMs < frameIntervalMs)
  {
    return;
  }
  lastSpinnerFrameMs = now;

  // 12-point circle around matrix center.
  static const uint8_t spinnerX[] = {3, 4, 5, 6, 6, 5, 4, 3, 2, 1, 1, 2};
  static const uint8_t spinnerY[] = {1, 1, 2, 3, 4, 5, 6, 6, 5, 4, 3, 2};
  static const uint8_t spinnerLen = sizeof(spinnerX) / sizeof(spinnerX[0]);

  // Clear frame.
  for (int i = 0; i < PIXEL_COUNT; ++i)
  {
    strip.setPixelColor(i, 0);
  }

  // Draw head and a soft tail.
  for (uint8_t t = 0; t < 4; ++t)
  {
    int idx = (int)spinnerFrame - (int)t;
    while (idx < 0)
    {
      idx += spinnerLen;
    }
    idx %= spinnerLen;

    // Light purple with fading tail intensity.
    uint8_t v = (uint8_t)(200 - (t * 55));
    uint32_t color = strip.Color(v, (uint8_t)(v * 2 / 3), v);
    strip.setPixelColor(xyToIndex(spinnerX[idx], spinnerY[idx]), color);
  }

  strip.show();
  spinnerFrame = (uint8_t)((spinnerFrame + 1) % spinnerLen);
}

void renderGridFadeTransition()
{
  if (!gridFadeActive)
  {
    return;
  }

  const uint32_t now = millis();
  uint32_t elapsed = now - gridFadeStartMs;
  if (elapsed > GRID_COLOR_FADE_MS)
  {
    elapsed = GRID_COLOR_FADE_MS;
  }

  for (int i = 0; i < PIXEL_COUNT; ++i)
  {
    int r = (int)gridStartR[i] + (((int)gridTargetR[i] - (int)gridStartR[i]) * (int)elapsed) / (int)GRID_COLOR_FADE_MS;
    int g = (int)gridStartG[i] + (((int)gridTargetG[i] - (int)gridStartG[i]) * (int)elapsed) / (int)GRID_COLOR_FADE_MS;
    int b = (int)gridStartB[i] + (((int)gridTargetB[i] - (int)gridStartB[i]) * (int)elapsed) / (int)GRID_COLOR_FADE_MS;

    gridCurrentR[i] = (uint8_t)r;
    gridCurrentG[i] = (uint8_t)g;
    gridCurrentB[i] = (uint8_t)b;
    strip.setPixelColor(i, strip.Color((uint8_t)r, (uint8_t)g, (uint8_t)b));
  }

  strip.show();

  if (elapsed >= GRID_COLOR_FADE_MS)
  {
    gridFadeActive = false;
  }
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

  const char *payload = NULL;
#if USE_MOCK_GRID
  payload = MOCK_GRID_PAYLOAD;
#else
  String endpoint(gridEndpointUrl);
  endpoint.trim();

  if (endpoint.startsWith("https://"))
  {
    finalizeWebhookPayloadIfComplete();

    if (webhookPayloadReady)
    {
      payload = webhookPayloadReadyBuffer;
      webhookPayloadReady = false;
    }
    else
    {
      if (!webhookRequestInFlight)
      {
        if (Particle.connected())
        {
          bool queued = Particle.publish(GRID_WEBHOOK_EVENT, endpoint, PRIVATE);
          if (queued)
          {
            webhookRequestInFlight = true;
            webhookRequestMs = now;
          }
          else
          {
            Log.warn("Failed to publish webhook request for %s", gridEndpointUrl);
          }
        }
        else
        {
          Log.warn("Cloud disconnected; cannot request HTTPS grid via webhook");
        }
      }
      else if (now - webhookRequestMs > WEBHOOK_RESPONSE_TIMEOUT_MS)
      {
        webhookRequestInFlight = false;
        webhookAssembleLen = 0;
        webhookAssembleBuffer[0] = '\0';
        Log.warn("Webhook response timeout for %s", gridEndpointUrl);
      }

      // Wait for webhook payload, skip parse/warn this cycle.
      return;
    }
  }
  else
  {
    static char livePayload[GRID_PAYLOAD_MAX_LEN];
    if (fetchGridPayloadHttp(gridEndpointUrl, livePayload, sizeof(livePayload)))
    {
      payload = livePayload;
    }
    else
    {
      payload = NULL;
    }
  }
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

void onGridWebhookResponse(const char *event, const char *data)
{
  if (data == NULL)
  {
    return;
  }

  int chunkIndex = -1;
  if (event != NULL)
  {
    const char *lastSlash = strrchr(event, '/');
    if (lastSlash != NULL && *(lastSlash + 1) != '\0')
    {
      bool numeric = true;
      for (const char *p = lastSlash + 1; *p != '\0'; ++p)
      {
        if (*p < '0' || *p > '9')
        {
          numeric = false;
          break;
        }
      }
      if (numeric)
      {
        chunkIndex = atoi(lastSlash + 1);
      }
    }
  }

  // New chunked response starts at /0.
  if (chunkIndex == 0)
  {
    webhookAssembleLen = 0;
    webhookAssembleBuffer[0] = '\0';
  }

  size_t incomingLen = strlen(data);
  if (incomingLen == 0)
  {
    return;
  }

  size_t remaining = GRID_PAYLOAD_MAX_LEN - 1 - webhookAssembleLen;
  if (incomingLen > remaining)
  {
    incomingLen = remaining;
  }

  if (incomingLen > 0)
  {
    memcpy(&webhookAssembleBuffer[webhookAssembleLen], data, incomingLen);
    webhookAssembleLen += incomingLen;
    webhookAssembleBuffer[webhookAssembleLen] = '\0';
    webhookLastChunkMs = millis();
  }

  // Unchunked webhook response arrives as a single event without /N suffix.
  if (chunkIndex < 0)
  {
    strncpy(webhookPayloadReadyBuffer, webhookAssembleBuffer, sizeof(webhookPayloadReadyBuffer) - 1);
    webhookPayloadReadyBuffer[sizeof(webhookPayloadReadyBuffer) - 1] = '\0';
    webhookPayloadReady = true;
    webhookRequestInFlight = false;
    webhookAssembleLen = 0;
    webhookAssembleBuffer[0] = '\0';
  }
}

void finalizeWebhookPayloadIfComplete()
{
  if (webhookAssembleLen == 0)
  {
    return;
  }

  if (millis() - webhookLastChunkMs < WEBHOOK_CHUNK_GAP_MS)
  {
    return;
  }

  strncpy(webhookPayloadReadyBuffer, webhookAssembleBuffer, sizeof(webhookPayloadReadyBuffer) - 1);
  webhookPayloadReadyBuffer[sizeof(webhookPayloadReadyBuffer) - 1] = '\0';
  webhookPayloadReady = true;
  webhookRequestInFlight = false;
  webhookAssembleLen = 0;
  webhookAssembleBuffer[0] = '\0';
}

bool fetchGridPayloadHttp(const char *url, char *outPayload, size_t outPayloadSize)
{
  if (url == NULL || outPayload == NULL || outPayloadSize < 2)
  {
    return false;
  }

  outPayload[0] = '\0';

  String urlStr(url);
  urlStr.trim();

  if (urlStr.length() == 0)
  {
    return false;
  }

  // Particle TCPClient does not support TLS/SSL directly.
  if (urlStr.startsWith("https://"))
  {
    Log.warn("HTTPS URL not supported by direct device polling: %s", url);
    return false;
  }

  String working = urlStr;
  if (working.startsWith("http://"))
  {
    working = working.substring(7);
  }

  int slashPos = working.indexOf('/');
  String hostPort = (slashPos >= 0) ? working.substring(0, slashPos) : working;
  String path = (slashPos >= 0) ? working.substring(slashPos) : String("/");

  int port = 80;
  int colonPos = hostPort.indexOf(':');
  String host = hostPort;
  if (colonPos >= 0)
  {
    host = hostPort.substring(0, colonPos);
    int parsedPort = hostPort.substring(colonPos + 1).toInt();
    if (parsedPort > 0)
    {
      port = parsedPort;
    }
  }

  host.trim();
  if (host.length() == 0)
  {
    return false;
  }

  TCPClient client;
  if (!client.connect(host.c_str(), (uint16_t)port))
  {
    Log.warn("HTTP connect failed: %s:%d", host.c_str(), port);
    return false;
  }

  client.print("GET ");
  client.print(path);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(host);
  client.println("Connection: close");
  client.println("Accept: text/plain");
  client.println("User-Agent: paint-a-booth/1.0");
  client.println();

  String response;
  uint32_t start = millis();
  const uint32_t timeoutMs = 5000;
  while (millis() - start < timeoutMs)
  {
    while (client.available())
    {
      response += (char)client.read();
      start = millis();
    }

    if (!client.connected())
    {
      break;
    }
    delay(1);
  }
  client.stop();

  if (response.length() == 0)
  {
    Log.warn("HTTP response empty from %s", url);
    return false;
  }

  // Basic status check: first line should include 200.
  int statusLineEnd = response.indexOf('\n');
  if (statusLineEnd < 0)
  {
    return false;
  }
  String statusLine = response.substring(0, statusLineEnd);
  if (statusLine.indexOf(" 200 ") < 0)
  {
    Log.warn("HTTP non-200 response: %s", statusLine.c_str());
    return false;
  }

  int bodyPos = response.indexOf("\r\n\r\n");
  if (bodyPos < 0)
  {
    bodyPos = response.indexOf("\n\n");
    if (bodyPos < 0)
    {
      return false;
    }
    bodyPos += 2;
  }
  else
  {
    bodyPos += 4;
  }

  String body = response.substring(bodyPos);
  body.trim();
  if (body.length() == 0)
  {
    return false;
  }

  if (body.length() >= (int)outPayloadSize)
  {
    Log.warn("Grid payload too large: %d bytes (max %d)", body.length(), (int)(outPayloadSize - 1));
    return false;
  }

  strncpy(outPayload, body.c_str(), outPayloadSize - 1);
  outPayload[outPayloadSize - 1] = '\0';
  return true;
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

  // Capture current displayed frame as fade start and clear targets to black.
  for (int i = 0; i < PIXEL_COUNT; ++i)
  {
    gridStartR[i] = gridCurrentR[i];
    gridStartG[i] = gridCurrentG[i];
    gridStartB[i] = gridCurrentB[i];
    gridTargetR[i] = 0;
    gridTargetG[i] = 0;
    gridTargetB[i] = 0;
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
    uint16_t p = xyToIndex(x, y);
    gridTargetR[p] = (uint8_t)r;
    gridTargetG[p] = (uint8_t)g;
    gridTargetB[p] = (uint8_t)b;

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

  gridFadeStartMs = millis();
  gridFadeActive = true;
  renderGridFadeTransition();
  hasReceivedGridPayload = true;
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
// Row direction alternates each row, with row 0 direction selected by
// SERPENTINE_ROW0_LEFT_TO_RIGHT.
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
  bool reverseRow = false;
  if (MATRIX_IS_SERPENTINE)
  {
    reverseRow = SERPENTINE_ROW0_LEFT_TO_RIGHT ? ((y & 0x01) != 0) : ((y & 0x01) == 0);
  }
  if (reverseRow)
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
