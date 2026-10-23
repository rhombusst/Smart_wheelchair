#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define TOUCH_PIN D0

LiquidCrystal_I2C lcd(0x27, 16, 2);

byte heart1[8] = {
  0b00000,
  0b01010,
  0b11111,
  0b11111,
  0b11111,
  0b01110,
  0b00100,
  0b00000
};

byte heart2[8] = {
  0b00000,
  0b00000,
  0b01010,
  0b11111,
  0b11111,
  0b01110,
  0b00100,
  0b00000
};

void setup() {
  Serial.begin(115200);

  pinMode(TOUCH_PIN, INPUT);

  Wire.begin(D2, D1);
  lcd.init();
  lcd.backlight();

  lcd.createChar(0, heart1);
  lcd.createChar(1, heart2);

  randomSeed(analogRead(A0));

  // Startup animation
  lcd.setCursor(0, 0);
  lcd.print("  PULSE SCANNER");
  lcd.setCursor(5, 1);
  lcd.print("BOOTING");

  for (int i = 0; i < 3; i++) {
    lcd.setCursor(12 + i, 1);
    lcd.print(".");
    delay(400);
  }

  lcd.clear();
}

void loop() {
  // Waiting screen
  lcd.setCursor(0, 0);
  lcd.print("  PULSE SCANNER");
  lcd.setCursor(0, 1);
  lcd.print("Touch to scan!");

  // Wait for touch
  while (digitalRead(TOUCH_PIN) == LOW) {
    delay(20);
  }

  // Scanning animation: 3 seconds
  unsigned long start = millis();
  int frame = 0;

  while (millis() - start < 3000) {
    lcd.setCursor(0, 0);
    lcd.print(" SCANNING NOW  ");

    lcd.setCursor(0, 1);
    lcd.print("Please wait ");

    for (int i = 0; i < frame; i++) {
      lcd.print(".");
    }
    for (int i = frame; i < 3; i++) {
      lcd.print(" ");
    }

    lcd.setCursor(15, 0);
    lcd.write((frame % 2) ? byte(0) : byte(1));

    frame = (frame + 1) % 4;
    delay(250);
  }

  // Generate simulated pulse
  int pulse = random(60, 141);

  // Result screen
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" PULSE DETECTED");
  lcd.setCursor(0, 1);
  lcd.write(byte(0));
  lcd.print(" ");
  lcd.print(pulse);
  lcd.print(" BPM ");

  Serial.println("----------------");
  Serial.println("PULSE SCAN DONE");
  Serial.print("Pulse: ");
  Serial.print(pulse);
  Serial.println(" BPM");
  Serial.println("----------------");

  // Keep result for 5 seconds
  unsigned long resultStart = millis();

  while (millis() - resultStart < 5000) {
    lcd.setCursor(0, 1);
    lcd.write((millis() / 400 % 2) ? byte(0) : byte(1));
    delay(100);
  }

  // Wait until finger is removed
  while (digitalRead(TOUCH_PIN) == HIGH) {
    delay(20);
  }

  lcd.clear();
}
