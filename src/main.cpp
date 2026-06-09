/*
 * ============================================================
 *  HELTEC 2 — LoRa Receiver + Built-in Web Server
 * ============================================================
 *  Board  : Heltec Wireless Stick V3 (ESP32-S3)
 *  MAC    : 64:E8:33:67:95:98
 *  WiFi   : KT_Guest
 *
 *  Open browser and go to the IP shown in Serial Monitor
 *  e.g. http://192.168.0.45
 *  Page auto-refreshes every 10 seconds
 * ============================================================
 */

//GOOD MORNING, GOOD AFTERNOON, GOOD NIGHT
//GOODBYE, SEE YOU TOMORROW!

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <WiFi.h>
#include <WebServer.h>

// ─────────────────────────────────────────────
//  WiFi
// ─────────────────────────────────────────────
#define WIFI_SSID  "Bobby"
#define WIFI_PASS  "babababa"

// ─────────────────────────────────────────────
//  LoRa Pins
// ─────────────────────────────────────────────
#define LORA_CS   8
#define LORA_SCK  9
#define LORA_MOSI 10
#define LORA_MISO 11
#define LORA_RST  12
#define LORA_BUSY 13
#define LORA_DIO1 14

#define LORA_FREQ   915.0
#define LORA_BW     125.0
#define LORA_SF     9
#define LORA_CR     7
#define LORA_SYNC   0x12
#define LORA_PWR    17

// ─────────────────────────────────────────────
//  SX1262 radio + Web server
// ─────────────────────────────────────────────
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
WebServer server(80);

// ─────────────────────────────────────────────
//  Latest sensor data (updated every LoRa packet)
// ─────────────────────────────────────────────
struct SensorData {
  float soil_moisture    = -1;
  float soil_temperature = -1;
  float conductivity     = -1;
  float ph               = -1;
  float air_temperature  = -1;
  float humidity         = -1;
  float distance         = -1;
  int   lora_rssi        = 0;
  float lora_snr         = 0;
  int   packet_count     = 0;
  String last_received   = "Never";
};

SensorData latest;

// ─────────────────────────────────────────────
//  Parse LoRa CSV payload
//  Format: H1,<moisture>,<soilTemp>,<ec>,<ph>,<airTemp>,<humidity>,<distance>
// ─────────────────────────────────────────────
bool parsePayload(const String &raw) {
  if (!raw.startsWith("H1,")) return false;

  String s = raw.substring(3);
  float vals[7];
  int idx = 0, start = 0;

  for (int i = 0; i <= (int)s.length() && idx < 7; i++) {
    if (i == (int)s.length() || s[i] == ',') {
      vals[idx++] = s.substring(start, i).toFloat();
      start = i + 1;
    }
  }

  if (idx < 7) return false;

  latest.soil_moisture    = vals[0];
  latest.soil_temperature = vals[1];
  latest.conductivity     = vals[2];
  latest.ph               = vals[3];
  latest.air_temperature  = vals[4];
  latest.humidity         = vals[5];
  latest.distance         = vals[6];
  return true;
}

// ─────────────────────────────────────────────
//  Helper: format value or show N/A
// ─────────────────────────────────────────────
String fmt(float v, int decimals = 2) {
  if (v < 0) return "<span style='color:#888'>N/A</span>";
  return String(v, decimals);
}

