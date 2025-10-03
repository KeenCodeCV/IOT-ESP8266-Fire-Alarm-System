#define BLYNK_TEMPLATE_ID           "TMPL6jEbaN0M3"
#define BLYNK_TEMPLATE_NAME         "Quickstart Template"
#define BLYNK_AUTH_TOKEN            "bREQ5eSPxbvl110c3kEGvskTrtbYI9lZ"
#define BLYNK_PRINT Serial

#include <Arduino.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <FirebaseESP8266.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <time.h>

char ssid[] = "IICE-WiFI";
char pass[] = "admin@123";

BlynkTimer timer;

#define FIREBASE_HOST "lab10-f138a-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "0afFMvUAuYZKK6X39t2sBk4vuraP9hdg5fwJyA8a"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastFirebaseUpdate = 0;
const unsigned long FIREBASE_INTERVAL = 1000; // เปลี่ยนเป็น 1 วินาที

float g_temp = NAN, g_hum = NAN;
int   g_mqAO = 0, g_mqDO = 1;
int   g_flameRaw = HIGH;
bool  g_smokeDetected = false;
bool  g_flameDetected = false;
bool  g_flameAlarm = false;
bool  g_buzzerState = false;
String g_statusText = "init";

#define DHTPIN D5
#define DHTTYPE DHT22
#define MQ2_DO D6
#define MQ2_AO A0
#define FLAME_PIN D0
#define ALARM_PIN D7
#define LED_RED D4
#define LED_GRN D3

const bool LED_ACTIVE_LOW = true;
const bool FLAME_ACTIVE_LOW = true;

int MQ2_ANALOG_THRESHOLD = 450;

const uint16_t BUZZ_FREQ = 2000;
const unsigned long BEEP_ON  = 1000;
const unsigned long BEEP_OFF = 1000;

bool buzzerState = false;
unsigned long buzzerLastMs = 0;

const unsigned long FLAME_HOLD_MS = 3000;
unsigned long flameLastSeenMs = 0;

DHT dht(DHTPIN, DHTTYPE);

inline void ledWrite(uint8_t pin, bool on) {
    if (LED_ACTIVE_LOW) digitalWrite(pin, on ? LOW : HIGH);
    else digitalWrite(pin, on ? HIGH : LOW);
}

inline void redOn()  { ledWrite(LED_RED, true); }
inline void redOff() { ledWrite(LED_RED, false); }
inline void grnOn()  { ledWrite(LED_GRN, true); }
inline void grnOff() { ledWrite(LED_GRN, false); }
inline void allOff() { redOff(); grnOff(); }

void startBeepPattern() {
    unsigned long now = millis();
    unsigned long interval = buzzerState ? BEEP_ON : BEEP_OFF;

    if (now - buzzerLastMs >= interval) {
        buzzerState = !buzzerState;
        buzzerLastMs = now;
    }

    if (buzzerState) tone(ALARM_PIN, BUZZ_FREQ);
    else noTone(ALARM_PIN);
}

void stopBeepPattern() {
    noTone(ALARM_PIN);
    buzzerState = false;
    buzzerLastMs = millis();
}

void initTime() {
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Waiting for NTP time sync: ");
    time_t nowSecs = time(nullptr);
    while (nowSecs < 8 * 3600 * 2) {
        delay(500);
        Serial.print(".");
        yield();
        nowSecs = time(nullptr);
    }
    Serial.println();
    struct tm timeinfo;
    gmtime_r(&nowSecs, &timeinfo);
    Serial.print("Current time: ");
    Serial.print(asctime(&timeinfo));
}

String getFormattedDateTime() {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char dateTimeString[30];
    sprintf(dateTimeString, "%04d-%02d-%02d %02d:%02d:%02d", 
            timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    return String(dateTimeString);
}

void sendToFirebase() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected, skipping Firebase update");
        return;
    }

    String timestamp = getFormattedDateTime();
    String path = "/sensor_data/" + String(millis());
    
    FirebaseJson json;
    json.set("timestamp", timestamp);
    json.set("temperature", g_temp);
    json.set("humidity", g_hum);
    json.set("mq2_analog", g_mqAO);
    json.set("mq2_digital", g_mqDO);
    json.set("flame_raw", g_flameRaw);
    json.set("smoke_detected", g_smokeDetected);
    json.set("flame_detected", g_flameDetected);
    json.set("flame_alarm", g_flameAlarm);
    json.set("buzzer_state", g_buzzerState);
    json.set("status", g_statusText);
    
    if (Firebase.setJSON(fbdo, path, json)) {
        Serial.println("Firebase data sent successfully");
        Serial.println("Path: " + fbdo.dataPath());
        Serial.println("Type: " + fbdo.dataType());
    } else {
        Serial.println("Firebase send failed");
        Serial.println("Reason: " + fbdo.errorReason());
    }
    
    String currentPath = "/current_status";
    if (Firebase.setJSON(fbdo, currentPath, json)) {
        Serial.println("Current status updated");
    } else {
        Serial.println("Current status update failed");
        Serial.println("Reason: " + fbdo.errorReason());
    }
    
    if (g_flameAlarm || g_smokeDetected || g_temp > 50.0) {
        String alertPath = "/alerts/" + String(millis());
        FirebaseJson alertJson;
        alertJson.set("timestamp", timestamp);
        alertJson.set("alert_type", g_flameAlarm ? "FIRE" : (g_smokeDetected ? "SMOKE" : "HIGH_TEMP"));
        alertJson.set("temperature", g_temp);
        alertJson.set("humidity", g_hum);
        alertJson.set("mq2_analog", g_mqAO);
        alertJson.set("status", g_statusText);
        
        if (Firebase.setJSON(fbdo, alertPath, alertJson)) {
            Serial.println("Alert logged to Firebase");
        } else {
            Serial.println("Alert log failed");
            Serial.println("Reason: " + fbdo.errorReason());
        }
    }
}

