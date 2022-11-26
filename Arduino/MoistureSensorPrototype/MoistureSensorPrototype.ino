// 
// Grundaufbau des Systems:
//
// 1. Mehrere unabhängige Arduinos mit jeweils bis zu 6 Sensoren
// 
// Jeder Arduino hat 1 Taster und je Sensor 1 LED. Bei Tastendruck wird für jeden Sensor, 
// welcher einen "zu trockenen" Wert zurückgibt, die LED aktiviert.
// Der Arduino gibt per CAN-Bus mindestens 1x pro Minute aus:
//   - Die aktuellen Werte jedes Sensors
//   - Die Schnellwertkalibrierung jedes Sensors
// Der Arduino hat ein Konsoleninterface, mit dem die Kalibration eingestellt werden kann.
// Die Sensoren müssen wie folgt angeschlossen sein:
//  - VCC jedes Sensors an Pin "5V" am Arduino
//  - GND jedes Sensors an Pin "GND" am Arduino
//  - AOut der Sensoren an die Pins "A0" bis "A5" am Arduino
// Das CAN-Modul MCP2515 wird so angeschlossen wie unter
// https://github.com/autowp/arduino-mcp2515 dokumentiert


//
// 2. Zentraler Arduino als Anzeigetafel
// 
// Dieser Arduino soll anzeigen, auf welchem Beet gegossen werden muss (d. h. wo mindestens ein Sensor "zu trocken" ist). 
// Zu diesem Zweck wird auf dem CAN-Bus "mitgelauscht".


// https://github.com/autowp/arduino-mcp2515 --> Auch zur Verkabelung des CAN-Moduls am Arduino nutzen!
#include <mcp2515.h>
struct can_frame canMsg1;
struct can_frame canMsg2;
MCP2515 mcp2515(10);

// Der persistente Speicher wird genutzt
#include <inttypes.h>
#include <avr/eeprom.h>
#include <avr/io.h>

#define EEPROM_MAGIC 0x76
#define DEFAULT_ID 0x13

uint16_t threshold[6];
uint8_t device_id;

void write_threshold(int i_sensor, uint16_t value) {
  eeprom_write_byte((uint8_t*)(1+2*i_sensor), (uint8_t)(value % 256)); // % == "Modulo"
  eeprom_write_byte((uint8_t*)(2+2*i_sensor), (uint8_t)(value / 256));  
};


void setup() {
  // Start der seriellen Kommunikation
  Serial.begin(9600);
  Serial.println("Neustart");
  // Setup der digitalen Pins
  pinMode(4, OUTPUT); // Anzeige für Sensor 1
  pinMode(5, OUTPUT); // f. Sensor 2
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT); // f. Sensor 6
  pinMode(3, INPUT_PULLUP); // f. Taster
  // Nutzung des persistenten Speichers um Schwellenwerte einzugeben
  byte magicbyte = eeprom_read_byte(0);
  // Der Speicher kann nur byteweise (0 bis 255) angesprochen werden, unsere Zahlenwerte sind aber bis zu 2 Byte (0 bis 16383) groß.
  // Deshalb sind die Werte in zwei Kommandos anzusprechen.
  if (magicbyte != EEPROM_MAGIC) {
    // Wenn Speicher noch leer, wird die Zahl "470" als Defaultwert hineingeschrieben.
    eeprom_write_byte(0, EEPROM_MAGIC);
    for (uint8_t i = 0; i < 6; i++) {
      write_threshold(i, 475);
    }
    eeprom_write_byte(13, DEFAULT_ID);
  }
  // Aus dem Speicher werden nun für jeden Sensor die hinterlegten Werte ausgelesen 
  // (ggf. auch die gerade gespeicherten Default-Werte)
  for (uint8_t i = 0; i < 6; i++) {
    threshold[i] = ((uint16_t)eeprom_read_byte((uint8_t*)(1+2*i))) + ((uint16_t)eeprom_read_byte((uint8_t*)(2+2*i)))*256;
  }
  device_id = eeprom_read_byte(13);
  
// Initialisierung CAN-Message zur Kommunikation mit anderen Geräten
  canMsg1.can_id = "0x40";
  canMsg1.can_dlc = 7;

  mcp2515.reset();
  mcp2515.setBitrate(CAN_5KBPS);
  mcp2515.setNormalMode();
    
// Die Geräteinformationen werden ausgegeben
  showinfo();
}


