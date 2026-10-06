#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>

#include "config.h"
#include "secrets.h"
#include "web_page.h"

// --- Types & Enums ---

enum class BlindPos : uint8_t { Unknown, Up, Down };
enum class MoveDir  : uint8_t { Up, Down };
enum class MotorPhase : uint8_t { Idle, DeadTime, Running };

// Saved in NVS
struct Settings {
  float    timeUpS          = DEFAULT_TIME_UP_S;
  float    timeDownS        = DEFAULT_TIME_DOWN_S;
  bool     autoRiseEnabled  = false;
  bool     autoDropEnabled  = false;
  uint16_t autoRiseMin      = 6 * 60; // mins from midnight
  uint16_t autoDropMin      = 21 * 60;
  bool     sunRiseEnabled   = false;
  bool     sunSetEnabled    = false;
};

// Reset on reboot
struct Runtime {
  BlindPos blindPos       = BlindPos::Unknown;
  bool     systemReady    = false;
  bool     overrideUp     = false;
  bool     overrideDown   = false;

  bool     delayedPending = false;
  MoveDir  delayedDir     = MoveDir::Up;
  uint32_t delayedAt      = 0;

  int      sunriseMin     = 6 * 60;
  int      sunsetMin      = 20 * 60;
  int      sunCalcDay     = -1;

  int      lastRiseDay    = -1;
  int      lastDropDay    = -1;

  bool     wifiUp         = false;
  bool     ntpSynced      = false;
};

struct Motor {
  MotorPhase phase      = MotorPhase::Idle;
  MoveDir    dir        = MoveDir::Up;
  uint32_t   durationMs = 0;
  uint32_t   deadline   = 0;
  uint32_t   lastStopMs = static_cast<uint32_t>(0) - MOTOR_REVERSE_DEADTIME_MS;
  bool       manual     = false;
};

static Settings cfg;
static Runtime  rt;
static Motor    motor;

static WebServer   server(HTTP_PORT);
static Preferences prefs;

static char    logBuf[LOG_CAPACITY][LOG_LINE_LEN];
static uint8_t logHead  = 0;
static uint8_t logCount = 0;

// --- Helpers ---

// Safe timer check against millis() overflow
static inline bool reached(uint32_t deadline) {
  return static_cast<int32_t>(millis() - deadline) >= 0;
}

static inline uint32_t secToMs(float s) {
  return static_cast<uint32_t>(s * 1000.0f + 0.5f);
}

static bool getNow(struct tm &out) {
  const time_t now = time(nullptr);
  if (now < MIN_VALID_EPOCH) return false;
  localtime_r(&now, &out);
  return true;
}

static void formatHm(char *buf, size_t len, int minutes) {
  snprintf(buf, len, "%02d:%02d", minutes / 60, minutes % 60);
}

// Fixed ring-buffer logger
static void addLog(const char *fmt, ...) {
  char msg[LOG_LINE_LEN - 12];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);

  char stamp[9] = "??:??:??";
  struct tm t;
  if (getNow(t)) snprintf(stamp, sizeof(stamp), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);

  snprintf(logBuf[logHead], LOG_LINE_LEN, "%s  %s", stamp, msg);
  Serial.printf("[LOG] %s\n", logBuf[logHead]);
  logHead = (logHead + 1) % LOG_CAPACITY;
  if (logCount < LOG_CAPACITY) logCount++;
}

static const char *dirWord(MoveDir d) { return d == MoveDir::Up ? "podnoszenie" : "opuszczanie"; }
static const char *dirArrow(MoveDir d) { return d == MoveDir::Up ? "↑" : "↓"; }

// --- Motor Control ---

static void relaysOff() {
  digitalWrite(PIN_RELAY_UP, RELAY_OFF);
  digitalWrite(PIN_RELAY_DOWN, RELAY_OFF);
}

static void motorEnergise() {
  relaysOff(); // make sure both are off first
  digitalWrite(motor.dir == MoveDir::Up ? PIN_RELAY_UP : PIN_RELAY_DOWN, RELAY_ON);
  motor.phase    = MotorPhase::Running;
  motor.deadline = millis() + motor.durationMs;
  addLog("Motor START %s %lums", dirArrow(motor.dir), static_cast<unsigned long>(motor.durationMs));
}