// ─────────────────────────────────────────────
//  Web page HTML
// ─────────────────────────────────────────────
void handleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta http-equiv="refresh" content="10">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Farm Sensor Dashboard</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: Arial, sans-serif; background: #1a1a2e; color: #eee; padding: 20px; }
    h1 { text-align: center; color: #00d4ff; margin-bottom: 6px; font-size: 1.6em; }
    .subtitle { text-align: center; color: #888; font-size: 0.85em; margin-bottom: 20px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 16px; }
    .card { background: #16213e; border-radius: 12px; padding: 18px; border-left: 4px solid #00d4ff; }
    .card.soil  { border-color: #8bc34a; }
    .card.air   { border-color: #03a9f4; }
    .card.dist  { border-color: #ff9800; }
    .card.lora  { border-color: #9c27b0; }
    .card h2 { font-size: 1em; margin-bottom: 12px; color: #ccc; letter-spacing: 1px; text-transform: uppercase; }
    .row { display: flex; justify-content: space-between; padding: 6px 0; border-bottom: 1px solid #2a2a4a; }
    .row:last-child { border-bottom: none; }
    .label { color: #aaa; font-size: 0.9em; }
    .value { font-weight: bold; font-size: 1em; color: #fff; }
    .status { text-align: center; margin-top: 20px; color: #555; font-size: 0.8em; }
    .badge { display: inline-block; background: #00d4ff22; color: #00d4ff; border-radius: 20px; padding: 2px 12px; font-size: 0.8em; margin-left: 8px; }
  </style>
</head>
<body>
  <h1> Sensor Dashboard </h1>
  <p class="subtitle">Auto-refreshes every 10s &nbsp;|&nbsp; Packets received: )rawhtml";

  html += String(latest.packet_count);
  html += R"rawhtml( &nbsp;|&nbsp; Last update: )rawhtml";
  html += latest.last_received;
  html += R"rawhtml(</p>

  <div class="grid">

    <div class="card soil">
      <h2>🌾 LilyGO 1 — Soil Sensor</h2>
      <div class="row"><span class="label">Soil Moisture</span>   <span class="value">)rawhtml";
  html += fmt(latest.soil_moisture); html += " %</span></div>";
  html += "<div class='row'><span class='label'>Soil Temperature</span><span class='value'>";
  html += fmt(latest.soil_temperature); html += " °C</span></div>";
  html += "<div class='row'><span class='label'>Conductivity</span><span class='value'>";
  html += fmt(latest.conductivity); html += " µS/cm</span></div>";
  html += "<div class='row'><span class='label'>pH</span><span class='value'>";
  html += fmt(latest.ph); html += "</span></div></div>";

  html += "<div class='card air'><h2>💧 LilyGO 2 — SHT31 Temp & Humidity</h2>";
  html += "<div class='row'><span class='label'>Air Temperature</span><span class='value'>";
  html += fmt(latest.air_temperature); html += " °C</span></div>";
  html += "<div class='row'><span class='label'>Humidity</span><span class='value'>";
  html += fmt(latest.humidity); html += " %</span></div></div>";

  html += "<div class='card dist'><h2>📏 LilyGO 3 — Ultrasonic</h2>";
  html += "<div class='row'><span class='label'>Distance</span><span class='value'>";
  html += fmt(latest.distance, 3); html += " m</span></div></div>";

  html += "<div class='card lora'><h2>📡 LoRa Link Quality</h2>";
  html += "<div class='row'><span class='label'>RSSI</span><span class='value'>";
  html += String(latest.lora_rssi); html += " dBm</span></div>";
  html += "<div class='row'><span class='label'>SNR</span><span class='value'>";
  html += String(latest.lora_snr, 2); html += " dB</span></div></div>";

  html += "</div><p class='status'>Heltec 2 &nbsp;|&nbsp; IP: ";
  html += WiFi.localIP().toString();
  html += "</p></body></html>";

  server.send(200, "text/html", html);
}

// ─────────────────────────────────────────────
//  WiFi connect
// ─────────────────────────────────────────────
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - start > 15000) {
      Serial.println("\n[WiFi] Timeout — will retry");
      return;
    }
  }
  Serial.printf("\n[WiFi] Connected!\n");
  Serial.printf("[WiFi] >>> Open browser: http://%s <<<\n", WiFi.localIP().toString().c_str());
}

// ─────────────────────────────────────────────
//  Print dashboard to Serial
// ─────────────────────────────────────────────
void printSerial(int rssi, float snr) {
  Serial.println("\n+======================================================+");
  Serial.printf( "  HELTEC 2 DASHBOARD  |  Packet #%d\n", latest.packet_count);
  Serial.printf( "  LoRa RSSI: %d dBm   |  SNR: %.2f dB\n", rssi, snr);
  Serial.println("+======================================================+");
  Serial.println("  [LilyGO1 - Soil Sensor] ------------------------------");
  Serial.printf( "    Soil Moisture   : %s %%\n",    latest.soil_moisture    >= 0 ? String(latest.soil_moisture, 2).c_str()    : "N/A");
  Serial.printf( "    Soil Temperature: %s C\n",     latest.soil_temperature >= 0 ? String(latest.soil_temperature, 2).c_str() : "N/A");
  Serial.printf( "    Conductivity    : %s uS/cm\n", latest.conductivity     >= 0 ? String(latest.conductivity, 2).c_str()     : "N/A");
  Serial.printf( "    pH              : %s\n",       latest.ph               >= 0 ? String(latest.ph, 2).c_str()               : "N/A");
  Serial.println("+------------------------------------------------------+");
  Serial.println("  [LilyGO2 - SHT31 Temp & Humidity] --------------------");
  Serial.printf( "    Air Temperature : %s C\n",     latest.air_temperature  >= 0 ? String(latest.air_temperature, 2).c_str()  : "N/A");
  Serial.printf( "    Humidity        : %s %%\n",    latest.humidity         >= 0 ? String(latest.humidity, 2).c_str()         : "N/A");
  Serial.println("+------------------------------------------------------+");
  Serial.println("  [LilyGO3 - Ultrasonic Distance] ----------------------");
  Serial.printf( "    Distance        : %s m\n",     latest.distance         >= 0 ? String(latest.distance, 3).c_str()         : "N/A");
  Serial.println("+======================================================+");
}

// ─────────────────────────────────────────────
//  setup()
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n+======================================================+");
  Serial.println("      HELTEC 2 — LoRa Receiver + Web Server");
  Serial.println("      Board: Heltec Wireless Stick V3 (SX1262)");
  Serial.println("+======================================================+");

  WiFi.mode(WIFI_STA);
  Serial.printf("[WiFi]  MAC: %s\n", WiFi.macAddress().c_str());
  connectWiFi();

  server.on("/", handleRoot);
  server.begin();
  Serial.println("[Web]   Server started on port 80");

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  radio.setDio2AsRfSwitch(true);
  int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SYNC, LORA_PWR);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LoRa]  Init FAILED  code=%d — halting\n", state);
    while (true) delay(1000);
  }
  Serial.printf("[LoRa]  Init OK  %.1f MHz  BW=%.0f  SF=%d\n",
                LORA_FREQ, LORA_BW, LORA_SF);

  Serial.println("\n[System] Ready — open browser at your IP address above\n");
}

// ─────────────────────────────────────────────
//  loop()
// ─────────────────────────────────────────────
void loop() {
  server.handleClient();

  String received = "";
  int state = radio.receive(received, 3000);  // 3s timeout so web server stays responsive

  if (state == RADIOLIB_ERR_NONE) {
    latest.packet_count++;
    latest.lora_rssi = (int)radio.getRSSI();
    latest.lora_snr  = radio.getSNR();

    // Timestamp
    unsigned long s = millis() / 1000;
    char ts[20];
    snprintf(ts, sizeof(ts), "%02lu:%02lu:%02lu", s/3600, (s%3600)/60, s%60);
    latest.last_received = String(ts);

    Serial.printf("\n[LoRa] Packet #%d  RSSI=%d dBm  SNR=%.2f dB\n",
                  latest.packet_count, latest.lora_rssi, latest.lora_snr);
    Serial.printf("[LoRa] Raw: %s\n", received.c_str());

    if (parsePayload(received)) {
      printSerial(latest.lora_rssi, latest.lora_snr);
      Serial.printf("[Web]  Dashboard updated — http://%s\n",
                    WiFi.localIP().toString().c_str());
    }
  }
  else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.printf("[LoRa] RX error code=%d\n", state);
  }

  if (WiFi.status() != WL_CONNECTED) connectWiFi();
}