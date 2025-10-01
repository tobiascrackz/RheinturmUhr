// -------------------- ESP8266 Version --------------------
// AP IP: 192.168.4.1
// AP PW: 12345678
// Firmware Update: version.txt im Repo anpassen UND hier currentVersion ändern

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ezTime.h>
#include <NeoPixelBus.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

// -------------------- LED Setup --------------------
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(60, 13);

uint8_t watchr = 50, watchg = 50, watchb = 255;
uint8_t trennr = 255, trenng = 50, trennb = 50;

uint32_t start_time = 0, stop_time = 86400;  // Sekunden des Tages
bool auto_mode = true;
bool watchmode = true;
uint8_t oldsecond;

// -------------------- WiFi & Server --------------------
ESP8266WebServer server(80);

String ssid, pass;
Timezone myTZ;
unsigned long lastSync = 0;

// -------------------- OTA Update --------------------
const char* fwVersionURL = "https://tobiascrackz.github.io/RheinturmUhr/version_8266.txt";
const char* fwBinURL     = "https://tobiascrackz.github.io/RheinturmUhr/firmware_8266.bin";
String currentVersion = "v1.0.1";

unsigned long lastUpdateCheck = 0;
bool firstUpdateCheckDone = false;

#define EEPROM_SIZE 512

// -------------------- EEPROM Speicherfunktionen --------------------
void saveColors() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(0, watchr);
  EEPROM.write(1, watchg);
  EEPROM.write(2, watchb);
  EEPROM.write(3, trennr);
  EEPROM.write(4, trenng);
  EEPROM.write(5, trennb);
  EEPROM.commit();
  Serial.println("Farben gespeichert.");
}

void loadColors() {
  EEPROM.begin(EEPROM_SIZE);
  watchr = EEPROM.read(0);
  watchg = EEPROM.read(1);
  watchb = EEPROM.read(2);
  trennr = EEPROM.read(3);
  trenng = EEPROM.read(4);
  trennb = EEPROM.read(5);
  EEPROM.end();
  Serial.printf("Farben geladen: Uhr RGB(%d,%d,%d), Trenn RGB(%d,%d,%d)\n",
                watchr, watchg, watchb, trennr, trenng, trennb);
}

void saveTimes(uint32_t start, uint32_t stop) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(10, start);
  EEPROM.put(14, stop);
  EEPROM.commit();
  Serial.println("Start/Stop-Zeit gespeichert.");
}

bool loadTimes() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(10, start_time);
  EEPROM.get(14, stop_time);
  EEPROM.end();
  if (start_time > 0 && stop_time > 0) {
    Serial.printf("Geladene Zeiten: Start=%u, Stop=%u\n", start_time, stop_time);
    return true;
  }
  return false;
}

void saveWiFi(const char* s, const char* p) {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 64; i++) EEPROM.write(20 + i, (i < strlen(s)) ? s[i] : 0);
  for (int i = 0; i < 64; i++) EEPROM.write(100 + i, (i < strlen(p)) ? p[i] : 0);
  EEPROM.commit();
  Serial.println("WLAN-Daten gespeichert.");
}

bool loadWiFi() {
  char s[65], p[65];
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 64; i++) s[i] = EEPROM.read(20 + i);
  for (int i = 0; i < 64; i++) p[i] = EEPROM.read(100 + i);
  s[64] = 0; p[64] = 0;
  ssid = String(s);
  pass = String(p);
  EEPROM.end();
  if (ssid.length() > 0) {
    Serial.println("Gefundene WLAN-Daten: SSID=" + ssid);
    return true;
  }
  return false;
}

// -------------------- OTA Update --------------------
bool checkForUpdate() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, fwVersionURL)) {
    Serial.println("Fehler bei http.begin()");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String newVersion = http.getString();
    newVersion.trim();
    Serial.println("Gefundene Version: " + newVersion);
    if (newVersion != currentVersion) {
      Serial.println("Neue Firmware verfügbar!");
      http.end();
      return true;
    }
  } else {
    Serial.printf("Fehler beim Version-Check: %d\n", httpCode);
  }
  http.end();
  return false;
}

bool performOTA() {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  WiFiClient client;
  t_httpUpdate_return ret = ESPhttpUpdate.update(client, fwBinURL, currentVersion);
  
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("Update fehlgeschlagen: %s\n", ESPhttpUpdate.getLastErrorString().c_str());
      return false;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("Keine Updates verfügbar.");
      return false;
    case HTTP_UPDATE_OK:
      Serial.println("Update erfolgreich!");
      return true;
  }
  return false;
}

