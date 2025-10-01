// AP IP: 192.168.4.1
// AP PW: 12345678
// firmware update: version.txt im repo anpassen UND hier currentVersion = ".....""

// -------------------- Includes --------------------
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ezTime.h>
#include <NeoPixelBus.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <Update.h>

// -------------------- LED Setup --------------------
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(60, 13);

uint8_t watchr = 50, watchg = 50, watchb = 255;
uint8_t trennr = 255, trenng = 50, trennb = 50;

uint32_t start_time = 0, stop_time = 86400;  // 0-86400 Sekunden des Tages
bool auto_mode = true;
bool watchmode = true;
uint8_t oldsecond;

// -------------------- WiFi & Server --------------------
Preferences prefs;
WebServer server(80);

String ssid, pass;
Timezone myTZ;
unsigned long lastSync = 0;  // globale Variable für NTP-Sync

// -------------------- OTA Update --------------------
const char* fwVersionURL = "https://tobiascrackz.github.io/RheinturmUhr/version.txt";
const char* fwBinURL     = "https://tobiascrackz.github.io/RheinturmUhr/firmware.bin";
String currentVersion = "v1.0.1";

unsigned long lastUpdateCheck = 0;
bool firstUpdateCheckDone = false;

// -------------------- Funktionen für OTA --------------------
bool checkForUpdate() {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.begin(fwVersionURL);
  int httpCode = http.GET();
  if (httpCode == 200) {
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

  HTTPClient http;
  http.begin(fwBinURL);
  int httpCode = http.GET();
  if (httpCode == 200) {
    int contentLength = http.getSize();
    WiFiClient* stream = http.getStreamPtr();

    Serial.printf("OTA startet, Größe: %d Bytes\n", contentLength);

    if (Update.begin(contentLength)) {
      size_t written = Update.writeStream(*stream);
      if (written == contentLength) {
        Serial.println("Firmware komplett geschrieben.");
      } else {
        Serial.printf("Geschrieben: %d von %d Bytes\n", written, contentLength);
      }

      if (Update.end()) {
        if (Update.isFinished()) {
          Serial.println("Update abgeschlossen. Neustart...");
          currentVersion = ""; // optional
          ESP.restart();
          return true;
        } else {
          Serial.println("Update unvollständig!");
        }
      } else {
        Serial.printf("Update Fehler: %s\n", Update.errorString());
      }
    } else {
      Serial.println("Nicht genug Speicherplatz für Update!");
    }
  } else {
    Serial.printf("HTTP Fehler: %d\n", httpCode);
  }
  http.end();
  return false;
}

void checkPeriodicUpdate() {
  unsigned long now = millis();

  // 7 Sekunden nach Start
  if (!firstUpdateCheckDone && now > 7000) {
    firstUpdateCheckDone = true;
    Serial.println("Erster Software-Check 7 Sekunden nach Start");
    if (checkForUpdate()) performOTA();
  }

  // alle 6 Stunden prüfen
  if (now - lastUpdateCheck > 21600000) {
    lastUpdateCheck = now;
    Serial.println("Regelmäßiger Software-Check (6h)");
    if (checkForUpdate()) performOTA();
  }
}

// -------------------- Farb- & Zeitfunktionen --------------------
void saveColors() {
  prefs.begin("colors", false);
  prefs.putUChar("watchr", watchr);
  prefs.putUChar("watchg", watchg);
  prefs.putUChar("watchb", watchb);
  prefs.putUChar("trennr", trennr);
  prefs.putUChar("trenng", trenng);
  prefs.putUChar("trennb", trennb);
  prefs.end();
  Serial.println("Farben gespeichert.");
}

void loadColors() {
  prefs.begin("colors", true);
  if (prefs.isKey("watchr")) watchr = prefs.getUChar("watchr");
  if (prefs.isKey("watchg")) watchg = prefs.getUChar("watchg");
  if (prefs.isKey("watchb")) watchb = prefs.getUChar("watchb");
  if (prefs.isKey("trennr")) trennr = prefs.getUChar("trennr");
  if (prefs.isKey("trenng")) trenng = prefs.getUChar("trenng");
  if (prefs.isKey("trennb")) trennb = prefs.getUChar("trennb");
  prefs.end();
  Serial.printf("Farben geladen: Uhr RGB(%d,%d,%d), Trenn RGB(%d,%d,%d)\n",
                watchr, watchg, watchb, trennr, trenng, trennb);
}

void saveTimes(uint32_t start, uint32_t stop) {
  prefs.begin("times", false);
  prefs.putUInt("start_time", start);
  prefs.putUInt("stop_time", stop);
  prefs.end();
  Serial.println("Start/Stop-Zeit gespeichert.");
}

bool loadTimes() {
  prefs.begin("times", true);
  if (prefs.isKey("start_time") && prefs.isKey("stop_time")) {
    start_time = prefs.getUInt("start_time");
    stop_time = prefs.getUInt("stop_time");
    prefs.end();
    Serial.printf("Geladene Zeiten: Start=%u, Stop=%u\n", start_time, stop_time);
    return true;
  }
  prefs.end();
  return false;
}

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

  Serial.printf("Uhrzeit: %02d:%02d:%02d | Uhrfarbe RGB(%d,%d,%d) | Trennfarbe RGB(%d,%d,%d)\n",
                h, m, s, watchr, watchg, watchb, trennr, trenng, trennb);
}

void handlestripdisplay() {
  if (oldsecond != second()) {
    uint32_t current_time = myTZ.hour() * 3600 + myTZ.minute() * 60 + myTZ.second();
    if (auto_mode) {
      if (current_time < start_time || current_time > stop_time) {
        cleanstrip();
        Serial.println("Strip aus (Auto-Mode Zeitfenster)");
      } else {
        showtime();
      }
    } else {
      if (watchmode) {
        showtime();
      } else {
        cleanstrip();
        Serial.println("Strip aus (Watchmode OFF)");
      }
    }
    oldsecond = second();
  }
}

// -------------------- WiFi Setup --------------------
void saveWiFi(const char* s, const char* p) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", s);
  prefs.putString("pass", p);
  prefs.end();
  Serial.println("WLAN-Daten gespeichert.");
}

bool loadWiFi() {
  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();
  if (ssid.length() > 0) {
    Serial.println("Gefundene WLAN-Daten: SSID=" + ssid);
    return true;
  }
  return false;
}

void setupAP() {
  WiFi.mode(WIFI_AP);
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP("RheinturmUhrSetup", "12345678");
  Serial.println("AP gestartet: ESP32-UhrSetup IP: " + WiFi.softAPIP().toString());

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

  server.onNotFound([]() {
    server.send(200, "text/html", "<h1>RheinturmUhr Setup</h1><p><a href='/'>Setup hier</a></p>");
  });

  server.begin();
  Serial.println("Webserver (AP) gestartet.");
}

// -------------------- User-Webseite nach WLAN-Connect --------------------
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
