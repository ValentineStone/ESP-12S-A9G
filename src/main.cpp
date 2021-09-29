#include <SoftwareSerial.h>
#include "keys.h"

#define BAUD_RATE          115200
#define A9G_BAUD_RATE      115200
#define A9G_BAUD_RATE_SLOW 9600

#define A9G_RX   14
#define A9G_TX   12
#define A9G_PON  16 // ESP12 GPIO16 A9/A9G POWON
#define A9G_POFF 15 // ESP12 GPIO15 A9/A9G POWOFF
#define A9G_WAKE 13 // ESP12 GPIO13 A9/A9G WAKE
#define A9G_LOWP 2  // ESP12 GPIO2 A9/A9G ENTER LOW POWER MODULE

SoftwareSerial A9G(A9G_RX, A9G_TX);
char BUFFER2k[2048];
char CHBUFF[2] = { 0, 0 };

void strchcat(char* str, char ch) {
  CHBUFF[0] = ch;
  CHBUFF[1] = 0;
  strncat(str, CHBUFF, 1);
}

bool includes(const char* str, const char* find) {
  /*
  Serial.println("<includes>");
  Serial.println("<str>");
  Serial.println(str);
  Serial.println("</str>");
  Serial.println("<find>");
  Serial.println(find);
  Serial.println("</find>");
  Serial.println("</includes>");
  */
  uint16_t len = strlen(str);
  uint16_t findlen = strlen(find);
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

bool await(const char* str, uint16_t timeout = 5000, uint16_t ttl = 1000) {
  auto& buffer = BUFFER2k;
  memset(buffer, 0, sizeof buffer);
  uint64_t untill = millis() + timeout + ttl;
  while (millis() <= untill) {
    if (A9G.available()) {
      while (A9G.available()) {
        char ch = A9G.read();
        Serial.write(ch);
        strchcat(buffer, ch);
        untill += ttl;
      }
      if (includes(buffer, str))
        return true;
    }
  }
  return false;
}

bool command(
  const char* str,
  uint16_t timeout = 5000,
  uint16_t ttl = 1000,
  const char* success = "",
  const char* error = ""
) {
  while (A9G.available())
    Serial.write(A9G.read());
  auto& buffer = BUFFER2k;
  memset(buffer, 0, sizeof buffer);
  A9G.write(str);
  A9G.write("\r\n");
  uint64_t untill = millis() + timeout + ttl;
  while (millis() <= untill) {
    if (A9G.available()) {
      while (A9G.available()) {
        char ch = A9G.read();
        Serial.write(ch);
        strchcat(buffer, ch);
        untill += ttl;
      }
      if (
        includes(buffer, "+CME ERROR")
        || (!strlen(error) && includes(buffer, error))
      ) {
        A9G.write(str);
        A9G.write("\r\n");
        memset(buffer, 0, sizeof buffer);
        untill = millis() + timeout + ttl;
      } else if (
        includes(buffer, "OK\r\n")
        && (!strlen(success) || includes(buffer, success))
      ) return true;
    }
  }
  return false;
}

void A9G_init_magic_chars() {
  for (char ch = ' '; ch <= 'z'; ch++)
    A9G.write(ch);
  A9G.println();
}

void setup() {
  Serial.begin(BAUD_RATE);
  Serial.println("Starting...");
  A9G.begin(A9G_BAUD_RATE, SWSERIAL_8N1, A9G_RX, A9G_TX, false, 1200);
  A9G_init_magic_chars();
  Serial.println("Powering A9G...");

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
  while (!await("READY\r\n"));
  A9G.flush();
  A9G.end();
  A9G.begin(A9G_BAUD_RATE_SLOW, SWSERIAL_8N1, A9G_RX, A9G_TX, false, 1200);
  Serial.println("Initializing...");
  while (!command(R"(AT)", 1000, 100));
  while (!command(R"(AT+CGDCONT=1,"IP","internet")"));
  while (!command(R"(AT+CGATT=1)"));
  while (!command(R"(AT+CGACT=1,1)"));
}

uint64_t last = 0;
void loop() {
  if (millis() - last > 60000 || last == 0) {
    const auto boat_id = "4";
    const auto lat = 55.973589 + random(100) / 100000.;
    const auto lon = 38.684297 + random(100) / 100000.;
    const auto speed = (int) random(11);
    const auto rotation = (int) random(360);
    const auto impact = random(100) == 1 ? 1 : 0;
    char postcommand[512];
    snprintf(postcommand, 512,
      R"(AT+HTTPPOST="%s","%s","%s,%f,%f,%i,%i,%u")",
      SERVICE_URL,
      MIMETYPE,
      boat_id,
      lat,
      lon,
      speed,
      rotation,
      impact
    );
    auto success = command(
      postcommand,
      20000,
      1000,
      R"("status":"success")",
      R"("status":"error")");
    if (success) last = millis();
  } else {
    while (A9G.available())
      Serial.write(A9G.read());
  }
}