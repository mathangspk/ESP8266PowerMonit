#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <EasyButton.h>
#include <PubSubClient.h>
#include <PZEM004Tv30.h>
PZEM004Tv30 pzem(5, 4);

// Arduino pin where the button is connected to.
#define BUTTON_PIN 0
// Instance of the button.
EasyButton button(BUTTON_PIN);
const IPAddress apIP(10 , 1 , 23 , 1);
const char* apSSID = "ESP8266_SETUP";
boolean settingMode;
String ssidList;
String localIpp;
#define mqtt_server "192.168.1.99"
const uint16_t mqtt_port = 1883; //Port của CloudMQTT TCP
String email = "";
String mqtt = "";
String sn = "";
DNSServer dnsServer;
ESP8266WebServer webServer(80);
WiFiClient espClient;
PubSubClient client(espClient);
const int ledPin =  LED_BUILTIN;// the number of the LED pin
// Variables will change:
int ledState = LOW;             // ledState used to set the LED
// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated
int state = 0;
int countCycle = 0;
void setup() {
  pinMode(ledPin, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  // Initialize the button.
  button.begin();
  // Add the callback function to be called when the button is pressed.
  button.onPressed(onPressed);
  Serial.begin(115200);
  EEPROM.begin(512);
  delay(10);
  if (restoreConfig()) {
    if (checkConnection()) {
      settingMode = false;
      startWebServer();
      return;
    }
  }
  settingMode = true;
  setupMode();
}

void blinkLedSettingMode(const long interval) {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    digitalWrite(ledPin, ledState);
  }
}

void blinkLedConnectedRouter(const long interval) {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    switch (state) {
      case 0:
        countCycle++;
        if (countCycle == 5) {
          ledState = HIGH;
          countCycle = 0;
          state = 1;
        }
      case 1:
        countCycle++;
        if (ledState == LOW) {
          ledState = HIGH;
        } else {
          ledState = LOW;
        }
        if (countCycle == 5) {
          ledState = LOW;
          countCycle = 0;
          state = 0;
        }
    }
    // set the LED with the ledState of the variable:
    digitalWrite(ledPin, ledState);
  }
}

String genStringRandom(int numBytes) {
  int i = 0;
  int j = 0;
  String letters[40] = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
  String randString = "";
  for (i = 0; i < numBytes; i++)
  {
    randString = randString + letters[random(0, 40)];
  }
  return randString;
}


