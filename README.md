Weather Station with ESP8266
This project implements a simple weather station using an ESP8266 microcontroller, a DHT22 sensor for measuring temperature and humidity, and a DS18B20 sensor for measuring outdoor temperature. The data is served via a web interface, allowing users to monitor the readings in real-time.

Features
Measures indoor temperature and humidity using a DHT22 sensor.
Measures outdoor temperature using a DS18B20 sensor.
Displays readings on a web page.
Updates readings every 10 seconds.
Provides JSON endpoint for easy data access.

Components Required
ESP8266 (e.g., NodeMCU, Wemos D1 Mini)
DHT22 Temperature and Humidity Sensor
DS18B20 Temperature Sensor
Breadboard and jumper wires

Installation
Clone the Repository:
git clone https://github.com/yourusername/weather-station.git
cd weather-station

Install Required Libraries:
Make sure to install the following libraries in your Arduino IDE:
ESPAsyncWebServer
ESPAsyncTCP
Adafruit_Sensor
GyverDS18
DHTStable

Configure Wi-Fi Credentials:
Create a file named secrets.h in the project directory and add your Wi-Fi credentials:
const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";

Upload the Code:
Open the project in the Arduino IDE, select the correct board and port, and upload the code.

Usage
Connect the Hardware:
Connect the DHT22 sensor to GPIO5 (D1).
Connect the DS18B20 sensor to GPIO2 (D4) with a 4.7kΩ resistor between the data and VCC pins.

Access the Web Interface:
Open the Serial Monitor to find the ESP8266's IP address.
Enter the IP address in a web browser to access the weather station interface.

View Readings:
The web page will display the indoor temperature, humidity, and outdoor temperature.
The readings will update every 10 seconds.

JSON Endpoint
You can access the sensor data in JSON format by navigating to /readings on the web server. The JSON response will include:
temperatureIn: Indoor temperature
humidityIn: Indoor humidity
temperatureOut: Outdoor temperature
total_read: Total number of readings
ok_DHT: Number of successful DHT readings
crc_error_DHT: Number of checksum errors
time_out_DHT: Number of timeout errors
unknown_DHT: Number of unknown errors
address_DS18B20: Address of the DS18B20 sensor
resolution_DS18B20: Resolution of the DS18B20 sensor
power_DS18B20: Power type of the DS18B20 sensor
error_read_DS18B20: Number of read errors from the DS18B20
read_time: Time taken to read the sensors

Individual Endpoints
You can also access individual sensor readings via the following endpoints:

Indoor Temperature: /temperature-in
Indoor Humidity: /humidity-in
Outdoor Temperature: /temperature-out
Each endpoint will return the corresponding value in plain text.

Troubleshooting
Ensure all connections are secure.
Check the Serial Monitor for error messages.
Make sure the correct libraries are installed.

License
This project is licensed under the MIT License. See the LICENSE file for details.

Acknowledgments
ESP8266 Community
Adafruit
GyverDS18 Library
Feel free to contribute to this project by submitting issues or pull requests!
