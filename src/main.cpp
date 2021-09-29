#include <SoftwareSerial.h>
#include "keys.h"

#define BAUD_RATE    115200
#define A9_BAUD_RATE 115200

#define A9G_RX   14
#define A9G_TX   12
#define A9G_PON  16 // ESP12 GPIO16 A9/A9G POWON
#define A9G_POFF 15 // ESP12 GPIO15 A9/A9G POWOFF
#define A9G_WAKE 13 // ESP12 GPIO13 A9/A9G WAKE
#define A9G_LOWP 2  // ESP12 GPIO2 A9/A9G ENTER LOW POWER MODULE

SoftwareSerial A9G(A9G_RX, A9G_TX);

bool includes(String& str, String find) {
  uint16_t len = str.length();
  uint16_t findlen = find.length();
  if (findlen > len)
    return false;
  for (uint16_t i = 0, found = 0; i < len; i++) {
    if (str[i] == find[found]) {
      found++;
      if (findlen == found)
        return true;
    } else
      found = 0;
  }
  return false;
}

bool await(String str, uint16_t timeout = 5000, uint16_t ttl = 1000) {
  String buffer = "";
  uint64_t untill = millis() + timeout + ttl;
  while (millis() <= untill) {
    if (A9G.available()) {
      while (A9G.available()) {
        char ch = A9G.read();
        Serial.write(ch);
        buffer.concat(ch);
        untill += ttl;
      }
      if (includes(buffer, str))
        return true;
    }
  }
  return false;
}

bool command(
  String str,
  uint16_t timeout = 5000,
  uint16_t ttl = 1000,
  String success = "",
  String error = ""
) {
  A9G.println(str);
  String buffer = "";
  uint64_t untill = millis() + timeout + ttl;
  bool gotself = false;
  while (millis() <= untill) {
    if (A9G.available()) {
      while (A9G.available()) {
        char ch = A9G.read();
        Serial.write(ch);
        buffer.concat(ch);
        untill += ttl;
      }
      if (!gotself) {
        if (includes(buffer, str)) {
          gotself = true;
        } else {
          A9G.println(str);
          buffer = "";
          untill = millis() + timeout + ttl;
        };
      } else {
        if (
          includes(buffer, F("+CME ERROR"))
          || (error.length() && includes(buffer, error))
        ) {
          A9G.println(str);
          buffer = "";
          untill = millis() + timeout + ttl;
        } else if (
          includes(buffer, F("OK\r\n"))
          && (!success.length() || includes(buffer, success))
        ) return true;
      }
    }
  }
  return false;
}

void setup() {
  Serial.begin(BAUD_RATE);
  A9G.begin(A9_BAUD_RATE, SWSERIAL_8N1, 14, 12, false, 1024);
  for (char ch = ' '; ch <= 'z'; ch++)
    A9G.write(ch);
  A9G.println();

  pinMode(A9G_PON, OUTPUT);  // LOW LEVEL ACTIVE
  pinMode(A9G_POFF, OUTPUT); // HIGH LEVEL ACTIVE
  pinMode(A9G_LOWP, OUTPUT); // LOW LEVEL ACTIVE

  digitalWrite(A9G_PON, HIGH);
  digitalWrite(A9G_POFF, LOW);
  digitalWrite(A9G_LOWP, HIGH);

  Serial.println();
  delay(2000);
  digitalWrite(A9G_PON, LOW);
  delay(3000);
  digitalWrite(A9G_PON, HIGH);
  while (!await(F("READY\r\n")));
  while (!command(F(R"(AT)")));
  while (!command(F(R"(AT+CGDCONT=1,"IP","internet")")));
  while (!command(F(R"(AT+CGATT=1)")));
  while (!command(F(R"(AT+CGACT=1,1)")));
}

uint64_t last = 0;
void loop() {
  if (millis() - last > 60000 || last == 0) {
    const auto url = String(SERVICE_URL);
    const auto telemetry = "4, 55.973589,  38.684297, 10,,0";
    const auto mimetype = "text/plain";
    const auto postcommand =
      F("AT+HTTPPOST=\"")
      + url
      + "\",\""
      + mimetype
      + "\",\""
      + telemetry
      + "\"";
    auto success = command(
      postcommand,
      20000,
      1000,
      R"("status":"success")",
      R"("status":"error")");
    if (success) last = millis();
  }
}