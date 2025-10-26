#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------------- LCD I2C ----------------
LiquidCrystal_I2C lcd(0x27, 16, 2);  // ถ้าไม่ขึ้น ลองเปลี่ยนเป็น 0x3F

// ---------------- พิน ----------------
const int PIN_MQ2     = A0;
const int PIN_LED_G   = 4;
const int PIN_LED_Y   = 5;
const int PIN_LED_R   = 6;
const int PIN_BUZZER  = 3;

// ---------------- เกณฑ์ระดับเตือน ----------------
const int TH_YELLOW_HIGH = 500;
const int TH_YELLOW_LOW  = 300;
const int TH_RED_HIGH    = 700;
const int TH_RED_LOW     = 500;

// ---------------- จังหวะกระพริบ / เวลา ----------------
const unsigned long BLINK_SLOW_INTERVAL = 600;  // เหลือง
const unsigned long BLINK_FAST_INTERVAL = 150;  // แดง

// ---------------- โทนเสียง ----------------
const int TONE_SAFE     = 0;     // ปิดเสียง
const int TONE_CAUTION  = 1200;  // ระดับเหลือง
const int TONE_DANGER_1 = 800;   // แดง (เสียงเตือนคู่)
const int TONE_DANGER_2 = 1800;

// ---------------- สถานะระบบ ----------------
enum Level { SAFE=0, CAUTION=1, DANGER=2 };
Level currentLevel = SAFE;

// ตัวแปรกระพริบและเสียง
unsigned long lastBlinkMillis = 0;
bool blinkState = false;

unsigned long lastBeepMillis = 0;
bool beepState = false;
unsigned long beepInterval = 0;
bool dangerSoundPlayed = false;

void setup() {
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_Y, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_MQ2,   INPUT);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("MQ-2 Gas Monitor");
  lcd.setCursor(0, 1); lcd.print("Initializing...");
  delay(1000);
  lcd.clear();
}

void loop() {
  int raw = analogRead(PIN_MQ2);
  Level prevLevel = currentLevel;
  Level nextLevel = currentLevel;

  // ---------- ตรวจระดับด้วยฮิสเทอรี ----------
  switch (currentLevel) {
    case SAFE:
      if (raw >= TH_RED_HIGH) nextLevel = DANGER;
      else if (raw >= TH_YELLOW_HIGH) nextLevel = CAUTION;
      break;
    case CAUTION:
      if (raw >= TH_RED_HIGH) nextLevel = DANGER;
      else if (raw <= TH_YELLOW_LOW) nextLevel = SAFE;
      break;
    case DANGER:
      if (raw <= TH_RED_LOW) nextLevel = CAUTION;
      break;
  }
  currentLevel = nextLevel;

  // ---------- ถ้าเพิ่งเข้าสู่โหมดใหม่ ----------
  if (currentLevel != prevLevel) {
    noTone(PIN_BUZZER);
    dangerSoundPlayed = false; // รีเซ็ตเสียงอันตราย
  }

  updateOutputs(raw, currentLevel);
  drawLCD(raw, currentLevel);

  delay(50);
}

// ------------------------------------------------------------
// จัดการ LED + เสียง
// ------------------------------------------------------------
void updateOutputs(int raw, Level level) {
  unsigned long now = millis();
  bool ledG = LOW, ledY = LOW, ledR = LOW;

  if (level == SAFE) {
    // เขียวติดนิ่ง
    ledG = HIGH;
    noTone(PIN_BUZZER);
    blinkState = false;
    beepState = false;

  } else if (level == CAUTION) {
    // ---------- ไฟเหลืองกระพริบ ----------
    if (now - lastBlinkMillis >= BLINK_SLOW_INTERVAL) {
      blinkState = !blinkState;
      lastBlinkMillis = now;
    }
    ledY = blinkState;

    // ---------- เสียง beep ช้า ----------
    if (now - lastBeepMillis >= 800) {
      tone(PIN_BUZZER, TONE_CAUTION, 200);
      lastBeepMillis = now;
    }

  } else if (level == DANGER) {
    // ---------- ไฟแดงกระพริบถี่ ----------
    if (now - lastBlinkMillis >= BLINK_FAST_INTERVAL) {
      blinkState = !blinkState;
      lastBlinkMillis = now;
    }
    ledR = blinkState;

    // ---------- เสียงเตือนคู่ (beep beep) ----------
    if (!dangerSoundPlayed && now - lastBeepMillis > 1000) {
      //dangerMelody(); // เล่นเมโลดี้เตือนครั้งเดียวเมื่อเข้าโหมด DANGER
      dangerSoundPlayed = true;
      lastBeepMillis = now;
    } else {
      // เสียงเตือนสั้นต่อเนื่อง
      tone(PIN_BUZZER, TONE_DANGER_1, 150);
      delay(100);
      tone(PIN_BUZZER, TONE_DANGER_2, 150);
      delay(100);
      noTone(PIN_BUZZER);
    }
  }

  digitalWrite(PIN_LED_G, ledG);
  digitalWrite(PIN_LED_Y, ledY);
  digitalWrite(PIN_LED_R, ledR);
}

// ------------------------------------------------------------
// แสดงผลบน LCD
// ------------------------------------------------------------
void drawLCD(int raw, Level level) {
  lcd.setCursor(0, 0);
  lcd.print("Gas: ");
  printPadded(raw, 4);
  lcd.print("     ");

  lcd.setCursor(0, 1);
  lcd.print("Level: ");
  switch (level) {
    case SAFE:    lcd.print("SAFE     "); break;
    case CAUTION: lcd.print("CAUTION  "); break;
    case DANGER:  lcd.print("DANGER   "); break;
  }
}

// ------------------------------------------------------------
// เมโลดี้เมื่อเข้าสู่โหมดอันตราย (Danger)
// ------------------------------------------------------------
void dangerMelody() {
  int melody[] = { 1000, 1500, 800, 1800 };
  int duration[] = { 200, 200, 200, 400 };

  for (int i = 0; i < 4; i++) {
    tone(PIN_BUZZER, melody[i]);
    delay(duration[i]);
    noTone(PIN_BUZZER);
    delay(80);
  }
}

// ------------------------------------------------------------
// ฟังก์ชันพิมพ์ตัวเลขให้จัดตำแหน่งตรงบน LCD
// ------------------------------------------------------------
void printPadded(int value, int width) {
  int digits = (value == 0) ? 1 : (int)log10(value) + 1;
  for (int i = 0; i < width - digits; i++) lcd.print(' ');
  lcd.print(value);
}
