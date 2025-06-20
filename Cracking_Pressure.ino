#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Pin definition
const uint8_t sensorPin = A0;
const uint8_t batteryPin = A1;
const uint8_t resetButtonPin = 2;
const uint8_t buttonGroundPin = 3;

// Button
unsigned long lastButtonDebounceTime = 0;
unsigned long buttonPressStartTime = 0;
bool isButtonPressed = false;
bool lastButtonState = false;
bool isShortPress = false;
bool isLongPress = false;

// Oversampling
uint16_t analogReadAverage(uint8_t pin, uint16_t samples = 64) {  // average 64 samples
  uint32_t total = 0;
  for (uint16_t i = 0; i < samples; i++) {
    total += analogRead(pin);
    delayMicroseconds(100);
  }
  return total / samples;
}

// Sampling
unsigned long lastSampleTime = 0;
float max_inchH2O = 0.0;

// Recording
float recordedPressure[200];  // 5 seconds at 40Hz
uint16_t recordIndex = 0;
bool plotDisplayed = false;

// Button State
void checkButton() {
  unsigned long currentTime = millis();
  bool currentButtonState = digitalRead(resetButtonPin) == LOW;

  if (currentButtonState != lastButtonState) {
    lastButtonDebounceTime = currentTime;  // Reset debounce timer
  }

  if ((currentTime - lastButtonDebounceTime) > 50) {  // Button debounce delay 50 ms
    if (currentButtonState) {  // Button pressed
      if (!isButtonPressed) {  // New press detected
        buttonPressStartTime = currentTime;
        isButtonPressed = true;
        isShortPress = false;
        isLongPress = false;
      } else if (!isLongPress && (currentTime - buttonPressStartTime >= 800)) {  // Long press threshold 800 ms
        isShortPress = false;
        isLongPress = true;
      }
    } else {  // Button is released
      if (isButtonPressed) {
        isButtonPressed = false;
        if (!isLongPress && (currentTime - buttonPressStartTime) <= 300) {  // Short press threshold 300 ms
          isShortPress = true;
        }
      }
    }
  }
  lastButtonState = currentButtonState;
}

// Show plot
void displayPlot() {
  display.clearDisplay();
  display.setTextSize(1);
  // Max value
  display.setCursor(38, 0);
  display.print(F("Max: "));
  display.print(max_inchH2O, 2);
  display.print(F(" inH2O"));
  // X-axis
  display.drawLine(8, 54, 127, 54, WHITE);
  display.setCursor(122, 46);
  display.print(F("T"));
  display.setCursor(8, 56);
  display.print(F("0"));
  display.setCursor(48, 56);
  display.print(F("1"));
  display.setCursor(88, 56);
  display.print(F("2"));
  display.setCursor(122, 56);
  display.print(F("3"));
  // Y-axis
  display.drawLine(8, 10, 8, 54, WHITE);
  display.setCursor(8, 0);
  display.print(F("P"));
  display.setCursor(0, 10);
  display.print(F("3"));
  display.setCursor(0, 23);
  display.print(F("2"));
  display.setCursor(0, 36);
  display.print(F("1"));
  display.setCursor(0, 50);
  display.print(F("0"));
  // Plot data
  int startIndex = (recordIndex + 200 - 80 - 120) % 200;  // Skip 2s before long press
  int prevX = -1, prevY = -1;
  for (uint16_t i = 0; i < 120; i++) {
    uint16_t bufferIndex = (recordIndex + i) % 200;
    int x = constrain(8 + i, 8, 127);
    int y = constrain(54 - (recordedPressure[bufferIndex] / 3.0 * 44), 10, 54);
    if (prevX != -1) {
      display.drawLine(prevX, prevY, x, y, WHITE);
    }
    prevX = x;
    prevY = y;
  }
  // Max line
  int maxPressureY = constrain(54 - (max_inchH2O / 3.0 * 44), 10, 54);
  for (int x = 8; x <= 127; x += 2) {
    display.drawPixel(x, maxPressureY, WHITE);
  }

  display.display();
  plotDisplayed = true;
}


void setup() {
  Wire.begin();
  Wire.setClock(400000); 
  pinMode(buttonGroundPin, OUTPUT);
  digitalWrite(buttonGroundPin, LOW);
  delay(10);
  pinMode(resetButtonPin, INPUT_PULLUP);
  analogReadResolution(12);

  // Initilisation
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(16, 8);
  display.print(F("Pressure"));
  display.setCursor(16, 40);
  display.print(F("Inch H2O"));
  display.display();
  delay(500);

  lastSampleTime = millis();
  recordIndex = 0;
}