void checkPeriodicUpdate() {
  unsigned long now = millis();
  if (!firstUpdateCheckDone && now > 7000) {
    firstUpdateCheckDone = true;
    Serial.println("Erster Software-Check 7 Sekunden nach Start");
    if (checkForUpdate()) performOTA();
  }
  if (now - lastUpdateCheck > 21600000) { // 6h
    lastUpdateCheck = now;
    Serial.println("Regelmäßiger Software-Check (6h)");
    if (checkForUpdate()) performOTA();
  }
}

// -------------------- LED Funktionen --------------------
void cleanstrip() {
  for (int i = 0; i < 60; i++) strip.SetPixelColor(i, RgbColor(0, 0, 0));
  strip.Show();
}

void showtrennled() {
  RgbColor color(trennr, trenng, trennb);
  strip.SetPixelColor(20, color);
  strip.SetPixelColor(21, color);
  strip.SetPixelColor(42, color);
  strip.SetPixelColor(43, color);
}

void showtime() {
  cleanstrip();
  int s = myTZ.second(), m = myTZ.minute(), h = myTZ.hour();
  for (int i = 1; i <= s % 10; i++) strip.SetPixelColor(i, RgbColor(watchr, watchg, watchb));
  for (int i = 14; i <= 13 + s / 10; i++) strip.SetPixelColor(i, RgbColor(watchr, watchg, watchb));
  for (int i = 23; i <= 22 + m % 10; i++) strip.SetPixelColor(i, RgbColor(watchr, watchg, watchb));
  for (int i = 36; i <= 35 + m / 10; i++) strip.SetPixelColor(i, RgbColor(watchr, watchg, watchb));
  for (int i = 45; i <= 44 + h % 10; i++) strip.SetPixelColor(i, RgbColor(watchr, watchg, watchb));
  for (int i = 58; i <= 57 + h / 10; i++) strip.SetPixelColor(i, RgbColor(watchr, watchg, watchb));
  showtrennled();
  strip.Show();
}

void handlestripdisplay() {
  if (oldsecond != second()) {
    uint32_t current_time = myTZ.hour() * 3600 + myTZ.minute() * 60 + myTZ.second();
    if (auto_mode) {
      if (current_time < start_time || current_time > stop_time) {
        cleanstrip();
      } else {
        showtime();
      }
    } else {
      if (watchmode) showtime(); else cleanstrip();
    }
    oldsecond = second();
  }
}

// -------------------- WiFi AP Setup --------------------
void setupAP() {
  WiFi.mode(WIFI_AP);
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP("RheinturmUhrSetup", "12345678");
  Serial.println("AP gestartet: ESP8266-UhrSetup IP: " + WiFi.softAPIP().toString());

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html",
                "<h1>WLAN Setup</h1>"
                "<form method='POST' action='/save'>"
                "SSID: <input name='ssid'><br>"
                "Passwort: <input name='pass' type='password'><br>"
                "<input type='submit' value='Speichern'></form>");
  });

  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("ssid") && server.hasArg("pass")) {
      saveWiFi(server.arg("ssid").c_str(), server.arg("pass").c_str());
      server.send(200, "text/html", "Gespeichert! Neustart...");
      delay(2000);
      ESP.restart();
    } else {
      server.send(400, "text/html", "Fehlende Daten!");
    }
  });

  server.begin();
}