static void motorHalt() {
  relaysOff();
  if (motor.phase == MotorPhase::Running) motor.lastStopMs = millis();
  motor.phase  = MotorPhase::Idle;
  motor.manual = false;
}

// Reversals wait for deadtime to protect the motor/relays
static void motorRequest(MoveDir dir, uint32_t ms, bool manual) {
  if (motor.phase == MotorPhase::Running) {
    if (motor.dir == dir) { // reset timer if moving same way
      motor.durationMs = ms;
      motor.deadline   = millis() + ms;
      motor.manual     = manual;
      return;
    }
    motorHalt(); // stop first if swapping direction
  }
  motor.dir        = dir;
  motor.durationMs = ms;
  motor.manual     = manual;

  const uint32_t readyAt = motor.lastStopMs + MOTOR_REVERSE_DEADTIME_MS;
  if (reached(readyAt)) {
    motorEnergise();
  } else {
    motor.phase    = MotorPhase::DeadTime;
    motor.deadline = readyAt;
  }
}

static void motorUpdate() {
  if (motor.phase == MotorPhase::DeadTime && reached(motor.deadline)) {
    motorEnergise();
  } else if (motor.phase == MotorPhase::Running && reached(motor.deadline)) {
    motorHalt();
    addLog("Motor STOP (timeout)");
  }
}

// --- High-Level Commands ---

static void doFullUp() {
  if (rt.blindPos == BlindPos::Up) { addLog("Już w górze – pomijam"); return; }
  motorRequest(MoveDir::Up, secToMs(cfg.timeUpS), false);
  rt.blindPos     = BlindPos::Up;
  rt.overrideDown = false;
  addLog("Pełne podniesienie");
}

static void doFullDown() {
  if (rt.blindPos == BlindPos::Down) { addLog("Już na dole – pomijam"); return; }
  motorRequest(MoveDir::Down, secToMs(cfg.timeDownS), false);
  rt.blindPos   = BlindPos::Down;
  rt.overrideUp = false;
  addLog("Pełne opuszczenie");
}

static void doFull(MoveDir d) { if (d == MoveDir::Up) doFullUp(); else doFullDown(); }

// --- Solar Calc ---

static int wrapDayMinutes(long m) {
  m %= 1440;
  if (m < 0) m += 1440;
  return static_cast<int>(m);
}

// NOAA approximation (~2 min error margin)
static void calcSunTimes() {
  struct tm t;
  if (!getNow(t)) return;

  const double rad = M_PI / 180.0;
  const double g   = 2.0 * M_PI / 365.0 * t.tm_yday;

  const double eqTime = 229.18 * (0.000075 + 0.001868 * cos(g) - 0.032077 * sin(g)
                                  - 0.014615 * cos(2 * g) - 0.040849 * sin(2 * g));
  const double decl = 0.006918 - 0.399912 * cos(g) + 0.070257 * sin(g)
                      - 0.006758 * cos(2 * g) + 0.000907 * sin(2 * g)
                      - 0.002697 * cos(3 * g) + 0.00148 * sin(3 * g);

  const double latR  = SITE_LAT_DEG * rad;
  double cosHa = cos(90.833 * rad) / (cos(latR) * cos(decl)) - tan(latR) * tan(decl);
  cosHa = fmax(-1.0, fmin(1.0, cosHa));
  const double haDeg = acos(cosHa) / rad;

  const double riseUtc = 720.0 - 4.0 * (SITE_LON_DEG + haDeg) - eqTime;
  const double setUtc  = 720.0 - 4.0 * (SITE_LON_DEG - haDeg) - eqTime;

  struct tm noon = t;
  noon.tm_hour = 12; noon.tm_min = 0; noon.tm_sec = 0; noon.tm_isdst = -1;
  mktime(&noon);
  const int tzMin = TZ_STD_OFFSET_MIN + (noon.tm_isdst > 0 ? TZ_DST_EXTRA_MIN : 0);

  rt.sunriseMin  = wrapDayMinutes(lround(riseUtc) + tzMin);
  rt.sunsetMin   = wrapDayMinutes(lround(setUtc) + tzMin);
  rt.sunCalcDay  = t.tm_yday;

  char a[6], b[6];
  formatHm(a, sizeof(a), rt.sunriseMin);
  formatHm(b, sizeof(b), rt.sunsetMin);
  addLog("Wschód: %s  Zachód: %s", a, b);
}