void loop() {
  unsigned long currentTime = millis();

  checkButton();
  if (isShortPress) {  // Short press to reset recording
    isShortPress = false;
    if (plotDisplayed) plotDisplayed = false;
    max_inchH2O = 0;
    recordIndex = 0;
  }
  if (isLongPress) {  //  Long press to view plot
    isLongPress = false;
    displayPlot();
  }

  // Sampling
  if (currentTime - lastSampleTime >= 25 && !plotDisplayed) {  // 40 Hz

    // Pressure
    int16_t pressureRaw = analogReadAverage(sensorPin);
    float pressureVoltage = pressureRaw * (3.3 / 4095.0) * (5.3 / 3.3);  // 2k : 3.3k ohm voltage divider
    float pressure_kPa = (2.5 - pressureVoltage) / 2.0;  // 0.5V 1kPa, 4.5V -1kPa negative pressure
    float pressure_inchH2O = pressure_kPa * 4.01865;
    if (pressure_inchH2O > max_inchH2O) max_inchH2O = pressure_inchH2O;
    recordedPressure[recordIndex] = pressure_inchH2O;
    recordIndex = (recordIndex + 1) % 200;

    // Battery
    int16_t batteryRaw = analogReadAverage(batteryPin);
    float batteryVoltage = batteryRaw * (3.3 / 4095.0) * (13.3 / 10.0);  // 3.3k : 10k ohm voltage divider
    int batteryPercentage = round(123 - (123 / pow((1 + pow((batteryVoltage / 3.7), 80)), 0.165)));
    if (batteryPercentage > 100) batteryPercentage = 100;

    // Diplay
    display.clearDisplay();
    display.setTextSize(1);
    // Max pressure
    display.setCursor(0, 0);
    display.print(F("Max:"));
    display.print(max_inchH2O, 2);
    // Battery indicator
    display.drawRect(106, 0, 20, 8, WHITE);  // Main battery rectangle
    display.fillRect(126, 3, 2, 2, WHITE);  // Battery tip
    int16_t fillWidth = map(batteryPercentage, 0, 100, 0, 16);  // Battery bar width
    display.fillRect(108, 2, fillWidth, 4, WHITE);  // Fill battery bar
    // Gauge
    display.drawCircle(64, 63, 54, WHITE);
    // Tick
    int cx = 64;
    int cy = 63;
    int r_major_outer = 54;
    int r_major_inner = 49;
    int r_minor_outer = 54;
    int r_minor_inner = 51;
    int majorTickAngles[] = {180, 135, 90, 45, 0};
    for (int i = 0; i < 5; i++) {
      float angle = radians(majorTickAngles[i]);
      int x_outer = cx + cos(angle) * r_major_outer;
      int y_outer = cy - sin(angle) * r_major_outer;
      int x_inner = cx + cos(angle) * r_major_inner;
      int y_inner = cy - sin(angle) * r_major_inner;
      display.drawLine(x_outer, y_outer, x_inner, y_inner, WHITE);
    }
    int minorTickAngles[] = {158, 113, 68, 23};
    for (int i = 0; i < 4; i++) {
      float angle = radians(minorTickAngles[i]);
      int x_outer = cx + cos(angle) * r_minor_outer;
      int y_outer = cy - sin(angle) * r_minor_outer;
      int x_inner = cx + cos(angle) * r_minor_inner;
      int y_inner = cy - sin(angle) * r_minor_inner;
      display.drawLine(x_outer, y_outer, x_inner, y_inner, WHITE);
    }
    // Label
    display.drawLine(0, 60, 2, 60, WHITE);
    display.setCursor(4, 56);
    display.print(F("1"));
    display.setCursor(20, 16);
    display.print(F("0"));
    display.setCursor(61, 1);
    display.print(F("1"));
    display.setCursor(102, 16);
    display.print(F("2"));
    display.setCursor(122, 56);
    display.print(F("3"));
    // Needle
    pressure_inchH2O = constrain(pressure_inchH2O, -1, 3);
    float needleAngleDeg = map(pressure_inchH2O * 100, -100, 300, 180, 0);
    float needleAngleRad = radians(needleAngleDeg);
    int needleX = cx + cos(needleAngleRad) * r_major_inner;
    int needleY = cy - sin(needleAngleRad) * r_major_inner;
    display.drawLine(cx, cy, needleX, needleY, WHITE);
    display.fillCircle(cx, cy, 2, WHITE);
    // Value
    display.setCursor(49, 42);
    if (pressure_inchH2O >= 0) display.print(F("+"));
    display.print(pressure_inchH2O, 2);
    // Voltage
    display.setCursor(43, 52);
    display.print(F("("));
    display.print(pressureVoltage, 2);
    display.print(F("V)"));

    display.display();
    lastSampleTime = currentTime;
  }
}