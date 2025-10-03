#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// TFT display
TFT_eSPI tft = TFT_eSPI();

// Firebase
#define FIREBASE_HOST "lab10-f138a-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "0afFMvUAuYZKK6X39t2sBk4vuraP9hdg5fwJyA8a"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJsonData jsonData;

// ตัวแปรเก็บข้อมูลจาก Firebase
float g_temp = NAN;
float g_hum = NAN;
int g_mq2 = -1;
bool g_flame = false;
bool g_smoke = false;
String g_status = "Waiting for data...";

// ตัวแปรเก็บค่าเก่าสำหรับลดการกระพริบ
float last_temp = NAN;
float last_hum = NAN;
int last_mq2 = -1;
bool last_flame = false;
bool last_smoke = false;
String last_status = "";
bool last_warning = false;
bool last_wifi = false;

unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 1000; // 1 วินาที

char ssid[] = "Booklab";
char pass[] = "ccsadmin";

// ----------------- Function prototypes -----------------
void connectWiFi();
void displayErrorScreen(String error);
void displayStatusScreen(bool fullRedraw);
void displayStandardWarningScreen();

// ----------------- Setup -----------------
void setup() {
  Serial.begin(115200);
  Serial.println("TFT Display Debug: Starting...");

  // Initialize TFT
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Connect WiFi
  connectWiFi();
  if (!last_wifi) {
    displayErrorScreen("WiFi Failed!");
    while (1) delay(1000);
  }

  // Initialize Firebase
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.ready()) Serial.println("Firebase ready!");
  else Serial.println("Firebase not ready - check token!");

  // Initial full draw
  displayStatusScreen(true);
}

// ----------------- Loop -----------------
void loop() {
  connectWiFi();  // เช็คและต่อ WiFi ทุกรอบ

  if (!last_wifi) {
    displayErrorScreen("WiFi Failed!");
    delay(1000);
    return;
  }

  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    bool dataUpdated = false;

    for (int retry = 0; retry < 3; retry++) {
      if (Firebase.getJSON(fbdo, "/current_status")) {
        FirebaseJson &json = fbdo.jsonObject();

        json.get(jsonData, "temperature"); g_temp = jsonData.success ? jsonData.floatValue : g_temp;
        json.get(jsonData, "humidity"); g_hum = jsonData.success ? jsonData.floatValue : g_hum;
        json.get(jsonData, "mq2_analog"); g_mq2 = jsonData.success ? jsonData.intValue : g_mq2;
        json.get(jsonData, "flame_detected"); g_flame = jsonData.success ? jsonData.boolValue : g_flame;
        json.get(jsonData, "smoke_detected"); g_smoke = jsonData.success ? jsonData.boolValue : g_smoke;
        json.get(jsonData, "status"); g_status = jsonData.success ? jsonData.stringValue : g_status;

        dataUpdated = true;
        break;
      } else {
        if (retry < 2) delay(500);
      }
    }

    if (!dataUpdated) g_status = "Connection Error";

    bool isWarning = g_flame || g_smoke || g_status == "FIRE!" || g_status == "SMOKE / GAS HIGH";
    if (isWarning != last_warning) {
      if (isWarning) displayStandardWarningScreen();
      else displayStatusScreen(true);
      last_warning = isWarning;
    } else if (!isWarning) {
      displayStatusScreen(false);
    }

    lastUpdate = millis();
  }

  delay(50);
}

// ----------------- WiFi Connect -----------------
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    last_wifi = true;
    return;
  }

  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, pass);
  unsigned long startAttempt = millis();
  const unsigned long timeout = 10000; // 10 วินาที timeout

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < timeout) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
    last_wifi = true;
  } else {
    Serial.println("\nWiFi connection failed!");
    last_wifi = false;
  }
}

// ----------------- Display helpers -----------------
void displayErrorScreen(String error) {
  tft.fillScreen(TFT_BLACK);

  // Title
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(4);
  int centerX = tft.width() / 2;
  String title = "ERROR!";
  int w = tft.textWidth(title);
  int yTitle = 36;
  tft.setCursor(centerX - w / 2, yTitle);
  tft.println(title);

  // Error message
  tft.setTextSize(3);
  int ew = tft.textWidth(error);
  int yError = yTitle + 44;
  tft.setCursor(centerX - ew / 2, yError);
  tft.println(error);
}

