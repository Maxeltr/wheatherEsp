#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <GyverDS18.h>
#include "DHTStable.h"
#include <Adafruit_BMP085.h>

// Replace with your network credentials
#include "secrets.h"

#define DHTPIN 12
#define DSPIN 2

DHTStable DHT;
GyverDS18Single ds(DSPIN);

Adafruit_BMP085 bmp;

uint64_t addr_DS;
uint8_t res_DS;
uint8_t power;

// current temperature, humidity, pressure, updated in loop()
float t = 0.0;
float h = 0.0;
float t_ds = 0.0;
float p = 0.0;

// current status, updated in loop()
struct
{
  uint32_t total;
  uint32_t ok;
  uint32_t crc_error;
  uint32_t time_out;
  uint32_t read_time;
  uint32_t err_read_ds;
  uint32_t err_read_bmp; 
  uint32_t unknown;
} counter = { 0, 0, 0, 0, 0, 0, 0, 0 };

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

unsigned long previousMillis = 0;  // will store the last time when the readings were updated

// Updates readings every 10 seconds
const long interval = 10000;

String createJson();
String floatToString(float number);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>Weather station</h2>
  <p>
    <i class="fas fa-tachometer-alt" style="color:#00add6;"></i> 
    <span class="dht-labels"></span>
    <span id="pressure">%PRESSURE%</span>
    <sup class="units">mmHg</sup>
  </p>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">In</span> 
    <span id="temperaturein">%TEMPERATUREIN%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Out</span> 
    <span id="temperatureout">%TEMPERATUREOUT%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">In</span>
    <span id="humidityin">%HUMIDITYIN%</span>
    <sup class="units">%</sup>
  </p>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      try {
        const jsonObject = JSON.parse(this.responseText);
        document.getElementById("temperaturein").innerHTML = jsonObject.temperatureIn;
        document.getElementById("humidityin").innerHTML = jsonObject.humidityIn;
        document.getElementById("temperatureout").innerHTML = jsonObject.temperatureOut;
        document.getElementById("pressure").innerHTML = jsonObject.pressure;
      } catch (error) {
        console.error("Error parsing JSON:", error);
      }
    }
  };
  xhttp.open("GET", "/readings", true);
  xhttp.send();
}, 10000 ) ;
</script>
</html>)rawliteral";

// Replaces placeholder with sensor values
String processor(const String &var) {
  if (var == "TEMPERATUREIN") {
    return floatToString(t);
  } else if (var == "HUMIDITYIN") {
    return floatToString(h);
  } else if (var == "TEMPERATUREOUT") {
    return floatToString(t_ds);
  } else if (var == "PRESSURE") {
    return floatToString(p);
  }
  return String();
}

String floatToString(float number) {
  String str = String(number, 2);

  while (str.endsWith("0")) {
    str.remove(str.length() - 1);
  }

  if (str.endsWith(".")) {
    str.remove(str.length() - 1);
  }

  return str;
}

