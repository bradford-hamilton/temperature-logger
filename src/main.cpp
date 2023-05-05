#include <Arduino.h>
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <CircularBuffer.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <vector>

SoftwareSerial bt_serial{13, 15};
OneWire one_wire{2};
DallasTemperature sensors{&one_wire};
CircularBuffer<float, 1024> temp_buf;
// CircularBuffer
// 128KB of ram and 4 bytes per float (128 * 1024) / 4 == 32,768
// Take half that (16,384) for the temperature data
// TODO actually find out ram of this device through code

char sync_signal;
char float_str[5];
int num_decimal_places = 2;

// Access point "database"
std::vector<float> ap_db(1024);

const char* ssid = "your_ssid";
const char* pass = "your_pass";

void connect_to_wifi();

void setup()
{
  delay(100);
  Serial.begin(9600);
  bt_serial.begin(9600);
  sensors.begin();
  connect_to_wifi();
}

void loop()
{
  delay(3000);

  sensors.requestTemperatures();
  float temp_f = sensors.getTempFByIndex(0);
  temp_buf.push(temp_f);

  // "Access point" code - requesting data from device. Writing out to serial bluetooth
  // on phone but will emulate the access point's persisted data in this same project
  // for simplicity's sake.
  if (bt_serial.available()) {
    sync_signal = bt_serial.read();
    if (sync_signal == 'S') {
      bt_serial.write("data requested...");

      for (int i = 0; i < temp_buf.size(); i++) {
        ap_db.push_back(temp_buf[i]);

        snprintf(float_str, sizeof(float_str), "%.*f", num_decimal_places, temp_buf[i]);
        bt_serial.write(float_str);
        bt_serial.write(',');
      }

      bt_serial.write(';');

      WiFiClient client;
      HTTPClient http_client;

      if (http_client.begin(client, "http://192.168.1.62:4000/dd/new")) {
        http_client.addHeader("Content-Type", "application/json");

        // { sensor: "DS18B20", values: ["81.47", "84.43", "83.56"] }
        // { \"sensor\": \"DS18B20\", \"values\": [\"81.47\", \"84.43\", \"83.56\"] }
        // \"81.47\", \"84.43\", \"83.56\"
        String payload {"{ \"sensor\": \"DS18B20\", \"data_values\": [\"81.47\", \"84.43\", \"83.56\"] }"};

        int httpCode = http_client.POST(payload);
        if (httpCode > 0) {
          if (httpCode == HTTP_CODE_OK) {
            Serial.println("POST data successful");
          }
        } else {
          Serial.printf("[HTTP] POST failed, error: %s\n", http_client.errorToString(httpCode).c_str());
        }

        http_client.end();
      } else {
        Serial.print("[HTTPS] Unable to connect\n");
      }
    }
  }

}

void connect_to_wifi()
{
  Serial.print("Connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