void displayStatusScreen(bool fullRedraw) {
  // Layout constants — ปรับให้เหมาะสมกับขนาดตัวอักษร
  const int PADDING_LEFT = 12;
  const int LABEL_COL_WIDTH = 100;   // เพิ่มความกว้างสำหรับ labels
  const int VALUE_COL_X = PADDING_LEFT + LABEL_COL_WIDTH;
  const int VALUE_WIDTH = 200;       // เพิ่มความกว้างสำหรับ values
  const int LINE_HEIGHT = 40;        // เพิ่มระยะห่างระหว่างบรรทัด
  const int START_Y = 60;

  int centerX = tft.width() / 2;

  if (fullRedraw) {
    // ล้างหน้าจอทั้งหมด
    tft.fillScreen(TFT_BLACK);

    // Header (ขนาดใหญ่)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(4);
    String title = "FIRE ALARM";
    int titleWidth = tft.textWidth(title);
    int yTitle = 8;
    tft.setCursor(centerX - titleWidth / 2, yTitle);
    tft.println(title);
    tft.drawFastHLine(0, yTitle + 48, tft.width(), TFT_WHITE);

    // Labels (ขนาดใหญ่)
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(PADDING_LEFT, START_Y + 0 * LINE_HEIGHT); tft.print("Temp:");
    tft.setCursor(PADDING_LEFT, START_Y + 1 * LINE_HEIGHT); tft.print("Hum:");
    tft.setCursor(PADDING_LEFT, START_Y + 2 * LINE_HEIGHT); tft.print("Gas:");
    tft.setCursor(PADDING_LEFT, START_Y + 3 * LINE_HEIGHT); tft.print("Flame:");
    tft.setCursor(PADDING_LEFT, START_Y + 4 * LINE_HEIGHT); tft.print("Smoke:");
    tft.setCursor(PADDING_LEFT, START_Y + 5 * LINE_HEIGHT); tft.print("Status:");
    tft.setCursor(PADDING_LEFT, START_Y + 6 * LINE_HEIGHT); tft.print("WiFi:");
  }

  // Set value text size
  tft.setTextSize(3);
  int fh = tft.fontHeight();
  int rectH = fh + 12; // เพิ่มความสูงของพื้นที่ล้าง

  // Temperature
  if (!isnan(g_temp) && (isnan(last_temp) || g_temp != last_temp || fullRedraw)) {
    tft.fillRect(VALUE_COL_X, START_Y + 0 * LINE_HEIGHT, VALUE_WIDTH, rectH, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(VALUE_COL_X, START_Y + 0 * LINE_HEIGHT);
    tft.print(g_temp, 1); tft.print(" C");
    last_temp = g_temp;
  }

  // Humidity
  if (!isnan(g_hum) && (isnan(last_hum) || g_hum != last_hum || fullRedraw)) {
    tft.fillRect(VALUE_COL_X, START_Y + 1 * LINE_HEIGHT, VALUE_WIDTH, rectH, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(VALUE_COL_X, START_Y + 1 * LINE_HEIGHT);
    tft.print(g_hum, 1); tft.print(" %");
    last_hum = g_hum;
  }

  // MQ2 / Gas
  if (g_mq2 != last_mq2 || fullRedraw) {
    tft.fillRect(VALUE_COL_X, START_Y + 2 * LINE_HEIGHT, VALUE_WIDTH, rectH, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(VALUE_COL_X, START_Y + 2 * LINE_HEIGHT);
    if (g_mq2 >= 0) tft.print(g_mq2); else tft.print("--");
    tft.print(" ppm");
    last_mq2 = g_mq2;
  }

  // Flame
  if (g_flame != last_flame || fullRedraw) {
    tft.fillRect(VALUE_COL_X, START_Y + 3 * LINE_HEIGHT, VALUE_WIDTH, rectH, TFT_BLACK);
    tft.setTextColor(g_flame ? TFT_RED : TFT_GREEN, TFT_BLACK);
    tft.setCursor(VALUE_COL_X, START_Y + 3 * LINE_HEIGHT);
    tft.print(g_flame ? "DETECTED!" : "SAFE");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    last_flame = g_flame;
  }

  // Smoke
  if (g_smoke != last_smoke || fullRedraw) {
    tft.fillRect(VALUE_COL_X, START_Y + 4 * LINE_HEIGHT, VALUE_WIDTH, rectH, TFT_BLACK);
    tft.setTextColor(g_smoke ? TFT_RED : TFT_GREEN, TFT_BLACK);
    tft.setCursor(VALUE_COL_X, START_Y + 4 * LINE_HEIGHT);
    tft.print(g_smoke ? "DETECTED!" : "SAFE");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    last_smoke = g_smoke;
  }

  // Status string
  if (g_status != last_status || fullRedraw) {
    int statusRectW = VALUE_WIDTH + 30; // เพิ่มความกว้างสำหรับ status
    tft.fillRect(VALUE_COL_X, START_Y + 5 * LINE_HEIGHT, statusRectW, rectH, TFT_BLACK);

    uint16_t statusColor = TFT_YELLOW;
    if (g_status == "TEMP > 50C") statusColor = TFT_ORANGE;
    else if (g_status == "NORMAL") statusColor = TFT_GREEN;

    tft.setTextColor(statusColor, TFT_BLACK);
    tft.setCursor(VALUE_COL_X, START_Y + 5 * LINE_HEIGHT);
    String displayStatus = g_status;
    int maxChars = 20; // ลดจำนวนตัวอักษรให้เหมาะสม
    if (tft.textWidth(displayStatus) > statusRectW - 10) {
      while (tft.textWidth(displayStatus + "...") > statusRectW - 10 && displayStatus.length() > 0) {
        displayStatus = displayStatus.substring(0, displayStatus.length() - 1);
      }
      displayStatus += "...";
    }
    tft.print(displayStatus);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    last_status = g_status;
  }

  // WiFi
  bool wifiOk = WiFi.status() == WL_CONNECTED;
  if (wifiOk != last_wifi || fullRedraw) {
    tft.fillRect(VALUE_COL_X, START_Y + 6 * LINE_HEIGHT, VALUE_WIDTH, rectH, TFT_BLACK);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(VALUE_COL_X, START_Y + 6 * LINE_HEIGHT);
    tft.print(wifiOk ? "OK" : "ERROR");
    last_wifi = wifiOk;
  }
}

void displayStandardWarningScreen() {
  const int EDGE_MARGIN = 12;
  tft.fillScreen(TFT_RED);

  int w = tft.width();
  int h = tft.height();
  int centerX = w / 2;

  // Reserve top area for triangle
  int availableHeight = h - EDGE_MARGIN * 2;
  int triAreaH = max((int)(availableHeight * 0.55), 80);
  int textAreaH = availableHeight - triAreaH;

  int yTop = EDGE_MARGIN + 8;
  int yBottom = yTop + triAreaH - 8;
  int triHalf = min((yBottom - yTop), w / 4);
  if (triHalf < 24) triHalf = 24;

  // Draw triangle centered
  tft.fillTriangle(centerX, yTop, centerX - triHalf, yBottom, centerX + triHalf, yBottom, TFT_YELLOW);
  tft.drawTriangle(centerX, yTop, centerX - triHalf, yBottom, centerX + triHalf, yBottom, TFT_BLACK);

  // Exclamation mark
  int exSize = 7;
  tft.setTextSize(exSize);
  int exW = tft.textWidth("!");
  int exH = tft.fontHeight();
  int triW = triHalf * 2;
  int triH = yBottom - yTop;
  while ((exW > triW - 12 || exH > triH - 12) && exSize > 2) {
    exSize--;
    tft.setTextSize(exSize);
    exW = tft.textWidth("!");
    exH = tft.fontHeight();
  }
  int triCenterY = yTop + triH / 2;
  int exX = centerX - exW / 2;
  int exY = triCenterY - exH / 2;
  if (exY < yTop + 4) exY = yTop + 4;
  if (exY + exH > yBottom - 4) exY = yBottom - exH - 4;

  tft.setTextSize(exSize);
  tft.setTextColor(TFT_BLACK, TFT_YELLOW);
  tft.setCursor(exX, exY);
  tft.println("!");

  // Warning title and evac lines
  int curY = yBottom + 16; // เพิ่มระยะห่าง
  tft.setTextSize(4);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  String warning = (g_flame || g_status == "FIRE!") ? "FIRE DETECTED!" :
                   (g_smoke || g_status == "SMOKE / GAS HIGH") ? "SMOKE DETECTED!" : "EMERGENCY!";
  int wWarn = tft.textWidth(warning);
  tft.setCursor(centerX - wWarn / 2, curY);
  tft.println(warning);

  curY += tft.fontHeight() + 12; // เพิ่มระยะห่าง
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  String evac = "EVACUATE NOW";
  int wEvac = tft.textWidth(evac);
  tft.setCursor(centerX - wEvac / 2, curY);
  tft.println(evac);

  // Status band
  String statusLine = "Status: " + g_status;
  tft.setTextSize(3);
  int ws = tft.textWidth(statusLine);
  int bandH = tft.fontHeight() + 12;
  int bandW = min(w - EDGE_MARGIN * 2, ws + 40);
  int bandX = centerX - bandW / 2;
  int bandY = h - EDGE_MARGIN - bandH;
  if (bandY < curY + 16) {
    bandY = curY + 16;
    if (bandY + bandH + EDGE_MARGIN > h) bandY = h - EDGE_MARGIN - bandH;
  }

  tft.fillRect(bandX, bandY, bandW, bandH, TFT_YELLOW);
  tft.setTextColor(TFT_BLACK, TFT_YELLOW);
  int textY = bandY + (bandH - tft.fontHeight()) / 2;
  tft.setCursor(centerX - ws / 2, textY);
  String dispStatus = statusLine;
  if (tft.textWidth(dispStatus) > bandW - 10) {
    while (tft.textWidth(dispStatus + "...") > bandW - 10 && dispStatus.length() > 0) {
      dispStatus = dispStatus.substring(0, dispStatus.length() - 1);
    }
    dispStatus += "...";
  }
  tft.print(dispStatus);
}