void setup() {
  Serial.begin(115200);  // Serial port for debugging purposes
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(".");
  }

  Serial.println(WiFi.localIP());  // Print ESP8266 Local IP Address
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/temperature-in", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", floatToString(t));
  });

  server.on("/humidity-in", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", floatToString(h));
  });

  server.on("/temperature-out", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", floatToString(t_ds));
  });

  server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", floatToString(p));
  });

  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", createJson());
  });

  addr_DS = ds.readAddress();
  if (addr_DS) {
    Serial.print("address DS18B20: ");
    gds::printAddress(addr_DS, Serial);
  } else {
    Serial.println("Error read address DS18B20.");
  }

  if (ds.setResolution(12)) Serial.println("setResolution: ok");
  else Serial.println("setResolution: error");

  res_DS = ds.readResolution();
  if (res_DS) {
    Serial.print("readResolution: ");
    Serial.println(res_DS);
  } else Serial.println("readResolution DS18B20: error");

  power = ds.readPower();
  if (power == 0) Serial.println("readPower DS18B20: error");
  else if (power == DS18_EXTERNAL) Serial.println("readPower DS18B20: external");
  else if (power == DS18_PARASITE) Serial.println("readPower DS18B20: parasite");

  ds.requestTemp();

  if (!bmp.begin()) {
    Serial.println("Could not find a valid BMP180 sensor, check wiring!");
  }

  server.begin();
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    uint32_t start = micros();

    float newP = bmp.readPressure();
    if (isnan(newP)) {
      Serial.println("Failed to read pressure from BMP180 sensor!");
      counter.err_read_bmp++;
    } else {
      p = newP * 0.00750063755419211;
      Serial.print("pressure: ");
      Serial.println(p);
    }
    

    int chk = DHT.read22(DHTPIN);

    counter.total++;
    switch (chk) {
      case DHTLIB_OK:
        counter.ok++;
        Serial.println("read DHT: OK,\t");
        break;
      case DHTLIB_ERROR_CHECKSUM:
        counter.crc_error++;
        Serial.println("read DHT:Checksum error,\t");
        break;
      case DHTLIB_ERROR_TIMEOUT:
        counter.time_out++;
        Serial.println("read DHT:Time out error,\t");
        break;
      default:
        counter.unknown++;
        Serial.println("read DHT:Unknown error,\t");
        break;
    }

    float newT = DHT.getTemperature();
    if (isnan(newT)) {
      Serial.println("Failed to read temperature from DHT sensor!");
    } else {
      t = newT;
      Serial.print("DHT temperature: ");
      Serial.println(t);
    }

    float newH = DHT.getHumidity();
    if (isnan(newH)) {
      Serial.println("Failed to read humidity from DHT sensor!");
    } else {
      h = newH;
      Serial.print("DHT humidity: ");
      Serial.println(h);
    }

    float new_t_ds;
    if (ds.ready()) {
      if (ds.readTemp()) {
        new_t_ds = ds.getTemp();
      } else {
        Serial.println("read error ds18b20");
        counter.err_read_ds++;
      }
    }
    if (isnan(new_t_ds)) {
      Serial.println("Failed to read from DS18B20 sensor!");
    } else {
      t_ds = new_t_ds;
      Serial.print("DS18B20 temperature: ");
      Serial.println(t_ds);
    }
    if (!ds.requestTemp()) {
      Serial.println("request error ds18b20");
      counter.err_read_ds++;
    }

    uint32_t stop = micros();
    counter.read_time = (stop - start) / 1000;
  }
}

String createJson() {
  String powerType;
  if (power == DS18_EXTERNAL) powerType = "DS18_EXTERNAL";
  else if (power == DS18_PARASITE) powerType = "DS18_PARASITE";

  String json = "{";
  json += "\"temperatureIn\":" + floatToString(t) + ",";
  json += "\"temperatureOut\":" + floatToString(t_ds) + ",";
  json += "\"humidityIn\":" + floatToString(h) + ",";
  json += "\"pressure\":" + floatToString(p) + ",";
  json += "\"total_read\":" + String(counter.total) + ",";
  json += "\"ok_DHT\":" + String(counter.ok) + ",";
  json += "\"crc_error_DHT\":" + String(counter.crc_error) + ",";
  json += "\"time_out_DHT\":" + String(counter.time_out) + ",";
  json += "\"unknown_error_DHT\":" + String(counter.unknown) + ",";
  json += "\"address_DS18B20\":" + String(addr_DS) + ",";
  json += "\"resolution_DS18B20\":" + String(res_DS) + ",";
  json += "\"power_DS18B20\":\"" + powerType + "\",";
  json += "\"error_read_DS18B20\":" + String(counter.err_read_ds) + ",";
  json += "\"error_read_BMP180\":" + String(counter.err_read_bmp) + ",";
  json += "\"read_time\":" + String(counter.read_time);
  json += "}";

  return json;
}