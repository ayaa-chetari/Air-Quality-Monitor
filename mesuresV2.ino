#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_NeoPixel.h>

// === Configuration capteurs et LEDs ===
#define LED_PIN 21
#define NUM_LEDS 12
#define LED_CONFORT 6   // 6 premières LEDs
#define LED_POLLUTION 6 // 6 dernières LEDs

Adafruit_BME280 bmeSensor;
Adafruit_NeoPixel ledStrip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// === Énumération qualité ===
enum Level { POOR = 0, WARNING = 1, MEDIUM = 2, GOOD = 3 };

// Structure pour stocker les données PMS7003
struct AirParticles {
  uint16_t pm1;
  uint16_t pm2_5;
  uint16_t pm10;
};
AirParticles airData;

// === Fonction pour lire le PMS7003 sur Serial1 ===
bool readPMS(AirParticles &data) {
  static uint8_t buffer[32];
  static uint8_t ptr = 0;

  while (Serial1.available()) {
    uint8_t byteReceived = Serial1.read();

    // Détecter le début de trame
    if (ptr == 0 && byteReceived != 0x42) continue;
    if (ptr == 1 && byteReceived != 0x4D) { ptr = 0; continue; }

    buffer[ptr++] = byteReceived;

    if (ptr == 32) {
      data.pm1   = (buffer[10] << 8) | buffer[11];
      data.pm2_5 = (buffer[12] << 8) | buffer[13];
      data.pm10  = (buffer[14] << 8) | buffer[15];
      ptr = 0;
      return true;
    }
  }
  return false;
}

// === Évaluation de la pollution (PM2.5) ===
Level evaluatePollution(float pm25) {
  if (pm25 < 12) return GOOD;
  else if (pm25 < 25) return MEDIUM;
  else if (pm25 < 50) return WARNING;
  else return POOR;
}

// === Évaluation du confort (temp + humidité) ===
Level evaluateConfort(float temp, float hum) {
  // Score température
  int tempScore = (temp > 20 && temp < 25) ? 3 :
                  (temp > 17 && temp < 28) ? 2 :
                  (temp > 10 && temp < 32) ? 1 : 0;

  // Score humidité
  int humScore = (hum > 40 && hum < 60) ? 3 :
                 (hum > 30 && hum < 70) ? 2 :
                 (hum > 20 && hum < 80) ? 1 : 0;

  int score = (tempScore + humScore) / 2; // moyenne
  return static_cast<Level>(score);
}

// === Couleur selon le niveau ===
uint32_t levelColor(Level level) {
  switch (level) {
    case GOOD: return ledStrip.Color(0, 255, 0);      // Vert
    case MEDIUM:      return ledStrip.Color(255, 255, 0);    // Jaune
    case WARNING:    return ledStrip.Color(255, 140, 0);    // Orange
    case POOR:
    default:        return ledStrip.Color(255, 0, 0);      // Rouge
  }
}

// === Texte selon le niveau ===
const char* levelText(Level level) {
  switch (level) {
    case GOOD: return "Good";
    case MEDIUM:      return "Medium";
    case WARNING:    return "Warning";
    case POOR:
    default:        return "Poor";
  }
}

// === Afficher LEDs pour confort et pollution ===
void displayLEDs(Level confort, Level pollution) {
  uint32_t colorConfort = levelColor(confort);
  uint32_t colorPollution = levelColor(pollution);

  // LEDs 0–5 : confort
  for (int i = 0; i < LED_CONFORT; i++) {
    ledStrip.setPixelColor(i, colorConfort);
  }

  // LEDs 6–11 : pollution
  for (int i = LED_CONFORT; i < NUM_LEDS; i++) {
    ledStrip.setPixelColor(i, colorPollution);
  }

  ledStrip.show();
}

// === Initialisation ===
void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);

  ledStrip.begin();
  ledStrip.setBrightness(70);
  ledStrip.show();

  if (!bmeSensor.begin(0x76) && !bmeSensor.begin(0x77)) {
    Serial.println("Erreur : BME280 introuvable !");
    displayLEDs(POOR, POOR);
    while (1);
  }

  Serial.println("Hello it's BME280");
  Serial.println("Hello it's PMS7003");
}

// === Boucle principale ===
void loop() {
  static unsigned long previousTime = 0;
  static bool newPMSData = false;

  // Lecture PMS7003
  if (readPMS(airData)) newPMSData = true;

  // Lecture BME280
  float temperature = bmeSensor.readTemperature();
  float humidity    = bmeSensor.readHumidity();
  float pressure    = bmeSensor.readPressure() / 100.0F;

  if (millis() - previousTime > 2000) {
    previousTime = millis();

    Serial.print("Temp: "); Serial.print(temperature, 1);
    Serial.print(" °C => Hum: "); Serial.print(humidity, 1);
    Serial.print(" % => Press: "); Serial.print(pressure, 1);

    if (newPMSData) {
      Serial.print(" => PM1: "); Serial.print(airData.pm1);
      Serial.print(" => PM2.5: "); Serial.print(airData.pm2_5);
      Serial.print(" => PM10: "); Serial.print(airData.pm10);
      newPMSData = false;
    } else {
      Serial.print(" || PM: ?? ?? ??");
    }

    Level pollutionLevel = evaluatePollution(airData.pm2_5);
    Level confortLevel = evaluateConfort(temperature, humidity);

    Serial.print(" || Confort: ");
    Serial.print(levelText(confortLevel));
    Serial.print(" || Air Quality: ");
    Serial.println(levelText(pollutionLevel));

    displayLEDs(confortLevel, pollutionLevel);
  }
}