void pushToBlynk() {
    Blynk.virtualWrite(V0, g_temp);
    Blynk.virtualWrite(V1, g_hum);
    Blynk.virtualWrite(V2, g_mqAO);
    Blynk.virtualWrite(V3, g_mqDO);
    Blynk.virtualWrite(V4, g_flameRaw);
    Blynk.virtualWrite(V5, g_smokeDetected);
    Blynk.virtualWrite(V6, g_flameDetected);
    Blynk.virtualWrite(V7, g_flameAlarm);
    Blynk.virtualWrite(V8, g_buzzerState);
    Blynk.virtualWrite(V9, g_statusText);
}

BLYNK_WRITE(V10) {  // ควบคุม LED แดง
    int value = param.asInt();
    if (value) redOn();
    else redOff();
}

BLYNK_WRITE(V11) {  // ควบคุม LED เขียว
    int value = param.asInt();
    if (value) grnOn();
    else grnOff();
}

BLYNK_WRITE(V12) {  // ควบคุม Buzzer
    int value = param.asInt();
    if (value) tone(ALARM_PIN, BUZZ_FREQ);
    else noTone(ALARM_PIN);
}

void setup() {
    Serial.begin(115200);

    pinMode(MQ2_DO, INPUT);
    pinMode(FLAME_PIN, INPUT_PULLUP);
    pinMode(ALARM_PIN, OUTPUT);
    noTone(ALARM_PIN);
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GRN, OUTPUT);
    allOff();

    dht.begin();

    Serial.println(F("ESP8266 DHT22 + MQ-2 + Flame + 2-LED (RED=D4, GRN=D3) started"));

    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("WiFi connected");
    Serial.println("IP address: " + WiFi.localIP().toString());

    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    timer.setInterval(1000L, pushToBlynk);

    config.database_url = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    Firebase.setwriteSizeLimit(fbdo, "tiny");
    
    initTime();
    
    Serial.println("Firebase initialized");
    Serial.println("Current time: " + getFormattedDateTime());
    
    sendToFirebase();
}

void loop() {
    Blynk.run();
    timer.run();

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
        Serial.println(F("อ่านค่า DHT22 ไม่สำเร็จ"));
        delay(100);
        return;
    }

    Serial.printf("Temp: %.1f C Hum: %.1f %%\n", t, h);

    int mqAnalog = analogRead(MQ2_AO);
    int mqDigital = digitalRead(MQ2_DO);
    bool smokeDetected = (mqAnalog >= MQ2_ANALOG_THRESHOLD) || (mqDigital == LOW);

    Serial.printf("MQ2 AO: %d DO: %d -> smoke=%d\n", mqAnalog, mqDigital, smokeDetected);

    int flameRaw = digitalRead(FLAME_PIN);
    bool flameDetected = (flameRaw == (FLAME_ACTIVE_LOW ? LOW : HIGH));

    unsigned long now = millis();
    if (flameDetected) flameLastSeenMs = now;

    bool flameAlarm = flameDetected || (now - flameLastSeenMs <= FLAME_HOLD_MS);

    Serial.printf("Flame DO: %d -> flame=%d (hold=%d)\n", flameRaw, flameDetected, flameAlarm);

    allOff();

    if (flameAlarm) {
        startBeepPattern();
        if (buzzerState) {
            redOn();
            grnOn();
        }
        Serial.println(flameDetected ? F("!!! พบ 'เปลวไฟ' !!!") : F("!! Flame cleared: holding alarm 3s !!"));
        g_statusText = flameDetected ? "FIRE!" : "Flame hold (3s)";

    } else if (smokeDetected) {
        startBeepPattern();
        if (buzzerState) redOn();
        Serial.println(F("!! ควัน/ก๊าซสูง !!"));
        g_statusText = "SMOKE / GAS HIGH";

    } else if (t > 50.0) {
        startBeepPattern();
        if (buzzerState) grnOn();
        Serial.println(F("! อุณหภูมิสูงเกิน 50°C !"));
        g_statusText = "TEMP > 50C";

    } else {
        stopBeepPattern();
        grnOn();
        g_statusText = "NORMAL";
    }

    g_temp = t;
    g_hum = h;
    g_mqAO = mqAnalog;
    g_mqDO = mqDigital;
    g_flameRaw = flameRaw;
    g_smokeDetected = smokeDetected;
    g_flameDetected = flameDetected;
    g_flameAlarm = flameAlarm;
    g_buzzerState = buzzerState;

    if (now - lastFirebaseUpdate >= FIREBASE_INTERVAL) {
        sendToFirebase();
        lastFirebaseUpdate = now;
    }

    delay(50);
}
