#include <StateMachine.h>
#include "AsyncTaskLib.h"
#include <LiquidCrystal.h>
#include <Keypad.h>
#include "DHTStable.h"
/**
 * @file main.cpp
 * @brief Código principal del sistema de control de seguridad y monitoreo ambiental.
 * @author Miguel Angel Carvajal Ruiz
 * @author Cristhian Camilo Unas
 * @author Jhonny Richard Fuertes
 */

/**
* @brief Inicialización del objeto LiquidCrystal.
*/
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
/**
 * @brief Configuración del teclado.
*/
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {24, 26, 28, 30}; /**< Conectar a los pines de fila del teclado */
byte colPins[COLS] = {32, 34, 36, 38}; /**< Conectar a los pines de columna del teclado */

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

#define DHT11_PIN 6

/**
 * @brief Inicialización del objeto DHTStable.
*/
DHTStable DHT;

/**
 * @brief Definición de constantes y pines.
*/
#define BUZZER_SENSOR 13
const int photocellPin = A7;
const int Led_rojo = 49;
const int Led_verde = 51;
const int Led_azul = 53;

/** 
 * @brief Variables de contraseña y bloqueo.
*/
char password[6];
char contrasena[6] = "00000";
int idx = 0;
int contBloqueo = 3;
boolean enter = false;
int outputValue = 0;

/**
 * @brief Declaración de funciones de los estados.
*/
void securityEntryState(void);
void eventAmbiente(void);
void environmentalMonitoringState(void);
void environmentalAlarmState(void);
void handleSecurityAlert(void);

/**
 * @brief Declaración de funciones auxiliares para lectura de sensores.
*/
void readPhotoresistor(void);
void readTemp(void);

/**
 * @brief Declaración de tareas asíncronas.
*/
AsyncTask taskEnvironmentalMonitoring(2000, true, environmentalMonitoringState);
AsyncTask taskBeforeEnvironmentalMonitoring(3000, true, eventAmbiente);
AsyncTask taskAlarmAlert(3000, true, handleSecurityAlert);
AsyncTask taskEventAlarm(3000, true, eventAmbiente);

/**
 * @brief Declaración de la máquina de estados y sus estados.
*/
StateMachine machine = StateMachine();
State* stSecurityEntry = machine.addState(&securityEntryState);
State* stEventAmbiente = machine.addState(&eventAmbiente);
State* stEnvironmentalMonitoring = machine.addState(&environmentalMonitoringState);
State* stEnvironmentalAlarm = machine.addState(&environmentalAlarmState);
State* stHandleSecurityAlert = machine.addState(&handleSecurityAlert);

void setup() {
  Serial.begin(115200);
  pinMode(Led_rojo, OUTPUT);
  pinMode(Led_verde, OUTPUT);
  pinMode(Led_azul, OUTPUT);

/**
   * @brief Agregar transiciones entre estados.
*/
  stSecurityEntry->addTransition(&checkCorrectPassword, stEventAmbiente);
  stEventAmbiente->addTransition(&checkTemperature, stEnvironmentalMonitoring);
  
  stEnvironmentalMonitoring->addTransition(&checkTemperatureOver30, stEnvironmentalAlarm);
  stEnvironmentalMonitoring->addTransition(&checkTimeout10sec, stEventAmbiente);
  
  stHandleSecurityAlert->addTransition(&checkTimeout6sec, stEventAmbiente);
  //stEnvironmentalAlarm->addTransition(&checkTemperatureOver30_5s, stHandleSecurityAlert);
  stEnvironmentalAlarm->addTransition(&checkTemperatureOver30_5s, stEnvironmentalAlarm);
  stEnvironmentalAlarm->addTransition(&checkTemperatureBelow30, stEnvironmentalMonitoring);

/**
*  @brief Inicialización del objeto LiquidCrystal
*/
  lcd.begin(16, 2);

  lcd.setCursor(3, 0);
  lcd.print("Bienvenido");
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ingrese la clave");
  lcd.setCursor(5, 1);
}

void loop() {
  machine.run();
}

