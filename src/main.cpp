#include <DHT.h>
#include <Adafruit_BMP280.h>
#include <LiquidCrystal.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <esp_sleep.h>

// Definiciones de red
#define BAUD_RATE 115200

// Definiciones de tiempo
#define SAMPLE_TIME_MIN 5     // 5 minutos
#define DISPLAY_TIME_SEC 10   // Cambio cada 10 segundos
#define MIN_TO_MS 60000       // Minutos a segundos
#define SEC_TO_US 1000000     // Segundos a microsegundos

// Definiciones para reloj
#define UTC_OFFSET -10800 // UTC-3 horas en segundos
#define DST_OFFSET 0      // No hay horario de verano en Argentina

// Configuración de WiFi y NTP
const char* ssid = "Fibertel Wifi249 2.4GHz";
const char* password = "gurdi161pr";
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = UTC_OFFSET;
const int daylightOffset_sec = DST_OFFSET;
// URL del script de Apps Script para enviar datos a Google Sheets
const char* scriptURL = "https://script.google.com/macros/s/AKfycbx4X2QpN6QjZ2zSx5OdtCEPX5S8v6WXFEhs6Pvx7NPWWzzBnqSnzMXLhmsGCfgRHCLLpg/exec";
int samples = 992; // Número de muestras a publicar en la datasheet cada vez que se prende la ESP32

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

// Variables de tiempo para control de intervalos
unsigned long previousMillisSense = 0;
unsigned long intervalSense = SAMPLE_TIME_MIN * MIN_TO_MS;
unsigned long previousMillisDisplay = 0;
unsigned long intervalDisplay = DISPLAY_TIME_SEC;
unsigned int time_error = 0;
bool showDateTime = false;

// Variables globales de los sensores
float temperatureDHT, humidityDHT, temperatureBMP, pressureBMP, temperatureProm;

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

// Función para enviar datos a Google Sheets
void update_data_to_google_sheets() {
  HTTPClient http;

  // Especifica URL del servidor y el script
  http.begin(scriptURL);
  http.addHeader("Content-Type", "application/json");

  // Crea el payload en formato JSON
  String postData = "{\"temperatureDHT\": " + String(temperatureDHT) + ", \"humidityDHT\": " + String(humidityDHT) +
                  ", \"temperatureBMP\": " + String(temperatureBMP) + ", \"pressureBMP\": " + String(pressureBMP) + 
                  ", \"temperatureProm\": " + String(temperatureProm) + "}";
  
  // Realiza la solicitud HTTP POST
  int httpResponseCode = http.POST(postData);

  // Verifica el código de respuesta
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Respuesta del servidor: " + response + "- Código de respuesta: " + String(httpResponseCode));
  } else {
    Serial.println("Error en la solicitud HTTP: " + http.errorToString(httpResponseCode));
  }
  http.end();
}

// Función para mostrar datos de sensores en el serial
void mostrarDatosSensores_Serial() {
  Serial.print("Temperatura: ");
  Serial.print(temperatureBMP);
  Serial.print("°C  Humedad: ");
  Serial.print(humidityDHT);
  Serial.print("%  ");
  Serial.print("Presion: ");
  Serial.print(pressureBMP);
  Serial.println(" hPa");
}

// Función para mostrar datos de sensores en el LCD
void mostrarDatosSensores_LCD() {
  lcd.setCursor(0, 0);
  lcd.printf("T:%dC H:%d%%", (int)temperatureBMP, (int)humidityDHT);
  lcd.setCursor(0, 1);
  lcd.printf("P:%.2fhPa", pressureBMP);
}

// Función para leer y mostrar datos de sensores en el serial y en el LCD
void actualizarDatosSensores() {
  // Leer temperatura y humedad del DHT11
  temperatureDHT = dht.readTemperature();
  humidityDHT = dht.readHumidity();

  // Leer presión y temperatura del BMP280
  temperatureBMP = bmp.readTemperature();
  pressureBMP = bmp.readPressure() / 100.0;  // Convertir de Pa a hPa

  temperatureProm = (temperatureDHT + temperatureBMP) / 2;
  
  // Imprimir en Serial
  mostrarDatosSensores_Serial();

  // Mostrar en el LCD
  mostrarDatosSensores_LCD();

  // Enviar datos a Google Sheets
  if (samples > 0) {
    update_data_to_google_sheets();
    samples--;
  }
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
    actualizarDatosSensores();  // Llama a la función para leer y mostrar los datos
  }
  
  // Configura el tiempo de espera para el modo de bajo consumo (light sleep, sigue conectado a WiFi)
  esp_sleep_enable_timer_wakeup(intervalDisplay * SEC_TO_US);
  // Se despierta del modo de bajo consumo

  // Toggle entre mostrar la fecha y hora o los datos de los sensores en el LCD
  showDateTime = !showDateTime;
    
  lcd.clear();
  if (showDateTime) {
    mostrarFechaHora();
  } else {
    mostrarDatosSensores_LCD();
  }

  delay(10);  // Para evitar que el Watchdog se active
}