// Hàm call back để nhận dữ liệu
void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Co tin nhan moi tu topic:");
  Serial.println(topic);
  for (int i = 0; i < length; i++)
    Serial.print((char)payload[i]);
  Serial.println();
}
// Hàm reconnect thực hiện kết nối lại khi mất kết nối với MQTT Broker
void reconnect()
{
  while (!client.connected()) // Chờ tới khi kết nối
  {
    // Thực hiện kết nối với mqtt user và pass
    if (client.connect(sn.c_str(), "ESP_offline", 0, 0, "ESP8266_id1_offline")) //kết nối vào broker
    {
      Serial.println("Đã kết nối to MQTT server");
      client.subscribe("ESP8266_read_data"); //đăng kí nhận dữ liệu từ topic ESP8266_read_data
    }
    else
    {
      Serial.print("Lỗi:, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Đợi 5s
      button.read();
      delay(5000);
    }
  }
}

void pubData() {
  float voltage = pzem.voltage();
  if (!isnan(voltage)) {
    Serial.print("Voltage: "); Serial.print(voltage); Serial.println("V");
  } else {
    Serial.println("Error reading voltage");
  }

  float current = pzem.current();
  if (!isnan(current)) {
    Serial.print("Current: "); Serial.print(current); Serial.println("A");
  } else {
    Serial.println("Error reading current");
  }

  float power = pzem.power();
  if (!isnan(power)) {
    Serial.print("Power: "); Serial.print(power); Serial.println("W");
  } else {
    Serial.println("Error reading power");
  }

  float energy = pzem.energy();
  if (!isnan(energy)) {
    Serial.print("Energy: "); Serial.print(energy, 3); Serial.println("kWh");
  } else {
    Serial.println("Error reading energy");
  }

  float frequency = pzem.frequency();
  if (!isnan(frequency)) {
    Serial.print("Frequency: "); Serial.print(frequency, 1); Serial.println("Hz");
  } else {
    Serial.println("Error reading frequency");
  }

  float pf = pzem.pf();
  if (!isnan(pf)) {
    Serial.print("PF: "); Serial.println(pf);
  } else {
    Serial.println("Error reading power factor");
  }

  Serial.println();
  String array[6] = {};
  String volt = String(voltage, 1);
  String cur = String(current, 2);
  String po = String(power, 2);
  String en = String(energy, 3);
  String f = String(frequency, 1);
  String pfS = String(pf, 2);
  array[0] = volt;
  array[1] = cur;
  array[2] = po;
  array[3] = en;
  array[4] = f;
  array[5] = pfS;
  String result = "";
  for (int i = 0; i < 6; i++) {
    result.concat(array[i]);
    result.concat("-");
  }
  client.publish(localIpp.c_str(), result.c_str()); // gửi dữ liệu lên topic ESP8266_sent_data
}

void loop() {
  if (settingMode) {
    dnsServer.processNextRequest();
    //blinkLedSettingMode(500);
    blinkLedConnectedRouter(200);
  } else {
    button.read();
    pubData();
    delay(2000);
    digitalWrite(ledPin, !digitalRead(ledPin));
    if (!client.connected())// Kiểm tra kết nối
      reconnect();
    client.loop();
  }
  webServer.handleClient();
}

void onPressed() {
  for (int i = 0; i < 120; ++i) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("EEPROM is cleaned!");
}
boolean restoreConfig() {
  Serial.println("Reading EEPROM...");
  String ssid = "";
  String pass = "";
  if (EEPROM.read(0) != 0) {
    for (int i = 0; i < 32; ++i) {
      ssid += char(EEPROM.read(i));
    }
    Serial.print("SSID: ");
    Serial.println(ssid);
    for (int i = 32; i < 64; ++i) {
      pass += char(EEPROM.read(i));
    }
    Serial.print("Password: ");
    Serial.println(pass);
    for (int i = 64; i < 100; ++i) {
      email += char(EEPROM.read(i));
    }
    Serial.print("Email: ");
    Serial.println(email);
     for (int i = 100; i < 120; ++i) {
      mqtt += char(EEPROM.read(i));
    }
    Serial.print("MQTT: ");
    Serial.println(mqtt);
     for (int i = 117; i < 130; ++i) {
      sn += char(EEPROM.read(i));
    }
    Serial.print("Serial Number: ");
    Serial.println(sn);
    WiFi.begin(ssid.c_str(), pass.c_str());
    return true;
  }
  else {
    Serial.println("Config not found.");
    return false;
  }
}

boolean checkConnection() {
  int count = 0;
  Serial.print("Waiting for Wi-Fi connection");
  while ( count < 30 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.println("Connected!");
      return (true);
    }
    delay(500);
    Serial.print(".");
    count++;
  }
  Serial.println("Timed out.");
  return false;
}

