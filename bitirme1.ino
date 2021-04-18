#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <Arduino_JSON.h>
#include <Servo.h>

ESP8266WebServer server(80);

#define ARRAY_LENGTH 180
#define THRESHOLD 500
#define X_PANEL_PIN 4
#define Y_PANEL_PIN 14
#define X_LDR_PIN 12
#define Y_LDR_PIN 15
#define LDR_PIN A0

Servo xPanelServo;
Servo yPanelServo;
Servo xLDRServo;
Servo yLDRServo;

int light;
int maxValue;
int altitude, azimuth;

const String HOST = "http://solar-path-api.herokuapp.com"; //without s
const String PATH = "/api/v1/solar-path";

const String CITY = "Istanbul";
const String TIMEZONE = "3";
const String LONGITUDE = "28.979";
const String LATITUDE = "41.015";
const String API_KEY = "5WQEKSF7PANB";

String queryString = "?city=" + CITY + "&timezone=" + TIMEZONE + "&longitude=" + LONGITUDE + "&latitude=" + LATITUDE + "&apikey=" + API_KEY;

const String URL = HOST + PATH + queryString;

void setup() {
  Serial.begin(9600);
  /*attach servos to pins*/
  xPanelServo.attach(X_PANEL_PIN);
  yPanelServo.attach(Y_PANEL_PIN);
  xLDRServo.attach(X_LDR_PIN);
  yLDRServo.attach(Y_LDR_PIN);

  xLDRServo.write(0);
  yLDRServo.write(90);

  light = 0;
}

void loop() {
  int xMaxDegree, yMaxDegree;
  if (light < THRESHOLD) {
    
    yLDRServo.write(90);
    xMaxDegree = scanAxis(xLDRServo);
    xLDRServo.write(xMaxDegree);

    yMaxDegree = scanAxis(yLDRServo);

    azimuth = xMaxDegree;
    altitude = yMaxDegree;
    if (maxValue < THRESHOLD) {
      connect2Server();
    }
    xPanelServo.write(azimuth);
    delay(1000);
    yPanelServo.write(altitude);
  }
  
  xLDRServo.write(azimuth);
  yLDRServo.write(altitude);
  
  light = analogRead(LDR_PIN);
  delay(50);
}


int scanAxis(Servo servo) {
  int _light = 0;
  int _values[ARRAY_LENGTH];
  for (int degree = 0; degree < ARRAY_LENGTH; degree++) {
    servo.write(degree);
    _light = analogRead(LDR_PIN);
    _values[degree] = _light;
    delay(50);
  }
  int maxNumbersIndex = findMaxNumbersIndex(_values);
  maxValue = _values[maxNumbersIndex];
  return maxNumbersIndex;
}

int findMaxNumbersIndex(int values[]) {
  int maxNumber = -100;
  int maxNumbersIndex = 0;

  for (int i = 0 ; i < ARRAY_LENGTH ; i++) {
    if (values[i] > maxNumber) {
      maxNumber = values[i];
      maxNumbersIndex = i;
    }
  }

  return maxNumbersIndex;
}

bool connect2Network() {
  const char* ssid = "yourNetworkName";
  const char* password = "yourNetworkPassword";
  int repeat = 0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    repeat++;
    if (repeat > 10) {
      Serial.print("Cant conenct to wifi...");
      return false;
    }
    delay(1000);
  }
  return true;
}

void connect2Server() {
  bool isWifiConnected = connect2Network();
  if (isWifiConnected) {
    HTTPClient http;
    http.begin(URL);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String response = http.getString();
      convert2JsonAndSetValues2Variables(response);
    }
    http.end();
  }
  WiFi.disconnect();
}

void convert2JsonAndSetValues2Variables(String payload) {
  JSONVar object = JSON.parse(payload);
  if (JSON.typeof(object) == "undefined") {
    Serial.println("Parsing input failed!");
    return;
  }

  if (!(bool)object["success"]) {
    Serial.println((const char*)object["message"]);
    return;
  }

  double tmpAzimuth = (double)object["azimuth"];
  altitude = round((double)object["altitude"]);
  azimuth = round(map(tmpAzimuth, -90, 90, 0, 180));
}
