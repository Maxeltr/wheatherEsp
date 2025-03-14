#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <GyverDS18.h>
#include "DHTStable.h"

// Replace with your network credentials
#include "secrets.h"

#define DHTPIN 5
#define DSPIN 2

DHTStable DHT;
GyverDS18Single ds(DSPIN);

// current temperature & humidity, updated in loop()
float t = 0.0;
float h = 0.0;
float t_ds = 0.0;

// current status, updated in loop()
uint64_t addr_DS;
uint8_t res_DS;
uint8_t power;
struct
{
  uint32_t total;
  uint32_t ok;
  uint32_t crc_error;
  uint32_t time_out;
  uint32_t read_time;
  uint32_t err_read_ds;
  uint32_t ack_h;  //reserve
  uint32_t unknown;
} counter = { 0, 0, 0, 0, 0, 0, 0, 0 };

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

unsigned long previousMillis = 0;  // will store the last time when the readings were updated

// Updates readings every 10 seconds
const long interval = 10000;

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
  <h2>Wheather station</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">In</span> 
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Out</span> 
    <span id="temperatureds">%TEMPERATUREDS%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">In</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">%</sup>
  </p>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidity").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperatureds").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperatureds", true);
  xhttp.send();
}, 10000 ) ;

</script>
</html>)rawliteral";

// Replaces placeholder with sensor values
String processor(const String &var) {
  //Serial.println(var);
  if (var == "TEMPERATURE") {
    return String(t);
  } else if (var == "HUMIDITY") {
    return String(h);
  } else if (var == "TEMPERATUREDS") {
    return String(t_ds);
  }
  return String();
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

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    String temperatureString = String(t);
    request->send(200, "text/plain", temperatureString);
  });

  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    String humidityString = String(h);
    request->send(200, "text/plain", humidityString);
  });

  server.on("/temperatureds", HTTP_GET, [](AsyncWebServerRequest *request) {
    String temperatureDsString = String(t_ds);
    request->send(200, "text/plain", temperatureDsString);
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String powerType;
    if (power == DS18_EXTERNAL) powerType = "DS18_EXTERNAL";
    else if (power == DS18_PARASITE) powerType = "DS18_PARASITE";

    String json = "{";
    json += "\"total_read\":" + String(counter.total) + ",";
    json += "\"ok_DHT\":" + String(counter.ok) + ",";
    json += "\"crc_error_DHT\":" + String(counter.crc_error) + ",";
    json += "\"time_out_DHT\":" + String(counter.time_out) + ",";
    json += "\"unknown_DHT\":" + String(counter.unknown) + ",";
    json += "\"address_DS18B20\":" + String(addr_DS) + ",";
    json += "\"resolution_DS18B20\":" + String(res_DS) + ",";
    json += "\"power_DS18B20\":\"" + powerType + "\",";
    json += "\"error_read_DS18B20\":" + String(counter.err_read_ds) + ",";
    json += "\"read_time\":" + String(counter.read_time);
    json += "}";
    request->send(200, "text/plain", json);
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

  server.begin();
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    uint32_t start = micros();
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
    if (!ds.requestTemp()) Serial.println("request error ds18b20");

    uint32_t stop = micros();
    counter.read_time = stop - start;
  }
}