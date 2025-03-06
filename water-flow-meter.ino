/*
 * Complete Arduino Water Flow Meter
 * 
 * Features:
 * - Measures water flow rate
 * - Calculates total water volume
 * - Displays data on LCD
 * - Stores total volume in EEPROM
 * - Calibration mode
 * - Flow rate alarm
 * - Simple button interface
 * 
 * Hardware:
 * - Arduino Uno/Nano
 * - YF-S201 Water Flow Sensor
 * - 16x2 LCD with I2C
 * - 3 Push buttons (Mode, Up, Down)
 * - Buzzer for alarm
 * - LED indicators
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// Pin definitions
const int FLOW_SENSOR_PIN = 2;  // Flow sensor on interrupt pin
const int MODE_BUTTON_PIN = 3;  // Mode button
const int UP_BUTTON_PIN = 4;    // Up button
const int DOWN_BUTTON_PIN = 5;  // Down button
const int BUZZER_PIN = 6;       // Buzzer for alarm
const int LED_GREEN_PIN = 7;    // Green LED (normal operation)
const int LED_RED_PIN = 8;      // Red LED (alarm)

// EEPROM addresses
const int EEPROM_TOTAL_VOLUME_ADDR = 0;     // 4 bytes for total volume (float)
const int EEPROM_CALIBRATION_ADDR = 4;      // 4 bytes for calibration factor (float)
const int EEPROM_ALARM_THRESHOLD_ADDR = 8;  // 4 bytes for alarm threshold (float)

// Flow sensor variables
volatile int pulseCount = 0;
float calibrationFactor = 7.5;  // Default: YF-S201 = 7.5 pulses per liter
float flowRate = 0.0;           // L/min
float totalVolume = 0.0;        // Liters
float previousTotalVolume = 0.0;
unsigned long oldTime = 0;
const unsigned long updateInterval = 1000;  // Update every second

// Alarm variables
float alarmThreshold = 10.0;  // L/min
bool alarmActive = false;

// Display and UI variables
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Set I2C address to 0x27 for a 16x2 display
enum Mode { NORMAL, MENU, CALIBRATION, RESET, ALARM_SETTING };
Mode currentMode = NORMAL;
int menuPosition = 0;
const int menuItems = 3;  // Number of menu items
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;  // Debounce time in ms

// Function prototypes
void pulseCounter();
void updateFlowCalculations();
void updateDisplay();
void handleButtons();
void saveToEEPROM();
void loadFromEEPROM();
void handleMenu();
void calibrationMode();
void resetTotalVolume();
void setAlarmThreshold();
void checkAlarm();

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Water Flow Meter");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  
  // Initialize pins
  pinMode(FLOW_SENSOR_PIN, INPUT);
  digitalWrite(FLOW_SENSOR_PIN, HIGH);  // Enable internal pull-up
  
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(UP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DOWN_BUTTON_PIN, INPUT_PULLUP);
  
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  
  // Turn on green LED
  digitalWrite(LED_GREEN_PIN, HIGH);
  digitalWrite(LED_RED_PIN, LOW);
  
  // Attach interrupt to flow sensor
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);
  
  // Load saved values from EEPROM
  loadFromEEPROM();
  
  delay(2000);  // Display startup message for 2 seconds
  lcd.clear();
}

void loop() {
  // Update flow calculations every second
  if ((millis() - oldTime) > updateInterval) {
    updateFlowCalculations();
    oldTime = millis();
  }
  
  // Handle button presses and UI
  handleButtons();
  
  // Update display based on current mode
  updateDisplay();
  
  // Check if alarm should be triggered
  checkAlarm();
  
  // Save to EEPROM if volume has changed significantly
  if (abs(totalVolume - previousTotalVolume) > 0.1) {
    saveToEEPROM();
    previousTotalVolume = totalVolume;
  }
}

// Interrupt function for flow sensor
void pulseCounter() {
  pulseCount++;
}

// Calculate flow rate and total volume
void updateFlowCalculations() {
  // Disable interrupt temporarily
  detachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN));
  
  // Calculate flow rate (L/min) = (Pulse count / Calibration factor) * 60
  flowRate = ((float)pulseCount / calibrationFactor) * (60.0 / (updateInterval / 1000.0));
  
  // Calculate volume passed in this interval
  float volumeInterval = (float)pulseCount / calibrationFactor;
  
  // Add to total volume
  totalVolume += volumeInterval;
  
  // Reset pulse counter
  pulseCount = 0;
  
  // Re-enable interrupt
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);
  
  // Print to serial for debugging
  Serial.print("Flow rate: ");
  Serial.print(flowRate);
  Serial.print(" L/min, Total volume: ");
  Serial.print(totalVolume);
  Serial.println(" L");
}

// Update LCD display based on current mode
void updateDisplay() {
  lcd.clear();
  
  switch (currentMode) {
    case NORMAL:
      // Display flow rate and total volume
      lcd.setCursor(0, 0);
      lcd.print("Flow: ");
      lcd.print(flowRate, 2);
      lcd.print(" L/min");
      
      lcd.setCursor(0, 1);
      lcd.print("Total: ");
      lcd.print(totalVolume, 2);
      lcd.print(" L");
      break;
      
    case MENU:
      // Display menu
      lcd.setCursor(0, 0);
      lcd.print("MENU");
      
      lcd.setCursor(0, 1);
      switch (menuPosition) {
        case 0:
          lcd.print("> Calibration");
          break;
        case 1:
          lcd.print("> Reset Total");
          break;
        case 2:
          lcd.print("> Alarm Setting");
          break;
      }
      break;
      
    case CALIBRATION:
      // Display calibration screen
      lcd.setCursor(0, 0);
      lcd.print("Calibration");
      
      lcd.setCursor(0, 1);
      lcd.print("Factor: ");
      lcd.print(calibrationFactor, 2);
      break;
      
    case RESET:
      // Display reset confirmation
      lcd.setCursor(0, 0);
      lcd.print("Reset Total?");
      
      lcd.setCursor(0, 1);
      lcd.print("UP:Yes  DOWN:No");
      break;
      
    case ALARM_SETTING:
      // Display alarm setting screen
      lcd.setCursor(0, 0);
      lcd.print("Alarm Threshold");
      
      lcd.setCursor(0, 1);
      lcd.print("Rate: ");
      lcd.print(alarmThreshold, 2);
      lcd.print(" L/min");
      break;
  }
}

// Handle button presses
void handleButtons() {
  // Debounce buttons
  if (millis() - lastButtonPress < debounceTime) {
    return;
  }
  
  // Mode button
  if (digitalRead(MODE_BUTTON_PIN) == LOW) {
    lastButtonPress = millis();
    
    if (currentMode == NORMAL) {
      // Enter menu
      currentMode = MENU;
      menuPosition = 0;
    } else if (currentMode == MENU) {
      // Select menu item
      switch (menuPosition) {
        case 0:
          currentMode = CALIBRATION;
          break;
        case 1:
          currentMode = RESET;
          break;
        case 2:
          currentMode = ALARM_SETTING;
          break;
      }
    } else {
      // Return to normal mode from any other mode
      currentMode = NORMAL;
      saveToEEPROM();  // Save any changes
    }
  }
  
  // Up button
  if (digitalRead(UP_BUTTON_PIN) == LOW) {
    lastButtonPress = millis();
    
    switch (currentMode) {
      case MENU:
        // Navigate menu up
        menuPosition = (menuPosition > 0) ? menuPosition - 1 : menuItems - 1;
        break;
        
      case CALIBRATION:
        // Increase calibration factor
        calibrationFactor += 0.1;
        break;
        
      case RESET:
        // Confirm reset
        totalVolume = 0.0;
        saveToEEPROM();
        currentMode = NORMAL;
        break;
        
      case ALARM_SETTING:
        // Increase alarm threshold
        alarmThreshold += 0.5;
        break;
    }
  }
  
  // Down button
  if (digitalRead(DOWN_BUTTON_PIN) == LOW) {
    lastButtonPress = millis();
    
    switch (currentMode) {
      case MENU:
        // Navigate menu down
        menuPosition = (menuPosition < menuItems - 1) ? menuPosition + 1 : 0;
        break;
        
      case CALIBRATION:
        // Decrease calibration factor (minimum 0.1)
        calibrationFactor = max(0.1, calibrationFactor - 0.1);
        break;
        
      case RESET:
        // Cancel reset
        currentMode = NORMAL;
        break;
        
      case ALARM_SETTING:
        // Decrease alarm threshold (minimum 0)
        alarmThreshold = max(0.0, alarmThreshold - 0.5);
        break;
    }
  }
}

// Save values to EEPROM
void saveToEEPROM() {
  // Save total volume
  EEPROM.put(EEPROM_TOTAL_VOLUME_ADDR, totalVolume);
  
  // Save calibration factor
  EEPROM.put(EEPROM_CALIBRATION_ADDR, calibrationFactor);
  
  // Save alarm threshold
  EEPROM.put(EEPROM_ALARM_THRESHOLD_ADDR, alarmThreshold);
  
  Serial.println("Saved to EEPROM");
}

// Load values from EEPROM
void loadFromEEPROM() {
  // Load total volume
  EEPROM.get(EEPROM_TOTAL_VOLUME_ADDR, totalVolume);
  previousTotalVolume = totalVolume;
  
  // Load calibration factor (use default if not set)
  float storedCalibration;
  EEPROM.get(EEPROM_CALIBRATION_ADDR, storedCalibration);
  if (storedCalibration > 0.1) {
    calibrationFactor = storedCalibration;
  }
  
  // Load alarm threshold (use default if not set)
  float storedThreshold;
  EEPROM.get(EEPROM_ALARM_THRESHOLD_ADDR, storedThreshold);
  if (storedThreshold > 0.0) {
    alarmThreshold = storedThreshold;
  }
  
  Serial.println("Loaded from EEPROM");
  Serial.print("Total volume: ");
  Serial.print(totalVolume);
  Serial.print(" L, Calibration: ");
  Serial.print(calibrationFactor);
  Serial.print(", Alarm: ");
  Serial.println(alarmThreshold);
}

// Check if alarm should be triggered
void checkAlarm() {
  if (flowRate > alarmThreshold) {
    if (!alarmActive) {
      alarmActive = true;
      digitalWrite(LED_GREEN_PIN, LOW);
      digitalWrite(LED_RED_PIN, HIGH);
      tone(BUZZER_PIN, 1000);  // 1kHz tone
    }
  } else {
    if (alarmActive) {
      alarmActive = false;
      digitalWrite(LED_GREEN_PIN, HIGH);
      digitalWrite(LED_RED_PIN, LOW);
      noTone(BUZZER_PIN);
    }
  }
}