/**
* @brief Estado de ingreso de seguridad
*/
void securityEntryState() {
  Serial.println("Ingreso de Seguridad");
  char key = keypad.getKey();

  if (key) {
    password[idx] = key;
    idx++;
    lcd.print("*");
  }

  if (idx == 5) {
    bool isCorrectPassword = true;
    for (int i = 0; i < 5; i++) {
      if (password[i] != contrasena[i]) {
        isCorrectPassword = false;
        break;
      }
    }

    lcd.clear();
    processing();
    delay(300);
    lcd.clear();

    if (isCorrectPassword) {
      digitalWrite(Led_verde, HIGH);
      lcd.setCursor(2, 1);
      lcd.print("Bienvenido");
      digitalWrite(Led_verde, LOW);
    } else {
      contBloqueo--;
      if (contBloqueo > 0) {
        digitalWrite(Led_rojo, HIGH);
        lcd.setCursor(2, 0);
        lcd.print("cont.Incorrecta");
        delay(300);
        digitalWrite(Led_rojo, LOW);
        lcd.setCursor(0, 1);
        lcd.print("digite de nuevo");
        delay(2000);
        lcd.clear();
        lcd.setCursor(1, 0);
        lcd.print("Contrasenia");
        lcd.setCursor(5, 1);
      } else {
        lcd.setCursor(0, 0);
        lcd.println("Sistema");
        lcd.setCursor(0, 1);
        lcd.println("Bloqueado");
        digitalWrite(Led_azul, HIGH);
        delay(5000);
        lcd.clear();
        lcd.setCursor(1, 0);
        lcd.print("digite de nuevo");
        lcd.print("Contrasenia");
        lcd.setCursor(5, 1);
        contBloqueo=3;
        //exit(0);
      }
    }

    idx = 0;
  }
}

/**
 * @brief Función para verificar la contraseña ingresada.
 * @return true si la contraseña es correcta, false en caso contrario.
*/
bool checkCorrectPassword() {
  if (strcmp(password, contrasena) == 0) {
    taskEnvironmentalMonitoring.Start();
    return true;
  }
  return false;
}

/**
* @brief Estado ambiente que verifica cuando las entradas de aire afectan la luz o humedad/temperatura
*/
void eventAmbiente() {
  //Serial.println("Procesando ambiente");
  taskEnvironmentalMonitoring.Update();
}

/**
* @brief Función para verificar el tiempo de espera de 2 segundos
*/
bool checkTemperature() {

  /*
  if (!taskEnvironmentalMonitoring.IsActive()) {
    taskEnvironmentalMonitoring.Stop();
    taskEnvironmentalMonitoring.Reset();
    return true;
  }
  return false;
  */
  if (checkTemperatureOver30) {
    taskEnvironmentalMonitoring.Stop();
    taskEnvironmentalMonitoring.Reset();
    return true;
  }
  return false;
  
}

/**
* @brief Función para verificar si se activó el estado para el ambiente
*/
bool checkAmbienteActivated() {
  return true;
}

/**
* @brief Estado de monitoreo ambiental
*/
void environmentalMonitoringState() {
  Serial.println("Revisando estado del ambiente");
  readTemperatureAndPhotoresistor();
  taskBeforeEnvironmentalMonitoring.Stop();
  taskAlarmAlert.Start();
  taskBeforeEnvironmentalMonitoring.Reset();
}

/**
* @brief Función para verificar si la temperatura supera los 30 grados Celsius
*/
bool checkTemperatureOver30() {

  if (DHT.getTemperature() > 20) {
    taskBeforeEnvironmentalMonitoring.Stop();
    taskAlarmAlert.Start();
    taskBeforeEnvironmentalMonitoring.Reset();
    
    return true;
  }
  return false;
}

/**
* @brief Función para verificar el tiempo de espera de 10 segundos
*/
bool checkTimeout10sec() {
  if (!taskBeforeEnvironmentalMonitoring.IsActive()) {
    taskBeforeEnvironmentalMonitoring.Stop();
    taskBeforeEnvironmentalMonitoring.Reset();
    taskEnvironmentalMonitoring.Start();
    return true;
  }
  return false;
}

