#include <Arduino.h>
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <CircularBuffer.h>
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

void setup()
{
  delay(100);
  Serial.begin(9600);
  bt_serial.begin(9600);
  sensors.begin();
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
    }
  }
  
}