void startWebServer() {
  if (settingMode) {
    Serial.print("Starting Web Server at ");
    Serial.println(WiFi.softAPIP());
    webServer.on("/settings", []() {
      String s = "<h1>Wi-Fi Settings</h1><p>Please enter your password by selecting the SSID.</p>";
      s += "<form method=\"get\" action=\"setap\"><label>SSID: </label><select name=\"ssid\">";
      s += ssidList;
      s += "</select><br>Email: <input name=\"email\" length=64 type=\"text\">";
      s += "</select><br>MQTT server: <input name=\"mqtt\" length=64 type=\"text\">";
      s += "</select><br>SerialNumber: <input name=\"sn\" length=64 type=\"text\">";
      s += "<br>Password: <input name=\"pass\" length=64 type=\"password\"><input type=\"submit\"></form>";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
    });
    webServer.on("/setap", []() {
      for (int i = 0; i < 130; ++i) {
        EEPROM.write(i, 0);
      }
      String ssid = urlDecode(webServer.arg("ssid"));
      Serial.print("SSID: ");
      Serial.println(ssid);
      String pass = urlDecode(webServer.arg("pass"));
      Serial.print("Password: ");
      Serial.println(pass);
      String mqtt = urlDecode(webServer.arg("mqtt"));
      Serial.print("MQTT: ");
      Serial.println(mqtt);
      String sn = urlDecode(webServer.arg("sn"));
      Serial.print("Serial Number: ");
      Serial.println(sn);
      String email = urlDecode(webServer.arg("email"));
      email.concat("-");
      email.concat(genStringRandom(10));
      Serial.print("Email: ");
      Serial.println(email);
      Serial.println("Writing SSID to EEPROM...");
      for (int i = 0; i < ssid.length(); ++i) {
        EEPROM.write(i, ssid[i]);
      }
      Serial.println("Writing Password to EEPROM...");
      for (int i = 0; i < pass.length(); ++i) {
        EEPROM.write(32 + i, pass[i]);
      }
      Serial.println("Writing Email to EEPROM...");
      for (int i = 0; i < email.length(); ++i) {
        EEPROM.write(64 + i, email[i]);
      }
      Serial.println("Writing MQTT to EEPROM...");
      for (int i = 0; i < mqtt.length(); ++i) {
        EEPROM.write(100 + i, mqtt[i]);
      }
      Serial.println("Writing Serial Number to EEPROM...");
      for (int i = 0; i < sn.length(); ++i) {
        EEPROM.write(117 + i, sn[i]);
      }
      EEPROM.commit();
      Serial.println("Write EEPROM done!");
      String s = "<h1>Setup complete.</h1><p>device will be connected to \"";
      s += ssid;
      s += "\" after the restart.";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
      ESP.restart();
    });
    webServer.onNotFound([]() {
      String s = "<h1>AP mode</h1><p><a href=\"/settings\">Wi-Fi Settings</a></p>";
      webServer.send(200, "text/html", makePage("AP mode", s));
    });
  }
  else {
    //localIpp.concat(WiFi.localIP().toString().c_str());
    //localIpp.concat("-");
    localIpp.concat(sn.c_str());
    Serial.print("Starting Web Server at ");
    Serial.println(WiFi.localIP());
    Serial.println(localIpp);
    client.setServer(mqtt.c_str(), mqtt_port);
    client.setCallback(callback);
    webServer.on("/", []() {
      String s = "<h1>STA mode</h1><p><a href=\"/reset\">Reset Wi-Fi Settings</a></p>";
      webServer.send(200, "text/html", makePage("STA mode", s));
    });
    webServer.on("/reset", []() {
      for (int i = 0; i < 130; ++i) {
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
      String s = "<h1>Wi-Fi settings was reset.</h1><p>Please reset device.</p>";
      webServer.send(200, "text/html", makePage("Reset Wi-Fi Settings", s));
    });
  }
  webServer.begin();
}

void setupMode() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  delay(100);
  Serial.println("");
  for (int i = 0; i < n; ++i) {
    ssidList += "<option value=\"";
    ssidList += WiFi.SSID(i);
    ssidList += "\">";
    ssidList += WiFi.SSID(i);
    ssidList += "</option>";
  }
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSSID);
  dnsServer.start(53, "*", apIP);
  startWebServer();
  Serial.print("Starting Access Point at \"");
  Serial.print(apSSID);
  Serial.println("\"");
}

String makePage(String title, String contents) {
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += contents;
  s += "</body></html>";
  return s;
}

String urlDecode(String input) {
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}
