#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <Arduino_JSON.h>
#include <Servo.h>

ESP8266WebServer server(80);

#define ARRAY_LENGTH 180 //Servonun her döndüğü açı için bulunacak ışık değerlerini tıtacak dizi boyutu.
#define THRESHOLD 500 //LDR bu değerden düşük bir değer hesapladığı takdirde servise gidilecek.
#define X_PANEL_PIN 12
#define Y_PANEL_PIN 15
#define X_LDR_PIN 4
#define Y_LDR_PIN 14
#define LDR_PIN A0

Servo xPanelServo;
Servo yPanelServo;
Servo xLDRServo;
Servo yLDRServo;

int light;
int maxValue;
int altitude, azimuth;

/* İsteğimize cevap verecek adresimiz */
const String HOST = "http://solar-path-api.herokuapp.com"; // kullandığımız nodemcu https protokolüne istek atamıyor.
const String PATH = "/api/v1/solar-path";

/*Bu değerler ise servisimize gönderilecek ve hesaplamalar yapılırken baz alınacak değerleri tanımlar*/
const String CITY = "Istanbul";
const String TIMEZONE = "3";
const String LONGITUDE = "28.979";
const String LATITUDE = "41.015";
const String API_KEY = "5WQEKSF7PANB";

String queryString = "?city=" + CITY + "&timezone=" + TIMEZONE + "&longitude=" + LONGITUDE + "&latitude=" + LATITUDE + "&apikey=" + API_KEY;

/* Aşağıdaki URL değişkeninin açık hali
    => http://solar-path-api.herokuapp.com/api/v1/solar-path?city=Istanbul&timezone=3&longitude=28.979&latitude=41.015&apikey=5WQEKSF7PANB
    Tarih: 30/05/2021
     ! Siz bu adrese istek atığınızda servis kapatılmıs olabilir yada apikey işlevsizleşmiş olabilir.
*/
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
  /*
    Neden bu if var?
      - Bu if sayesinde LDR'nın bulunduğu konumdaki ışık değeri 
       THRESHOLD değerinin altına düştüğü an x ve y eksenleri tekrar taranacak ve 
       bu THRESHOLD değerinden büyük bir değer bulmaya çalışacak.
       Fakat LDR'nin bulunduğu konumdaki ışık değeri bu THRESHOLD değerinin altına düşmez ise
       panel baktığı açıyı değiştirmeyecek.
  */
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
  // LDR'yi kontrol eden servoların dönüşünü gerçekleştirecek fonksiyon. 
  int _light = 0;
  int _values[ARRAY_LENGTH];
  // 0-180 arası tarayarak her bir derecedeki ışık şiddetini dizimizin içine kaydeder.
  for (int degree = 0; degree < ARRAY_LENGTH; degree++) {
    servo.write(degree);
    _light = analogRead(LDR_PIN);
    _values[degree] = _light;
    delay(50);
  }
  /*  
      Dönmemiz gereken açı en büyük değerin bulunduğu indise denk geleceği için
      en büyük değerin bulunduğu indisi bulmak için yazdığımız fonksiyona dizimizi gönderiyoruz.
  */
  int maxNumbersIndex = findMaxNumbersIndex(_values);
  /*  
      En yüksek ışık değeri global değişken olan maxValue'ya atanıyor. 
      Bu değer loop fonksiyonunda kontrol kısmında lazım olacak
   */
  maxValue = _values[maxNumbersIndex]; 
  return maxNumbersIndex; // En yüksek değere sahip açıyı geri döndürür.
}

int findMaxNumbersIndex(int values[]) {
  /*Dizi içerisindeki en büyük sayıya sahip indisi geri döndürür.*/
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

void connect2Server() {
  /*
    Nodemcu internete bağlandıysa belirttiğimiz adrese isteği gönderir.
  */
  bool isWifiConnected = connect2Network();
  if (isWifiConnected) {
    HTTPClient http;
    http.begin(URL);
    /*
      GET() metodu,
      Bağlantı başarısız olursa -1 döndürür.
      Başarılı olur ise sayfanın döndürdüğü status code'u döndürür.
    */
    int httpCode = http.GET();
    if (httpCode > 0) {
      String response = http.getString();
      /*
        istek yolladğımız adresin bize verdiği cevabı, cevaptan (bu cevap json formatında)
        ihtiyacımız olan yerleri alabilmek için yazdığımız bir fonksiyona gönderiyoruz.
      */
      convert2JsonAndSetValues2Variables(response);
    }
    http.end();
  }
  WiFi.disconnect();
}

bool connect2Network() {
  /*
    Nodemcu'yu internete bağlar.
    10 deneme sonunda bağlanamaz ise daha fazla bağlanmayı denemez.
    Bağlantı başarılı olur ise true olmaz ise false döndürür.
  */
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

void convert2JsonAndSetValues2Variables(String payload) {
  JSONVar object = JSON.parse(payload);
  if (JSON.typeof(object) == "undefined") {
    Serial.println("Parsing input failed!");
    return;
  }

  /*
    servis başarısız bir cevap döndürmüş ise cevapta istediğimiz kısımlar olmadığından 
    fonksiyonu ani bir şekilde sonlandırıyoruz.
  */
  if (!(bool)object["success"]) {
    Serial.println((const char*)object["message"]);
    return;
  }

  /*isteğimize başarılı bir cevap verilmiş ise bu istekte belirttiğimiz yerleri alıyoruz*/
  double tmpAzimuth = (double)object["azimuth"];
  altitude = round((double)object["altitude"]);
  /*
    Servisimuz azimuth değerini -90 ile 90 aralığında hesaplıyor.
    Servolarımız bu negatif değerlere dönmeyeceğinden bu aralığı 0 ile 180 arasına çeviriyoruz.
    Ve bu değerleri global değişkenlere atıyoruz. 
    Bu değerler loop fonksiyonunda servoların döneceği açı için kullanılacak
  */
  azimuth = round(map(tmpAzimuth, -90, 90, 0, 180));
}
