/*
 * Sistema Completo de Monitoramento de Água
 * Com Medidor de Fluxo e Sensor de Nível da Caixa d'Água
 * 
 * Recursos:
 * - Medição de fluxo de água
 * - Cálculo de volume total
 * - Monitoramento do nível da caixa d'água
 * - Sistema de alertas
 * - Armazenamento em EEPROM
 * - Interface LCD
 * 
 * Hardware:
 * - Arduino Mega/Uno
 * - Sensor de fluxo YF-S201
 * - Sensor ultrassônico HC-SR04 (para nível da caixa)
 * - Display LCD 20x4 com I2C
 * - Botões de navegação
 * - LEDs indicadores
 * - Buzzer
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <NewPing.h>

// Definição de pinos
const int FLOW_SENSOR_PIN = 2;  // Sensor de fluxo (interrupção)
const int TRIG_PIN = 9;         // Sensor ultrassônico - Trigger
const int ECHO_PIN = 10;        // Sensor ultrassônico - Echo
const int BUTTON_MENU = 3;      // Botão de menu
const int BUTTON_UP = 4;        // Botão para cima
const int BUTTON_DOWN = 5;      // Botão para baixo
const int BUTTON_SELECT = 6;    // Botão de seleção
const int LED_GREEN = 7;        // LED verde (operação normal)
const int LED_YELLOW = 8;       // LED amarelo (alerta médio)
const int LED_RED = 11;         // LED vermelho (alerta crítico)
const int BUZZER_PIN = 12;      // Buzzer para alarmes

// Configurações do sensor ultrassônico
const int MAX_DISTANCE = 200;   // Distância máxima em cm
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

// Configurações da caixa d'água
const int TANK_HEIGHT = 100;    // Altura da caixa em cm
int tankCapacity = 1000;        // Capacidade da caixa em litros
int tankAlertLow = 20;          // Alerta de nível baixo (%)
int tankAlertMid = 50;          // Alerta de nível médio (%)
int tankAlertHigh = 90;         // Alerta de nível alto (%)
float tankLevel = 0;            // Nível atual da caixa (%)
bool tankAlertsEnabled = true;  // Alertas da caixa ativados

// Endereços EEPROM
const int EEPROM_TOTAL_VOLUME_ADDR = 0;     // 4 bytes para volume total (float)
const int EEPROM_CALIBRATION_ADDR = 4;      // 4 bytes para fator de calibração (float)
const int EEPROM_TANK_CAPACITY_ADDR = 8;    // 2 bytes para capacidade da caixa (int)
const int EEPROM_TANK_ALERT_LOW_ADDR = 10;  // 1 byte para alerta baixo (int)
const int EEPROM_TANK_ALERT_MID_ADDR = 11;  // 1 byte para alerta médio (int)
const int EEPROM_TANK_ALERT_HIGH_ADDR = 12; // 1 byte para alerta alto (int)
const int EEPROM_DAILY_USAGE_ADDR = 13;     // 4 bytes para uso diário (float)
const int EEPROM_LAST_DAY_ADDR = 17;        // 1 byte para último dia registrado

// Variáveis do sensor de fluxo
volatile int pulseCount = 0;
float calibrationFactor = 7.5;  // Padrão: YF-S201 = 7.5 pulsos por litro
float flowRate = 0.0;           // L/min
float totalVolume = 0.0;        // Litros
float previousTotalVolume = 0.0;
float dailyUsage = 0.0;         // Uso diário em litros
unsigned long oldTime = 0;
const unsigned long updateInterval = 1000;  // Atualizar a cada segundo

// Variáveis de detecção de vazamento
bool flowDetected = false;
unsigned long flowStartTime = 0;
const unsigned long leakDetectionTime = 21600000; // 6 horas em milissegundos
bool leakDetected = false;

// Display e interface
LiquidCrystal_I2C lcd(0x27, 20, 4);  // Display 20x4 no endereço 0x27
enum Mode { MAIN, MENU, FLOW_INFO, TANK_INFO, SETTINGS, TANK_SETTINGS, ALERTS };
Mode currentMode = MAIN;
int menuPosition = 0;
int menuItems = 5;  // Número de itens no menu
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;  // Tempo de debounce em ms

// Protótipos de funções
void pulseCounter();
void updateFlowCalculations();
void updateTankLevel();
void checkAlerts();
void updateDisplay();
void handleButtons();
void saveToEEPROM();
void loadFromEEPROM();
void checkDayChange();
void resetDailyUsage();
void checkLeaks();

void setup() {
  // Inicializa comunicação serial
  Serial.begin(9600);
  
  // Inicializa LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sistema de");
  lcd.setCursor(0, 1);
  lcd.print("Monitoramento de Agua");
  lcd.setCursor(0, 2);
  lcd.print("Inicializando...");
  
  // Inicializa pinos
  pinMode(FLOW_SENSOR_PIN, INPUT);
  digitalWrite(FLOW_SENSOR_PIN, HIGH);  // Ativa pull-up interno
  
  pinMode(BUTTON_MENU, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Ativa LED verde
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  
  // Anexa interrupção ao sensor de fluxo
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);
  
  // Carrega valores salvos da EEPROM
  loadFromEEPROM();
  
  delay(2000);  // Exibe mensagem de inicialização por 2 segundos
  lcd.clear();
}

void loop() {
  // Atualiza cálculos de fluxo a cada segundo
  if ((millis() - oldTime) > updateInterval) {
    updateFlowCalculations();
    updateTankLevel();
    checkAlerts();
    checkLeaks();
    checkDayChange();
    oldTime = millis();
  }
  
  // Trata pressionamentos de botões e interface
  handleButtons();
  
  // Atualiza display com base no modo atual
  updateDisplay();
  
  // Salva na EEPROM se o volume mudou significativamente
  if (abs(totalVolume - previousTotalVolume) > 0.1) {
    saveToEEPROM();
    previousTotalVolume = totalVolume;
  }
}

// Função de interrupção para o sensor de fluxo
void pulseCounter() {
  pulseCount++;
}

// Calcula vazão e volume total
void updateFlowCalculations() {
  // Desativa interrupção temporariamente
  detachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN));
  
  // Calcula vazão (L/min) = (Contagem de pulsos / Fator de calibração) * 60
  flowRate = ((float)pulseCount / calibrationFactor) * (60.0 / (updateInterval / 1000.0));
  
  // Calcula volume passado neste intervalo
  float volumeInterval = (float)pulseCount / calibrationFactor;
  
  // Adiciona ao volume total e uso diário
  totalVolume += volumeInterval;
  dailyUsage += volumeInterval;
  
  // Detecta se há fluxo para monitoramento de vazamentos
  if (pulseCount > 0) {
    if (!flowDetected) {
      flowDetected = true;
      flowStartTime = millis();
    }
  } else {
    flowDetected = false;
  }
  
  // Reseta contador de pulsos
  pulseCount = 0;
  
  // Reativa interrupção
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);
  
  // Imprime no serial para depuração
  Serial.print("Vazao: ");
  Serial.print(flowRate);
  Serial.print(" L/min, Volume total: ");
  Serial.print(totalVolume);
  Serial.println(" L");
}

// Atualiza o nível da caixa d'água usando o sensor ultrassônico
void updateTankLevel() {
  // Faz 3 leituras e calcula a média para maior precisão
  int distance = 0;
  for (int i = 0; i < 3; i++) {
    delay(50);
    distance += sonar.ping_cm();
  }
  distance /= 3;
  
  // Se a distância for 0 (sem eco) ou maior que a altura da caixa, ajusta
  if (distance == 0 || distance > TANK_HEIGHT) {
    distance = TANK_HEIGHT;
  }
  
  // Calcula o nível em porcentagem (invertido, pois a distância é do topo)
  tankLevel = 100.0 * (1.0 - ((float)distance / TANK_HEIGHT));
  
  Serial.print("Nivel da caixa: ");
  Serial.print(tankLevel);
  Serial.println("%");
}

// Verifica alertas baseados no nível da caixa e vazão
void checkAlerts() {
  // Reseta todos os LEDs
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  noTone(BUZZER_PIN);
  
  // Verifica nível da caixa
  if (tankLevel <= tankAlertLow) {
    // Nível crítico
    digitalWrite(LED_RED, HIGH);
    if (tankAlertsEnabled) {
      tone(BUZZER_PIN, 2000, 500);  // Tom agudo por 500ms
    }
  } else if (tankLevel <= tankAlertMid) {
    // Nível médio-baixo
    digitalWrite(LED_YELLOW, HIGH);
  } else {
    // Nível normal
    digitalWrite(LED_GREEN, HIGH);
  }
  
  // Se houver vazamento detectado, pisca o LED vermelho
  if (leakDetected) {
    if ((millis() / 500) % 2 == 0) {  // Pisca a cada 500ms
      digitalWrite(LED_RED, HIGH);
      if (tankAlertsEnabled) {
        tone(BUZZER_PIN, 1500, 200);
      }
    }
  }
}

// Verifica se há vazamentos (fluxo contínuo por muito tempo)
void checkLeaks() {
  if (flowDetected && (millis() - flowStartTime > leakDetectionTime)) {
    leakDetected = true;
  }
}

// Verifica mudança de dia para resetar o uso diário
void checkDayChange() {
  // Obter o dia atual (simplificado - em sistemas reais usaria RTC)
  int currentDay = (millis() / 86400000) % 30;  // Aproximação do dia do mês
  
  // Verificar se o dia mudou
  int lastDay;
  EEPROM.get(EEPROM_LAST_DAY_ADDR, lastDay);
  
  if (currentDay != lastDay) {
    resetDailyUsage();
    EEPROM.put(EEPROM_LAST_DAY_ADDR, currentDay);
  }
}

// Reseta o uso diário
void resetDailyUsage() {
  dailyUsage = 0.0;
  EEPROM.put(EEPROM_DAILY_USAGE_ADDR, dailyUsage);
}

// Atualiza o display LCD com base no modo atual
void updateDisplay() {
  lcd.clear();
  
  switch (currentMode) {
    case MAIN:
      // Tela principal com informações resumidas
      lcd.setCursor(0, 0);
      lcd.print("Vazao: ");
      lcd.print(flowRate, 1);
      lcd.print(" L/min");
      
      lcd.setCursor(0, 1);
      lcd.print("Nivel: ");
      lcd.print(tankLevel, 1);
      lcd.print("%");
      
      lcd.setCursor(0, 2);
      lcd.print("Hoje: ");
      lcd.print(dailyUsage, 1);
      lcd.print(" L");
      
      lcd.setCursor(0, 3);
      lcd.print("Total: ");
      lcd.print(totalVolume, 1);
      lcd.print(" L");
      break;
      
    case MENU:
      // Menu principal
      lcd.setCursor(0, 0);
      lcd.print("MENU PRINCIPAL");
      
      for (int i = 0; i < 3; i++) {
        if (menuPosition + i < menuItems) {
          lcd.setCursor(0, i + 1);
          if (i == 0) lcd.print(">");
          else lcd.print(" ");
          
          switch (menuPosition + i) {
            case 0:
              lcd.print("Informacoes de Fluxo");
              break;
            case 1:
              lcd.print("Info. Caixa d'Agua");
              break;
            case 2:
              lcd.print("Configuracoes");
              break;
            case 3:
              lcd.print("Config. Caixa d'Agua");
              break;
            case 4:
              lcd.print("Alertas");
              break;
          }
        }
      }
      break;
      
    case FLOW_INFO:
      // Informações detalhadas de fluxo
      lcd.setCursor(0, 0);
      lcd.print("INFORMACOES DE FLUXO");
      
      lcd.setCursor(0, 1);
      lcd.print("Vazao: ");
      lcd.print(flowRate, 2);
      lcd.print(" L/min");
      
      lcd.setCursor(0, 2);
      lcd.print("Uso Hoje: ");
      lcd.print(dailyUsage, 2);
      lcd.print(" L");
      
      lcd.setCursor(0, 3);
      lcd.print("Total: ");
      lcd.print(totalVolume, 2);
      lcd.print(" L");
      break;
      
    case TANK_INFO:
      // Informações detalhadas da caixa d'água
      lcd.setCursor(0, 0);
      lcd.print("INFO. CAIXA D'AGUA");
      
      lcd.setCursor(0, 1);
      lcd.print("Nivel: ");
      lcd.print(tankLevel, 1);
      lcd.print("%");
      
      // Calcula volume atual na caixa
      float currentVolume = (tankLevel / 100.0) * tankCapacity;
      lcd.setCursor(0, 2);
      lcd.print("Volume: ");
      lcd.print(currentVolume, 0);
      lcd.print("/");
      lcd.print(tankCapacity);
      lcd.print("L");
      
      // Status da caixa
      lcd.setCursor(0, 3);
      lcd.print("Status: ");
      if (tankLevel <= tankAlertLow) {
        lcd.print("CRITICO");
      } else if (tankLevel <= tankAlertMid) {
        lcd.print("MEDIO");
      } else if (tankLevel <= tankAlertHigh) {
        lcd.print("BOM");
      } else {
        lcd.print("CHEIO");
      }
      break;
      
    case SETTINGS:
      // Configurações gerais
      lcd.setCursor(0, 0);
      lcd.print("CONFIGURACOES");
      
      lcd.setCursor(0, 1);
      lcd.print("Calibracao: ");
      lcd.print(calibrationFactor, 1);
      
      lcd.setCursor(0, 2);
      lcd.print("Detec. Vazamento: ");
      lcd.print(leakDetectionTime / 3600000);
      lcd.print("h");
      
      lcd.setCursor(0, 3);
      lcd.print("UP/DOWN para ajustar");
      break;
      
    case TANK_SETTINGS:
      // Configurações da caixa d'água
      lcd.setCursor(0, 0);
      lcd.print("CONFIG. CAIXA D'AGUA");
      
      lcd.setCursor(0, 1);
      lcd.print("Capacidade: ");
      lcd.print(tankCapacity);
      lcd.print("L");
      
      lcd.setCursor(0, 2);
      lcd.print("Alertas: ");
      lcd.print(tankAlertLow);
      lcd.print("/");
      lcd.print(tankAlertMid);
      lcd.print("/");
      lcd.print(tankAlertHigh);
      lcd.print("%");
      
      lcd.setCursor(0, 3);
      lcd.print("Alertas: ");
      lcd.print(tankAlertsEnabled ? "ATIVADOS" : "DESATIVADOS");
      break;
      
    case ALERTS:
      // Tela de alertas
      lcd.setCursor(0, 0);
      lcd.print("ALERTAS");
      
      lcd.setCursor(0, 1);
      if (leakDetected) {
        lcd.print("! VAZAMENTO DETECTADO !");
      } else {
        lcd.print("Sem vazamentos");
      }
      
      lcd.setCursor(0, 2);
      if (tankLevel <= tankAlertLow) {
        lcd.print("! NIVEL CRITICO !");
      } else if (tankLevel <= tankAlertMid) {
        lcd.print("Nivel medio");
      } else {
        lcd.print("Nivel normal");
      }
      
      lcd.setCursor(0, 3);
      lcd.print("SELECT: Resetar alertas");
      break;
  }
}

// Trata pressionamentos de botões
void handleButtons() {
  // Debounce de botões
  if (millis() - lastButtonPress < debounceTime) {
    return;
  }
  
  // Botão de menu
  if (digitalRead(BUTTON_MENU) == LOW) {
    lastButtonPress = millis();
    
    if (currentMode == MAIN) {
      // Entra no menu
      currentMode = MENU;
      menuPosition = 0;
    } else {
      // Volta para a tela principal
      currentMode = MAIN;
    }
  }
  
  // Botão para cima
  if (digitalRead(BUTTON_UP) == LOW) {
    lastButtonPress = millis();
    
    switch (currentMode) {
      case MENU:
        // Navega para cima no menu
        menuPosition = (menuPosition > 0) ? menuPosition - 1 : 0;
        break;
        
      case SETTINGS:
        // Aumenta fator de calibração
        calibrationFactor += 0.1;
        break;
        
      case TANK_SETTINGS:
        // Aumenta capacidade da caixa
        tankCapacity += 100;
        if (tankCapacity > 10000) tankCapacity = 10000;
        break;
    }
  }
  
  // Botão para baixo
  if (digitalRead(BUTTON_DOWN) == LOW) {
    lastButtonPress = millis();
    
    switch (currentMode) {
      case MENU:
        // Navega para baixo no menu
        menuPosition = (menuPosition < menuItems - 1) ? menuPosition + 1 : menuItems - 1;
        break;
        
      case SETTINGS:
        // Diminui fator de calibração (mínimo 0.1)
        calibrationFactor = max(0.1, calibrationFactor - 0.1);
        break;
        
      case TANK_SETTINGS:
        // Diminui capacidade da caixa
        tankCapacity -= 100;
        if (tankCapacity < 100) tankCapacity = 100;
        break;
    }
  }
  
  // Botão de seleção
  if (digitalRead(BUTTON_SELECT) == LOW) {
    lastButtonPress = millis();
    
    if (currentMode == MENU) {
      // Seleciona item do menu
      switch (menuPosition) {
        case 0:
          currentMode = FLOW_INFO;
          break;
        case 1:
          currentMode = TANK_INFO;
          break;
        case 2:
          currentMode = SETTINGS;
          break;
        case 3:
          currentMode = TANK_SETTINGS;
          break;
        case 4:
          currentMode = ALERTS;
          break;
      }
    } else if (currentMode == ALERTS) {
      // Reseta alertas
      leakDetected = false;
      flowDetected = false;
    } else if (currentMode == TANK_SETTINGS) {
      // Alterna alertas da caixa
      tankAlertsEnabled = !tankAlertsEnabled;
    }
  }
}

// Salva valores na EEPROM
void saveToEEPROM() {
  // Salva volume total
  EEPROM.put(EEPROM_TOTAL_VOLUME_ADDR, totalVolume);
  
  // Salva fator de calibração
  EEPROM.put(EEPROM_CALIBRATION_ADDR, calibrationFactor);
  
  // Salva configurações da caixa
  EEPROM.put(EEPROM_TANK_CAPACITY_ADDR, tankCapacity);
  EEPROM.put(EEPROM_TANK_ALERT_LOW_ADDR, tankAlertLow);
  EEPROM.put(EEPROM_TANK_ALERT_MID_ADDR, tankAlertMid);
  EEPROM.put(EEPROM_TANK_ALERT_HIGH_ADDR, tankAlertHigh);
  
  // Salva uso diário
  EEPROM.put(EEPROM_DAILY_USAGE_ADDR, dailyUsage);
  
  Serial.println("Dados salvos na EEPROM");
}

// Carrega valores da EEPROM
void loadFromEEPROM() {
  // Carrega volume total
  EEPROM.get(EEPROM_TOTAL_VOLUME_ADDR, totalVolume);
  previousTotalVolume = totalVolume;
  
  // Carrega fator de calibração (usa padrão se não estiver definido)
  float storedCalibration;
  EEPROM.get(EEPROM_CALIBRATION_ADDR, storedCalibration);
  if (storedCalibration > 0.1) {
    calibrationFactor = storedCalibration;
  }
  
  // Carrega configurações da caixa
  int storedCapacity;
  EEPROM.get(EEPROM_TANK_CAPACITY_ADDR, storedCapacity);
  if (storedCapacity >= 100 && storedCapacity <= 10000) {
    tankCapacity = storedCapacity;
  }
  
  byte storedLow, storedMid, storedHigh;
  EEPROM.get(EEPROM_TANK_ALERT_LOW_ADDR, storedLow);
  EEPROM.get(EEPROM_TANK_ALERT_MID_ADDR, storedMid);
  EEPROM.get(EEPROM_TANK_ALERT_HIGH_ADDR, storedHigh);
  
  if (storedLow > 0 && storedLow < 100) tankAlertLow = storedLow;
  if (storedMid > 0 && storedMid < 100) tankAlertMid = storedMid;
  if (storedHigh > 0 && storedHigh < 100) tankAlertHigh = storedHigh;
  
  // Carrega uso diário
  EEPROM.get(EEPROM_DAILY_USAGE_ADDR, dailyUsage);
  
  Serial.println("Dados carregados da EEPROM");
}