// --- Persistence ---

static void loadSettings() {
  if (!prefs.begin(NVS_NAMESPACE, false)) { Serial.println("NVS: open failed, using defaults"); return; }
  cfg.timeUpS         = constrain(prefs.getFloat("tUp", DEFAULT_TIME_UP_S), MIN_TRAVEL_TIME_S, MAX_TRAVEL_TIME_S);
  cfg.timeDownS       = constrain(prefs.getFloat("tDn", DEFAULT_TIME_DOWN_S), MIN_TRAVEL_TIME_S, MAX_TRAVEL_TIME_S);
  cfg.autoRiseEnabled = prefs.getBool("riseEn", false);
  cfg.autoDropEnabled = prefs.getBool("dropEn", false);
  cfg.sunRiseEnabled  = prefs.getBool("sunRise", false);
  cfg.sunSetEnabled   = prefs.getBool("sunSet", false);
  const uint16_t r    = prefs.getUShort("riseMin", 6 * 60);
  const uint16_t d    = prefs.getUShort("dropMin", 21 * 60);
  cfg.autoRiseMin     = (r < 1440) ? r : 6 * 60;
  cfg.autoDropMin     = (d < 1440) ? d : 21 * 60;
  prefs.end();
}

static void saveSettings() {
  if (!prefs.begin(NVS_NAMESPACE, false)) { addLog("NVS: błąd zapisu"); return; }
  prefs.putFloat("tUp", cfg.timeUpS);
  prefs.putFloat("tDn", cfg.timeDownS);
  prefs.putBool("riseEn", cfg.autoRiseEnabled);
  prefs.putBool("dropEn", cfg.autoDropEnabled);
  prefs.putBool("sunRise", cfg.sunRiseEnabled);
  prefs.putBool("sunSet", cfg.sunSetEnabled);
  prefs.putUShort("riseMin", cfg.autoRiseMin);
  prefs.putUShort("dropMin", cfg.autoDropMin);
  prefs.end();
}

// --- Web Server & JSON ---

static void jsonEscape(String &out, const char *s) {
  for (; *s; ++s) {
    const char c = *s;
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (static_cast<uint8_t>(c) < 0x20) out += ' ';
    else out += c;
  }
}
static void jBool(String &j, const char *k, bool v) {
  j += ",\""; j += k; j += "\":"; j += v ? "true" : "false";
}
static void jNum(String &j, const char *k, float v) {
  j += ",\""; j += k; j += "\":"; j += String(v, 1);
}
static void jStr(String &j, const char *k, const char *v) {
  j += ",\""; j += k; j += "\":\""; jsonEscape(j, v); j += '"';
}

static const char *posName(BlindPos p) {
  switch (p) {
    case BlindPos::Up:   return "up";
    case BlindPos::Down: return "down";
    default:             return "unknown";
  }
}

