#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>  // Библиотека для TFT 1.8"

// ===== Пины TFT =====
#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ===== Настройки Wi-Fi =====
const char* ssid     = "NN";
const char* password = "123456789";

// ===== Сервер =====
IPAddress serverIP(172,20,10,2);
const int httpPort = 3000;

// ===== Кнопки =====
const int buttonPins[4] = {12, 13, 14, 27};
int lastButtonStates[4] = {HIGH, HIGH, HIGH, HIGH};

// ===== Клиент =====
WiFiClient client;

// ===== Текущий вопрос =====
int currentQuestion = -1;

// ===== Экран подсказки (тот же, что при новом вопросе) =====
void showHintScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);

  tft.setTextColor(ST77XX_CYAN);
  tft.println("=== QUESTION ===");

  tft.setTextColor(ST77XX_WHITE);
  tft.print("Question #: ");

  tft.setTextColor(ST77XX_YELLOW);
  tft.println(currentQuestion);

  tft.setTextColor(ST77XX_WHITE);
  tft.println("--------------------");

  tft.setTextColor(ST77XX_GREEN);
  tft.println("Buttons:");

  tft.setTextColor(ST77XX_WHITE);
  tft.println("Black  = 1");
  tft.println("Red    = 2");
  tft.println("Green  = 3");
  tft.println("Blue   = 4");

  tft.setTextColor(ST77XX_CYAN);
  tft.println("--------------------");
  tft.println("Make your choice!");
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 4; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  // ===== TFT =====
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST77XX_BLACK);
  tft.setRotation(1);
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

  Serial.println("\nWiFi connected");

  // ===== Стартовый экран =====
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);

  tft.setTextColor(ST77XX_GREEN);
  tft.println("=== CONNECTED ===");

  tft.setTextColor(ST77XX_WHITE);
  tft.print("Wi-Fi: ");
  tft.println(ssid);

  tft.print("IP:   ");
  tft.println(WiFi.localIP());

  tft.setTextColor(ST77XX_CYAN);
  tft.print("Server ");
  tft.print(serverIP);
  tft.print(":");
  tft.println(httpPort);

  tft.setTextColor(ST77XX_YELLOW);
  tft.println("----------------");
  tft.setTextColor(ST77XX_WHITE);
  tft.println("System ready...");
  delay(2000);
}

void loop() {
  checkQuestion();

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
      showHintScreen();
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

  // ===== Экран подтверждения =====
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);

  tft.setTextColor(ST77XX_CYAN);
  tft.println("=== PLAYER 2 ===");

  tft.setTextColor(ST77XX_GREEN);
  tft.println("Answer sent!");

  tft.setTextColor(ST77XX_WHITE);
  tft.println("----------------");

  tft.setTextColor(ST77XX_YELLOW);
  tft.print("Selected: ");
  tft.setTextColor(ST77XX_WHITE);
  tft.println(choice);

  tft.setTextColor(ST77XX_CYAN);
  tft.println("Please wait...");

  client.stop();

  // ✅ ДЕРЖИМ 3 СЕКУНДЫ
  delay(3000);

  // ✅ ПОТОМ СНОВА ПОДСКАЗКА
  showHintScreen();
}
