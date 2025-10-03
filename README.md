# 🔥 IoT Fire Alarm System – ESP8266 + MQ-2 + Flame + DHT22 + TFT + Firebase/Blynk

## ดูคลิปรีวิวอุปกรณ์ได้ที่ https://youtu.be/foR5ZaKeqn0?si=HviKViOOjZy2VsCi 

ระบบต้นแบบตรวจจับและแจ้งเตือนไฟไหม้แบบครบวงจร ใช้บอร์ด **ESP8266** สองตัว ได้แก่  

- **Sensor Node** : อ่านค่าเซนเซอร์ (MQ-2, Flame, DHT22), ควบคุม LED/บัซเซอร์ และส่งข้อมูลขึ้น **Firebase RTDB** และ **Blynk**  
- **Display Node** : ดึงข้อมูลสถานะล่าสุดจาก **Firebase RTDB** มาแสดงบนจอ **TFT 3.5\" (TFT_eSPI)**

---

## 📂 โครงสร้างโปรเจกต์

```
/ (Arduino sketches)
├─ CodeSensor.ino        # โค้ด Sensor Node + Firebase + Blynk + Alarm Logic
└─ CodeDisplayLCD.ino    # โค้ด Display Node + TFT_eSPI + Firebase Reader + UI
```

---
![553060630_670841895583296_6913829591042675090_n](https://github.com/user-attachments/assets/604172ed-e18a-42dc-b8aa-9ce1334759c4)


## ⚙️ ฟังก์ชันการทำงาน

### 🛰 Sensor Node (`CodeSensor.ino`)
- อ่านค่า:
  - **DHT22** (อุณหภูมิ/ความชื้น) – D5
  - **MQ-2** Gas sensor – A0 (Analog), D6 (Digital)
  - **Flame sensor** – D0 (*Active Low*)
- อุปกรณ์แจ้งเตือน:
  - **LED**: RED (D4), GREEN (D3) (*Active Low*)
  - **Passive Buzzer**: D7, ความถี่เตือน 2000 Hz
- เกณฑ์ตรวจจับ:
  - MQ-2 Analog > 450
  - Temp > 50 °C
  - Flame detected (ถือค่า 3 วินาทีลด false alarm)
- การสื่อสาร:
  - **Blynk**: อัปเดตค่า (V0–V9) และรับคำสั่งควบคุม (V10–V12)
  - **Firebase RTDB**: บันทึก `/sensor_data`, `/current_status`, `/alerts`
  - **NTP Time**: ซิงค์ UTC+7

### 📺 Display Node (`CodeDisplayLCD.ino`)
- ดึง JSON `/current_status` จาก Firebase ทุก 1 วินาที
- แสดงผลบน **TFT_eSPI 3.5\"**
  - โหมดปกติ: ตารางค่า (Temp, Humidity, Gas, Flame, Smoke, Status, Wi-Fi)
  - โหมดเตือน: แสดงหน้าจอแดง + ไอคอนเตือน + ข้อความ **EVACUATE NOW**
- จัดการ Wi-Fi อัตโนมัติ, แสดง Wi-Fi OK/ERROR
- สีข้อความเปลี่ยนตามสถานะ (เขียว/ส้ม/เหลือง)

---

## 🔌 การต่อวงจร (Sensor Node)

| อุปกรณ์        | Pin ESP8266 | หมายเหตุ |
|-----------------|-------------|-----------|
| DHT22           | D5          | Data |
| MQ-2 Analog     | A0          | ต้องมีวงจรแบ่งแรงดัน (0–1V) |
| MQ-2 Digital    | D6          | Digital Output |
| Flame Sensor    | D0          | Active-Low, อาจต้อง pull-up ภายนอก |
| LED RED         | D4          | Active-Low |
| LED GREEN       | D3          | Active-Low |
| Buzzer          | D7          | Passive buzzer |

---

## ☁️ Firebase RTDB – โครงสร้างข้อมูล

```json
/sensor_data/1727938123456: {
  "timestamp": "2025-10-03 12:34:56",
  "temperature": 29.3,
  "humidity": 64.1,
  "mq2_analog": 512,
  "mq2_digital": 0,
  "flame_raw": 1,
  "smoke_detected": false,
  "flame_detected": false,
  "status": "NORMAL"
}

/current_status: {
  "timestamp": "2025-10-03 12:34:56",
  "temperature": 29.3,
  "humidity": 64.1,
  "mq2_analog": 512,
  "flame_detected": false,
  "smoke_detected": false,
  "status": "NORMAL"
}
```

---

## 📱 Blynk Virtual Pins

| Virtual Pin | ความหมาย |
|-------------|-----------|
| V0–V9       | ค่าเซนเซอร์ & สถานะ |
| V10         | ควบคุม LED แดง |
| V11         | ควบคุม LED เขียว |
| V12         | ควบคุม Buzzer |

---

## 🛠 ไลบรารีที่ใช้

- `DHT sensor library`
- `Blynk`
- `FirebaseESP8266`
- `TFT_eSPI`
- `time.h`

---

## 🚀 วิธีใช้งาน

1. ตั้งค่า Firebase RTDB และเปิดใช้งาน Legacy Token  
2. ใส่ `ssid`, `pass`, `FIREBASE_HOST`, `FIREBASE_AUTH`, `BLYNK_AUTH_TOKEN`  
3. อัปโหลด `CodeSensor.ino` ไปยัง ESP8266 (Sensor Node)  
4. อัปโหลด `CodeDisplayLCD.ino` ไปยัง ESP8266 + TFT (Display Node)  
5. เปิด Serial Monitor ตรวจสอบการเชื่อมต่อ  
6. ทดสอบด้วยควัน/เปลวไฟ (ในที่ปลอดภัย) และตรวจสอบการแจ้งเตือน

---

## ⚠️ ข้อควรระวัง

- MQ-2 A0 ต้องไม่เกิน 1.0V → ใช้วงจรแบ่งแรงดัน
- Flame Sensor อาจต้องเพิ่ม pull-up ภายนอก
- หลีกเลี่ยงใช้ D3/D8 ดึงลงขณะบูต (boot-strap pins)
- เกณฑ์ Temp (50°C) และ MQ2 Threshold ควรปรับตามสภาพจริง
- ทดลองไฟ/ควันในพื้นที่ควบคุม พร้อมอุปกรณ์ดับเพลิง

---

## 📝 Roadmap ปรับปรุง

- Moving Average filter ลด noise
- Thresholds อ่านจาก Firebase (ปรับสดได้)
- Offline buffer + sync ย้อนหลัง
- Push Notification (ผ่าน FCM)
- อัปเดตระบบ Auth ของ Firebase ให้ทันสมัย