static String buildStatusJson() {
  String j;
  j.reserve(STATUS_JSON_RESERVE);
  j += F("{\"ok\":true");

  jStr(j, "pos", posName(rt.blindPos));
  jBool(j, "motor", motor.phase == MotorPhase::Running);
  jBool(j, "motorUp", motor.dir == MoveDir::Up);
  jBool(j, "ntp", rt.ntpSynced);
  jStr(j, "ip", WiFi.localIP().toString().c_str());
  jNum(j, "timeUp", cfg.timeUpS);
  jNum(j, "timeDown", cfg.timeDownS);
  jBool(j, "autoRise", cfg.autoRiseEnabled);
  jBool(j, "autoDrop", cfg.autoDropEnabled);

  char hm[6];
  formatHm(hm, sizeof(hm), cfg.autoRiseMin);  jStr(j, "riseTime", hm);
  formatHm(hm, sizeof(hm), cfg.autoDropMin);  jStr(j, "dropTime", hm);
  jBool(j, "sunRise", cfg.sunRiseEnabled);
  jBool(j, "sunSet", cfg.sunSetEnabled);
  formatHm(hm, sizeof(hm), rt.sunriseMin);    jStr(j, "sunriseStr", hm);
  formatHm(hm, sizeof(hm), rt.sunsetMin);     jStr(j, "sunsetStr", hm);

  char label[40] = "";
  if (rt.delayedPending) {
    int32_t left = static_cast<int32_t>(rt.delayedAt - millis()) / 1000;
    if (left < 0) left = 0;
    snprintf(label, sizeof(label), "%s za %dmin %ds", dirArrow(rt.delayedDir),
             static_cast<int>(left / 60), static_cast<int>(left % 60));
  }
  jBool(j, "delayed", rt.delayedPending);
  jStr(j, "delayLabel", label);

  j += F(",\"logs\":[");
  for (uint8_t i = 0; i < logCount; i++) {
    const uint8_t idx = (logHead + LOG_CAPACITY - 1 - i) % LOG_CAPACITY;
    if (i) j += ',';
    j += '"'; jsonEscape(j, logBuf[idx]); j += '"';
  }
  j += F("]}");
  return j;
}

static bool authorised() {
#if WEB_AUTH_ENABLED
  if (!server.authenticate(WEB_USER, WEB_PASS)) {
    server.requestAuthentication();
    return false;
  }
#endif
  return true;
}
#define REQUIRE_AUTH() do { if (!authorised()) return; } while (0)

static void sendStatus() { server.send(200, "application/json", buildStatusJson()); }

static void sendError(int code, const char *msg) {
  String body = F("{\"ok\":false,\"error\":\"");
  jsonEscape(body, msg);
  body += F("\"}");
  server.send(code, "application/json", body);
}

static bool parseDir(const String &s, MoveDir &out) {
  if (s == "up")   { out = MoveDir::Up;   return true; }
  if (s == "down") { out = MoveDir::Down; return true; }
  return false;
}

static bool parseHhMm(const String &s, uint16_t &minutes) {
  if (s.length() != 5 || s[2] != ':') return false;
  const int idx[4] = {0, 1, 3, 4};
  for (int i : idx) if (!isDigit(s[i])) return false;
  const int h = s.substring(0, 2).toInt();
  const int m = s.substring(3, 5).toInt();
  if (h > 23 || m > 59) return false;
  minutes = static_cast<uint16_t>(h * 60 + m);
  return true;
}

// --- HTTP Routes ---

static void handleRoot() {
  REQUIRE_AUTH();
  server.send_P(200, "text/html", PAGE_HTML);
}

static void handleStatus() {
  REQUIRE_AUTH();
  sendStatus();
}

