#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>  // Библиотека для TFT 1.8"

// ===== Пины TFT =====
#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4  // Можно подключить к RESET ESP, тогда укажи -1

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ===== Настройки Wi-Fi =====
const char* ssid     = "NASA";        
const char* password = "Nouan2525";   

// ===== Сервер =====
IPAddress serverIP(172,20,10,5);   
const int httpPort = 3000;

// ===== Кнопки =====
const int buttonPins[4] = {12, 13, 14, 27};
int lastButtonStates[4] = {HIGH, HIGH, HIGH, HIGH};

// ===== Клиент =====
WiFiClient client;

// ===== Текущий вопрос =====
int currentQuestion = -1;

void setup() {
  Serial.begin(115200);

  // Инициализация кнопок
  for (int i = 0; i < 4; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  // ===== TFT =====
  tft.initR(INITR_BLACKTAB);  // Инициализация ST7735
  tft.fillScreen(ST77XX_BLACK);
  tft.setRotation(1);  // Повернём горизонтально
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setRotation(4);
  // ===== Wi-Fi =====
  Serial.println("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected, IP: ");
  Serial.println(WiFi.localIP());

  // Отображаем на TFT
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.println("WiFi connected!");
  tft.print("IP: ");
  tft.println(WiFi.localIP());
  delay(2000);
}

void loop() {
  checkQuestion();

  // Проверка кнопок
  for (int i = 0; i < 4; i++) {
    int state = digitalRead(buttonPins[i]);
    if (state == LOW && lastButtonStates[i] == HIGH) {
      sendAnswer("Player_2", i + 1);
      delay(500);
    }
    lastButtonStates[i] = state;
  }
}

// ===== Получение текущего вопроса =====
void checkQuestion() {
  if (!client.connect(serverIP, httpPort)) {
    Serial.println("Connection to /current failed");
    return;
  }

  client.print(String("GET /current HTTP/1.1\r\n") +
               "Host: " + serverIP.toString() + "\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String response = client.readString();
  client.stop();

  int idx = response.indexOf("questionIndex");
  if (idx > 0) {
    int q = response.substring(idx).toInt();
    if (q != currentQuestion) {
      currentQuestion = q;
      tft.fillScreen(ST77XX_BLACK);
      tft.setCursor(0, 0);
      tft.setTextColor(ST77XX_YELLOW);
      tft.print("Question #");
      tft.println(currentQuestion);
    }
  }
}

// ===== Отправка ответа =====
void sendAnswer(String player, int choice) {
  if (!client.connect(serverIP, httpPort)) {
    Serial.println("Connection to /answer failed");
    return;
  }

  String url = "/answer?player=" + player + "&choice=" + String(choice);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + serverIP.toString() + "\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String response = client.readString();
  Serial.println("Server response: " + response);

  // Отображаем подтверждение
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_CYAN);
  tft.println("Player 2");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(0, 16);
  tft.println("Answer sent!");
  tft.setCursor(0, 32);
  tft.print("Choice: ");
  tft.println(choice);
  client.stop();
}