/**
* @brief Función para leer la temperatura y el valor del fotoresistor
*/
void readTemperatureAndPhotoresistor() {
  readTemp();
  readPhotoresistor();
  checkTemperatureOver30();
}

/**
* @brief Estado de alarma ambiental
*/
void environmentalAlarmState() {
  Serial.println("Alarma Ambiental");
  tone(BUZZER_SENSOR, 262, 250);
  taskAlarmAlert.Update();
}

/**
* @brief Función para verificar si la temperatura supera los 32 grados Celsius durante 5 segundos
*/
bool checkTemperatureOver30_5s() {
  Serial.println(!taskAlarmAlert.IsActive());
  if (!taskAlarmAlert.IsActive()) {
    taskAlarmAlert.Stop();
    taskAlarmAlert.Reset();
    taskEventAlarm.Reset();
    return true;
  }
  return false;
}

/**
* @brief Función para verificar si la temperatura está por debajo de los 30 grados Celsius
*/
bool checkTemperatureBelow30() {
  if (DHT.getTemperature() < 20) {
    taskAlarmAlert.Reset();
    return true;
  }
  return false;
}

/**
* @brief Estado de manejo de la alerta de seguridad
*/
void handleSecurityAlert() {
  Serial.println("Alerta de Seguridad");
  taskEventAlarm.Update();
}

/**
* @brief Función para verificar el tiempo de espera de 6 segundos
*/
bool checkTimeout6sec() {
  if (!taskEventAlarm.IsActive()) {
    taskEventAlarm.Stop();
    taskEventAlarm.Reset();
    taskEnvironmentalMonitoring.Start();
    return true;
  }
  return false;
}

/**
* @brief Función para mostrar "Procesando" en la pantalla LCD
*/
void processing() {
  for (int i = 0; i < 3; i++) {
    lcd.setCursor(2, 0);
    lcd.print("Verificando");
    lcd.setCursor(12, 0);
    delay(200);
    lcd.print(".");
    lcd.setCursor(13, 0);
    delay(200);
    lcd.print(".");
    lcd.setCursor(14, 0);
    delay(200);
    lcd.print(".");
    delay(200);
    lcd.clear();
  }
}

/**
* @brief Función para leer la temperatura
*/
void readTemp() {
  int chk = DHT.read11(DHT11_PIN);

  const char* errorMessages[] = {
    "OK",
    "Checksum error",
    "Time out error",
    "Unknown error"
  };

  if (chk >= 0 && chk < sizeof(errorMessages) / sizeof(errorMessages[0])) {
    Serial.print(errorMessages[chk]);
  } else {
    Serial.print("Invalid error code");
  }

  float humidity = DHT.getHumidity();
  float temperature = DHT.getTemperature();

  lcd.setCursor(0, 1);
  lcd.print("H%:");
  lcd.setCursor(4, 1);
  lcd.print(humidity, 1);
  Serial.print(",\t H:");
  Serial.print(humidity, 1);
  lcd.setCursor(9, 1);
  lcd.print("T:");
  lcd.setCursor(12, 1);
  lcd.print(temperature, 1);
  
  Serial.print("\t T:");
  Serial.print(temperature, 1);
  Serial.println();
}

/**
* @brief Función para leer el valor del fotoresistor
*/
void readPhotoresistor() {

  int lightIntensity = analogRead(photocellPin);
  lcd.setCursor(0, 0);
  lcd.print("Luz:");
  lcd.setCursor(5, 0);
  lcd.print(lightIntensity);
  Serial.print("Luz = ");
  Serial.println(lightIntensity);

  if (lightIntensity < 20) {
    Serial.println("LA LUZ ES MENOR A 20");
    Serial.println("LA LUZ ES MENOR A 20");
    Serial.println("LA LUZ ES MENOR A 20");
    Serial.println("LA LUZ ES MENOR A 20");
    Serial.println("LA LUZ ES MENOR A 20");
    Serial.println("LA LUZ ES MENOR A 20");
    Serial.println("Alarma Ambiental");
    tone(BUZZER_SENSOR, 262, 250);
    digitalWrite(Led_azul, HIGH);
  } else {
    digitalWrite(Led_azul, LOW);
  }
}