// -------------------- User-Webseite --------------------
void setupUserWeb() {
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Rheinturm Uhr Steuerung</title></head><body style='font-family:Arial;color:white;background-color:black;'>";
    html += "<h1 style='color:white;'>RheinturmUhr Steuerung</h1>";
    html += "<p>Aktuelle Uhrzeit: <span id='currentTime'>--:--:--</span></p>";
    html += "<p>Startzeit: <span id='startTimeDisplay'>--:--</span></p>";
    html += "<p>Stopzeit: <span id='stopTimeDisplay'>--:--</span></p>";
    html += "<form id='timeForm'>Start <input type='time' name='start'> Stop <input type='time' name='stop'><input type='submit' value='Speichern'></form>";
    html += "<p>Uhrfarbe: <input type='color' id='watchColor'></p>";
    html += "<p>Trennfarbe: <input type='color' id='trennColor'></p>";

    html += R"rawliteral(
<script>
function rgbToHex(r,g,b){return"#"+((1<<24)+(r<<16)+(g<<8)+b).toString(16).slice(1);}
function secToHHMM(sec){let h=Math.floor(sec/3600);let m=Math.floor((sec%3600)/60);return (h<10?"0":"")+h+":"+(m<10?"0":"")+m;}
function updateStatus(){fetch('/status').then(r=>r.json()).then(data=>{
document.getElementById('currentTime').textContent=data.time;
document.getElementById('watchColor').value=rgbToHex(data.watchr,data.watchg,data.watchb);
document.getElementById('trennColor').value=rgbToHex(data.trennr,data.trenng,data.trennb);
document.getElementById('startTimeDisplay').textContent=secToHHMM(data.start_time);
document.getElementById('stopTimeDisplay').textContent=secToHHMM(data.stop_time);});}
document.addEventListener('DOMContentLoaded',()=>{
document.getElementById('watchColor').addEventListener('input',()=>{
const c=document.getElementById('watchColor').value;
fetch('/setWatch?r='+parseInt(c.substr(1,2),16)+'&g='+parseInt(c.substr(3,2),16)+'&b='+parseInt(c.substr(5,2),16));
});
document.getElementById('trennColor').addEventListener('input',()=>{
const c=document.getElementById('trennColor').value;
fetch('/setTrenn?r='+parseInt(c.substr(1,2),16)+'&g='+parseInt(c.substr(3,2),16)+'&b='+parseInt(c.substr(5,2),16));
});
document.getElementById('timeForm').addEventListener('submit',e=>{
e.preventDefault();
const formData=new FormData(e.target);
fetch('/times?start='+formData.get('start')+'&stop='+formData.get('stop'),{method:'POST'}).then(r=>r.text()).then(a=>alert(a));
});
updateStatus(); setInterval(updateStatus,1000);
});
</script>
)rawliteral";

    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/status", HTTP_GET, []() {
    String json = "{";
    json += "\"time\":\"" + String(myTZ.hour()) + ":" + String(myTZ.minute()) + ":" + String(myTZ.second()) + "\",";
    json += "\"watchr\":" + String(watchr) + ",";
    json += "\"watchg\":" + String(watchg) + ",";
    json += "\"watchb\":" + String(watchb) + ",";
    json += "\"trennr\":" + String(trennr) + ",";
    json += "\"trenng\":" + String(trenng) + ",";
    json += "\"trennb\":" + String(trennb) + ",";
    json += "\"start_time\":" + String(start_time) + ",";
    json += "\"stop_time\":" + String(stop_time);
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/setWatch", HTTP_GET, []() {
    watchr = server.arg("r").toInt();
    watchg = server.arg("g").toInt();
    watchb = server.arg("b").toInt();
    saveColors();
    server.send(200, "text/plain", "OK");
  });

  server.on("/setTrenn", HTTP_GET, []() {
    trennr = server.arg("r").toInt();
    trenng = server.arg("g").toInt();
    trennb = server.arg("b").toInt();
    saveColors();
    server.send(200, "text/plain", "OK");
  });

  server.on("/times", HTTP_POST, []() {
    if (server.hasArg("start") && server.hasArg("stop")) {
      int sh = server.arg("start").substring(0, 2).toInt();
      int sm = server.arg("start").substring(3, 5).toInt();
      int eh = server.arg("stop").substring(0, 2).toInt();
      int em = server.arg("stop").substring(3, 5).toInt();
      start_time = sh * 3600 + sm * 60;
      stop_time = eh * 3600 + em * 60;
      auto_mode = true;
      saveTimes(start_time, stop_time);
      server.send(200, "text/html", "Zeiten gespeichert");
    } else server.send(400, "text/html", "Fehlerhafte Eingabe");
  });

  server.onNotFound([]() { server.send(404, "text/plain", "Not Found"); });
  server.begin();
  Serial.println("Webserver (User) gestartet.");
}

// -------------------- OTA Setup --------------------
void OTA_setup() {
  ArduinoOTA.setHostname("RheinturmUhr");
  ArduinoOTA.begin();
  Serial.println("OTA bereit.");
}

// -------------------- Setup & Loop --------------------
void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  loadTimes();
  loadColors();
  strip.Begin();
  strip.Show();

  if (loadWiFi()) {
    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.println("Verbinde mit WLAN...");
    if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      Serial.println("Verbunden: " + WiFi.localIP().toString());
      myTZ.setLocation(F("Europe/Berlin"));
      waitForSync();
      setupUserWeb();
      OTA_setup();
    } else {
      Serial.println("WLAN-Verbindung fehlgeschlagen.");
      setupAP();
    }
  } else {
    setupAP();
  }
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();
  handlestripdisplay();
  checkPeriodicUpdate();
  if (millis() - lastSync > 3600000) {
    waitForSync();
    lastSync = millis();
    Serial.println("Uhrzeit erneut synchronisiert");
  }
}
