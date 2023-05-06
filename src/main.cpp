#include <Arduino.h>
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <CircularBuffer.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <vector>
#include <string>
#include <sstream>

#define SSID "your_ssid"
#define PASS "your_pass"

#define BAUD 9600

#define RX_PIN 13
#define TX_PIN 15
#define GPIO_2 2

SoftwareSerial bt_serial{RX_PIN, TX_PIN};
OneWire one_wire{GPIO_2};
DallasTemperature sensors{&one_wire};
CircularBuffer<float, 1024> temp_buf;

char signal;
char float_str[5];
uint8_t decimal_places{2};
std::vector<float> ap_db; // The access point "database"
uint16_t last_posted_index{0};

void connect_to_wifi();
void request_and_record_temps();
void emulate_access_point();
void handle_bt_signal(char sig);
void sync_bt_data();
void post_bt_data();
String build_payload();

void setup()
{
  Serial.begin(BAUD);
  bt_serial.begin(BAUD);
  sensors.begin();
  connect_to_wifi();
}

void loop()
{
  delay(3000);
  request_and_record_temps();
  emulate_access_point();
}

void connect_to_wifi()
{
  Serial.print("Connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void request_and_record_temps()
{
  sensors.requestTemperatures();
  temp_buf.push(sensors.getTempFByIndex(0));
}

void emulate_access_point()
{
  if (bt_serial.available()) {
    handle_bt_signal(bt_serial.read());
  }
}

void handle_bt_signal(char sig)
{
  switch(sig) {
    case 'S':
      Serial.write("data sync requested...");
      sync_bt_data();
      post_bt_data();
      break;
  }
}

void sync_bt_data()
{
  for (int i = 0; i < temp_buf.size(); i++) {
    ap_db.push_back(temp_buf[i]);
    snprintf(
      float_str,
      sizeof(float_str),
      "%.*f",
      decimal_places,
      temp_buf[i]
    );
    bt_serial.write(float_str);
    bt_serial.write(',');
  }
  bt_serial.write(';');
}

void post_bt_data()
{
  WiFiClient client;
  HTTPClient http_client;

  if (!http_client.begin(client, "http://192.168.1.62:4000/dd/new")) {
    Serial.print("[HTTP] Unable to connect\n");
    return;
  }

  http_client.addHeader("Content-Type", "application/json");

  int httpCode = http_client.POST(build_payload());
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("[HTTP] POST successful");
    }
  } else {
    Serial.printf("[HTTP] POST failed: %s\n", http_client.errorToString(httpCode).c_str());
  }

  http_client.end();
}

String build_payload()
{
  std::stringstream ss;
  ss << "{ \"sensor\": \"DS18B20\", \"data_values\": [";

  for (uint16_t i = last_posted_index; i < ap_db.size(); i++) {
    ss << "\"" << ap_db.at(i) << "\",";
  }

  std::string payload = ss.str();
  if (!payload.empty()) { payload.pop_back(); }
  payload += "] }";

  last_posted_index = ap_db.size();

  return String{payload.c_str()};
}
