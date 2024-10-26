#include <DHT.h>
#include <Adafruit_BMP280.h>
#include <LiquidCrystal.h>
#include <WiFi.h>
#include <time.h>

#define BAUD_RATE 115200

// Definiciones de tiempo
#define SAMPLE_TIME_MS 600000 // 10 minutos en milisegundos
#define DISPLAY_TIME_MS 10000 // Cambio cada 10 segundos

// Pines del LCD
const int rs = 15, en = 2, d4 = 4, d5 = 16, d6 = 17, d7 = 5;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
#define LCD_COLS 16
#define LCD_ROWS 2

// Pines y configuración del DHT11
#define DHTPIN 32        // Ajusta el pin al que está conectado el DHT11
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Configuración del BMP280
#define I2C_ADDRESS_SDO_GND 0x76
Adafruit_BMP280 bmp;

// Definiciones para reloj
#define UTC_OFFSET -10800 // UTC-3 horas en segundos
#define DST_OFFSET 0      // No hay horario de verano en Argentina

// Configuración de WiFi y NTP
const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = UTC_OFFSET;
const int daylightOffset_sec = DST_OFFSET;

// Variables de tiempo para control de intervalos
unsigned long previousMillisSense = 0;
unsigned long intervalSense = SAMPLE_TIME_MS;
unsigned long previousMillisDisplay = 0;
unsigned long intervalDisplay = DISPLAY_TIME_MS; // Cambio cada 10 segundos
unsigned int time_error = 0;
bool showDateTime = false;

// Función para obtener la hora NTP con límite de intentos
void obtenerHoraNTP() {
  struct tm timeinfo;
  int retry = 0;
  while (!getLocalTime(&timeinfo) && retry < 10) {
    lcd.print("Error de hora, i=" + String(retry));
    Serial.println("Esperando la hora...");
    delay(1000);
    retry++;
  }

  if (retry == 10) {
    Serial.println("Error: No se pudo obtener la hora desde el NTP.");
    time_error = 1;
  } else {
    Serial.println("Hora obtenida correctamente!");
    Serial.printf("Fecha y hora: %02d/%02d/%04d %02d:%02d:%02d\n",
                  timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    lcd.setCursor(0, 0);
    lcd.printf("Fecha: %02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
    lcd.setCursor(0, 1);
    lcd.printf("Hora: %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  }
}

// Función para mostrar la fecha y hora en el LCD
void mostrarFechaHora() {
  struct tm timeinfo;
  if (time_error) {
    lcd.print("Error al obtener hora");
    return;
  } else if (getLocalTime(&timeinfo)) {
    lcd.setCursor(0, 0);
    lcd.printf("%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
    lcd.setCursor(0, 1);
    lcd.printf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  }
}

// Función para leer y mostrar datos de sensores en el serial y en el LCD
void mostrarDatosSensores() {
  // Leer temperatura y humedad del DHT11
  float temperatureDHT = dht.readTemperature();
  float humidityDHT = dht.readHumidity();

  // Leer presión y temperatura del BMP280
  float temperatureBMP = bmp.readTemperature();
  float pressureBMP = bmp.readPressure() / 100.0;  // Convertir de Pa a hPa
  
  // Imprimir en Serial
  Serial.print("Temperatura: ");
  Serial.print(temperatureDHT);
  Serial.print("°C  Humedad: ");
  Serial.print(humidityDHT);
  Serial.print("%  ");
  Serial.print("Presion: ");
  Serial.print(pressureBMP);
  Serial.println(" hPa");

  // Mostrar en el LCD
  lcd.setCursor(0, 0);
  lcd.printf("T:%dC H:%d%%", (int)temperatureDHT, (int)humidityDHT);
  lcd.setCursor(0, 1);
  lcd.printf("P:%.2fhPa", pressureBMP);
}

void setup() {
  Serial.begin(BAUD_RATE);

  // Inicializar DHT11
  dht.begin();
  Serial.println("DHT11 inicializado.");

  // Inicializar BMP280
  if (!bmp.begin(I2C_ADDRESS_SDO_GND)) {
    Serial.println("Error al inicializar BMP280.");
    while (1); // Detener aquí si no se detecta el sensor
  }
  Serial.println("BMP280 inicializado.");

  // Inicializar LCD
  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.print("Inicializando...");

  // Conexión WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a WiFi!");

  // Configuración de tiempo usando NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  obtenerHoraNTP();
}

void loop() {
  unsigned long currentMillis = millis();

  // Toma de muestras cada 10 minutos
  if (currentMillis - previousMillisSense >= intervalSense) {
    previousMillisSense = currentMillis;
    mostrarDatosSensores();  // Llama a la función para leer y mostrar los datos
  }
  
  // Rotar entre mostrar datos de sensores y la fecha/hora cada 10 segundos
  if (currentMillis - previousMillisDisplay >= intervalDisplay) {
    previousMillisDisplay = currentMillis;
    showDateTime = !showDateTime;
    
    lcd.clear();
    if (showDateTime) {
      mostrarFechaHora();
    } else {
      mostrarDatosSensores();
    }
  }

  delay(10);  // Para evitar que el Watchdog se active
}