// Methode zum Ausgeben von Werten
void print_values(uint16_t* values) {
  
  for (int i = 0; i < 6; i++) { // i = 0,1,2,3,4,5
    Serial.print("S");
    Serial.print((char)(0x31+i)); // ASCII: (0x30 = '0'), 0x31 = '1', 0x32 = '2', ...
    Serial.print(": ");
    Serial.print(values[i]);
    Serial.print(" ");
  }
  Serial.println();
}


uint8_t pause = 0;

void loop() {
  // Auslesen der aktuellen Sensorwerte, bis zu 6 Sensoren (Capacitive Soil
  // Moisture Sensor v1.2 (Schwingkreis))
  uint16_t sensorwert[6];
  sensorwert[0] = analogRead(A0);
  sensorwert[1] = analogRead(A1);
  sensorwert[2] = analogRead(A2);
  sensorwert[3] = analogRead(A3);
  sensorwert[4] = analogRead(A4);
  sensorwert[5] = analogRead(A5);
  if (pause)
    pause--;
  else
    print_values(sensorwert);
  
  bool led_ein = (digitalRead(3) == LOW);

  for (uint8_t i = 0; i < 6; i++) { // i = 0,1,2,3,4,5
    bool trocken = sensorwert[i] > threshold[i];
    if (trocken && led_ein) {
      // falls Boden trocken und Taster gedrückt
      digitalWrite(4+i, HIGH); // für Sensor "i+1" die LED einschalten
    } 
    else
      digitalWrite(4+i, LOW); // ansonsten die LED ausschalten

    // Angelehnt an das Beispiel für die Bibliothek mcp2515 
    canMsg1.data[0] = device_id;
    canMsg1.data[1] = i;
    canMsg1.data[2] = trocken ? 0x0f : 0x00;
    canMsg1.data[3] = sensorwert[i] % 256;
    canMsg1.data[4] = sensorwert[i] / 256;
    canMsg1.data[5] = threshold[i] % 256;
    canMsg1.data[6] = threshold[i] / 256;
    mcp2515.sendMessage(&canMsg1);
  }
  delay(1000);
  shellinterface();
}

void showinfo() {
  // Die Geräte-ID wird angezeigt
    Serial.print("Geräte-ID: ");
    Serial.println(device_id);
    // Die Schwellwerte anzeigen unter Nutzung der Auslesemethode
    Serial.print("Schwellenwerte: ");
    print_values(threshold);
    Serial.print("-------------\n");
}

void shellinterface() {
  String line = Serial.readString();
  // Leere Eingaben zurückgeben
  if (line.length() == 0)
    return;
  // Zeilenende löschen
  if (line[line.length()-1] == '\n')
    line = line.substring(0, line.length()-1);
  else if (line[line.length()-1] == '\r')
    line = line.substring(0, line.length()-1);
  // Für Debuggingzwecke die Eingabe anzeigen
  Serial.print("Received: ");
  Serial.println(line);
  // Ab hier werden die Kommandos definiert:
  
  if (line == "show") {
    showinfo();
  }
  else if (line == "pause") {
    Serial.println("Stoppe Ausgabe für 60 Sekunden.");
    pause = 60;
    // Dieser Wert wird in der Loop-Methode dann jede Sekunde heruntergezählt
  }
  else if (line.startsWith("set S") && line.length() > 7 && line[6] == '=') {
    // Sensorkanal auslesen, das ASCII-Zeichen für "0" abziehen um direkt die Zahl von 1 bis 6 zu erhalten
    // Außerdem 1 abziehen, da die Sensor-Nummern 1 bis 6 den internen Sensornummern 0-5 entsprechen.
    int i_sensor = line[5] - '0' - 1;
    int new_threshold = line.substring(7).toInt();
    Serial.print("Setze den Schwellwert von Sensor S");
    Serial.print(i_sensor+1);
    Serial.print(" von ");
    Serial.print(threshold[i_sensor]);
    Serial.print(" auf ");
    Serial.println(new_threshold);
    threshold[i_sensor] = new_threshold;
    write_threshold(i_sensor, new_threshold);
  }
  else if (line.startsWith("set ID=")) {
    int new_id = line.substring(7).toInt();
    
    Serial.print("Setze die Geraete-ID von ");
    Serial.print(device_id);
    Serial.print(" auf ");
    Serial.println(new_id);
    device_id = new_id;
    eeprom_write_byte(13, new_id);
  }  
  else Serial.println("Kommando \"" + line + "\" nicht erkannt.");
}