static void handleInit() {
  REQUIRE_AUTH();
  const String pos = server.arg("pos");
  if (pos == "up") {
    rt.blindPos = BlindPos::Up;
    addLog("Init: PODNIESIONA");
  } else if (pos == "down") {
    rt.blindPos = BlindPos::Down;
    addLog("Init: OPUSZCZONA");
  } else {
    sendError(400, "pos must be 'up' or 'down'");
    return;
  }
  rt.systemReady = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleFull() {
  REQUIRE_AUTH();
  if (!rt.systemReady) { sendError(409, "not initialised"); return; }
  MoveDir d;
  if (!parseDir(server.arg("dir"), d)) { sendError(400, "bad dir"); return; }
  doFull(d);
  rt.overrideUp   = (d == MoveDir::Up);
  rt.overrideDown = (d == MoveDir::Down);
  sendStatus();
}

static void handleManual() {
  REQUIRE_AUTH();
  if (!rt.systemReady) { sendError(409, "not initialised"); return; }
  MoveDir d;
  if (!parseDir(server.arg("dir"), d)) { sendError(400, "bad dir"); return; }
  const String act = server.arg("act");
  const bool up = (d == MoveDir::Up);

  if (act == "start") {
    if (motor.phase == MotorPhase::Idle) motorRequest(d, MANUAL_MAX_MS, true);
  } else if (act == "pulse") {
    // Keep-alive pulse extends runtime
    if (motor.phase == MotorPhase::Running && motor.manual && motor.dir == d)
      motor.deadline = millis() + MANUAL_PULSE_TIMEOUT_MS;
  } else if (act == "stop") {
    motorHalt();
    rt.blindPos     = up ? BlindPos::Up : BlindPos::Down;
    rt.overrideUp   = up ? true : rt.overrideUp;
    rt.overrideDown = up ? rt.overrideDown : true;
    addLog("Ręczne STOP %s", dirArrow(d));
  } else {
    sendError(400, "bad act");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleTiming() {
  REQUIRE_AUTH();
  const float u = server.arg("up").toFloat();
  const float d = server.arg("down").toFloat();
  if (u < MIN_TRAVEL_TIME_S || u > MAX_TRAVEL_TIME_S ||
      d < MIN_TRAVEL_TIME_S || d > MAX_TRAVEL_TIME_S) {
    sendError(400, "time out of range");
    return;
  }
  cfg.timeUpS   = u;
  cfg.timeDownS = d;
  saveSettings();
  addLog("Czasy: ↑%.1fs ↓%.1fs", cfg.timeUpS, cfg.timeDownS);
  sendStatus();
}

static void handleSchedule() {
  REQUIRE_AUTH();
  uint16_t rise = cfg.autoRiseMin, drop = cfg.autoDropMin;
  if (!parseHhMm(server.arg("riseTime"), rise) || !parseHhMm(server.arg("dropTime"), drop)) {
    sendError(400, "time must be HH:MM");
    return;
  }
  cfg.autoRiseEnabled = (server.arg("riseEn") == "1");
  cfg.autoDropEnabled = (server.arg("dropEn") == "1");
  cfg.autoRiseMin = rise;
  cfg.autoDropMin = drop;
  saveSettings();
  addLog("Harmonogram zapisany");
  sendStatus();
}

static void handleSun() {
  REQUIRE_AUTH();
  cfg.sunRiseEnabled = (server.arg("sunRise") == "1");
  cfg.sunSetEnabled  = (server.arg("sunSet") == "1");
  saveSettings();
  addLog("Słońce: wschód=%d zachód=%d", cfg.sunRiseEnabled, cfg.sunSetEnabled);
  sendStatus();
}

static void handleDelay() {
  REQUIRE_AUTH();
  if (server.arg("cancel") == "1") {
    rt.delayedPending = false;
    addLog("Anulowano zaplanowaną akcję");
    sendStatus();
    return;
  }
  if (!rt.systemReady) { sendError(409, "not initialised"); return; }
  MoveDir d;
  if (!parseDir(server.arg("dir"), d)) { sendError(400, "bad dir"); return; }
  long mins = server.arg("min").toInt();
  if (mins < 1) mins = 1;
  if (mins > MAX_DELAY_MIN) mins = MAX_DELAY_MIN;

  rt.delayedPending = true;
  rt.delayedDir     = d;
  rt.delayedAt      = millis() + static_cast<uint32_t>(mins) * 60000UL;
  addLog("Zaplanowano: %s za %ld min", dirArrow(d), mins);
  sendStatus();
}

static void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// --- Network & Background Tasks ---

static void onWifiConnected() {
  addLog("WiFi: %s", WiFi.localIP().toString().c_str());

  MDNS.end();
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    addLog("mDNS aktywny: http://%s.local", HOSTNAME);
  } else {
    addLog("mDNS błąd konfiguracji");
  }

  configTzTime(TZ_POSIX, NTP_SERVER_1, NTP_SERVER_2);
}

static void networkUpdate() {
  static uint32_t lastCheck = 0, lastRetry = 0;
  if (!reached(lastCheck + NETWORK_CHECK_PERIOD_MS)) return;
  lastCheck = millis();

  const bool up = (WiFi.status() == WL_CONNECTED);
  if (up && !rt.wifiUp) {
    rt.wifiUp = true;
    onWifiConnected();
  } else if (!up && rt.wifiUp) {
    rt.wifiUp = false;
    lastRetry = millis();
    addLog("WiFi: utracono połączenie");
  } else if (!up && reached(lastRetry + WIFI_RECONNECT_INTERVAL_MS)) {
    lastRetry = millis();
    WiFi.reconnect();
  }

  if (!rt.ntpSynced) {
    struct tm t;
    if (getNow(t)) {
      rt.ntpSynced = true;
      addLog("NTP zsynchronizowany");
      calcSunTimes();
    }
  }
}

static void delayedActionUpdate() {
  if (rt.delayedPending && reached(rt.delayedAt)) {
    rt.delayedPending = false;
    doFull(rt.delayedDir);
    addLog("Wykonano zaplanowaną akcję");
  }
}

static void fireAuto(MoveDir d, const char *source) {
  bool &override_ = (d == MoveDir::Up) ? rt.overrideUp : rt.overrideDown;
  if (override_) {
    override_ = false;
    addLog("Auto %s pominięte (ręczne nadpisanie)", dirWord(d));
    return;
  }
  doFull(d);
  addLog("Auto %s (%s)", dirWord(d), source);
}

static void schedulerUpdate() {
  static uint32_t lastRun = 0;
  if (!reached(lastRun + SCHEDULER_PERIOD_MS)) return;
  lastRun = millis();

  if (!rt.ntpSynced) return;
  struct tm t;
  if (!getNow(t)) return;

  if (t.tm_yday != rt.sunCalcDay) calcSunTimes();
  if (!rt.systemReady) return;

  const int nowMin = t.tm_hour * 60 + t.tm_min;
  const int today  = t.tm_yday;

  int riseAt = -1, dropAt = -1;
  const char *riseSrc = "", *dropSrc = "";
  if (cfg.sunRiseEnabled)       { riseAt = rt.sunriseMin;  riseSrc = "wschód słońca"; }
  else if (cfg.autoRiseEnabled){ riseAt = cfg.autoRiseMin; riseSrc = "harmonogram"; }
  if (cfg.sunSetEnabled)        { dropAt = rt.sunsetMin;   dropSrc = "zachód słońca"; }
  else if (cfg.autoDropEnabled){ dropAt = cfg.autoDropMin; dropSrc = "harmonogram"; }

  if (riseAt >= 0 && nowMin == riseAt && rt.lastRiseDay != today) {
    rt.lastRiseDay = today;
    fireAuto(MoveDir::Up, riseSrc);
  }
  if (dropAt >= 0 && nowMin == dropAt && rt.lastDropDay != today) {
    rt.lastDropDay = today;
    fireAuto(MoveDir::Down, dropSrc);
  }
}

static void ledUpdate() {
  static uint32_t last = 0;
  static bool on = false;
  const uint32_t period = (motor.phase == MotorPhase::Running) ? LED_PERIOD_BUSY_MS : LED_PERIOD_IDLE_MS;
  if (millis() - last > period) {
    last = millis();
    on = !on;
    digitalWrite(PIN_LED, on ? LED_ON : LED_OFF);
  }
}

// --- Main Setup & Loop ---

void setup() {
  // Ensure relays are off at boot
  pinMode(PIN_RELAY_UP, OUTPUT);
  pinMode(PIN_RELAY_DOWN, OUTPUT);
  relaysOff();
  pinMode(PIN_LED, OUTPUT);

  Serial.begin(115200);
  loadSettings();

  // LED startup blink
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_LED, LED_ON);  delay(120);
    digitalWrite(PIN_LED, LED_OFF); delay(120);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  server.on("/",         HTTP_GET,  handleRoot);
  server.on("/status",   HTTP_GET,  handleStatus);
  server.on("/init",     HTTP_POST, handleInit);
  server.on("/full",     HTTP_POST, handleFull);
  server.on("/manual",   HTTP_POST, handleManual);
  server.on("/timing",   HTTP_POST, handleTiming);
  server.on("/schedule", HTTP_POST, handleSchedule);
  server.on("/sun",      HTTP_POST, handleSun);
  server.on("/delay",    HTTP_POST, handleDelay);
  server.onNotFound(handleNotFound);
  server.begin();
  addLog("Serwer HTTP uruchomiony");
}

void loop() {
  server.handleClient();
  motorUpdate();
  delayedActionUpdate();
  schedulerUpdate();
  networkUpdate();
  ledUpdate();
}