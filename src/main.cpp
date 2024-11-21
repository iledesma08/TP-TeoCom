#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h> // Usa la biblioteca I2C para el LCD
#include <ThingSpeak.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <time.h>

#define HPA_SEA_LEVEL 1013.25 // Presión atmosférica al nivel del mar en hPa

// Definiciones de red
#define BAUD_RATE 115200

// Definiciones para mostrar datos en el LCD
#define LCD_LINE_0        0
#define LCD_LINE_1        1
#define DATE_INIT_COL     3 // 16-10(largo de la fecha)=6/2 = 3
#define TIME_INIT_COL     6 // 16-5(largo de la hora)=11/2 = 5.5->6
#define TEMP_HUM_INIT_COL 3 // 16-11(largo de la cadena)=5/2 = 2.5->3
#define PRES_INIT_COL     4 // 16-8(largo de la cadena)=8/2 = 4

// Definiciones de tiempo
#define SAMPLE_TIME_MIN  0.5     // Tiempo de muestreo en minutos
#define DISPLAY_TIME_SEC 10      // Tiempo de actualizacion del display en segundos
#define MIN_TO_MS        60000   // Minutos a segundos
#define SEC_TO_MS        1000    // Segundos a milisegundos
#define SEC_TO_US        1000000 // Segundos a microsegundos
#define DELAY_WATCHDOG   10      // Tiempo de espera para evitar que el Watchdog se active

// Definiciones para reloj
#define UTC_OFFSET -10800 // UTC-3 horas en segundos
#define DST_OFFSET 0      // No hay horario de verano en Argentina

// Configuración de WiFi y NTP
const char* ssid = "";
const char* password = "";
#define INTENTOS_WIFI 10
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
#define INTENTOS_HORA   10
#define DELAY_AFTER_TRY 1000
const long gmtOffset_sec = UTC_OFFSET;
const int daylightOffset_sec = DST_OFFSET;

// Configuración de conexión al Gateway (ThingSpeak)
unsigned long channelID = 2684607;            // ID
const char* WriteAPIKey = "WWLNAKEODAP70AF3"; // Write API Key
bool connectToGateway = true;                 // Con cambiar esto a true se conecta a la gateway

// URL del script de Apps Script para enviar datos a Google Sheets
const char* scriptURL = "https://script.google.com/macros/s/"
                        "AKfycbx4X2QpN6QjZ2zSx5OdtCEPX5S8v6WXFEhs6Pvx7NPWWzzBnq"
                        "SnzMXLhmsGCfgRHCLLpg/exec";
int samples = 24 * 60 / SAMPLE_TIME_MIN; // Número de muestras a publicar en la datasheet
bool sendToGoogleSheets = false;         // Con cambiar esto a true ya se empiezan a mandar datos a la datasheet

// Configuración de LCD I2C (dirección I2C y tamaño)
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(0x27, LCD_COLS, LCD_ROWS); // Cambia la dirección I2C si es necesario

// Pines y configuración del DHT11
#define DHTPIN  13 // Ajusta el pin al que está conectado el DHT11
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Configuración del BMP280
#define I2C_ADDRESS_SDO_GND 0x76
Adafruit_BMP280 bmp;

// Variables de tiempo para control de intervalos
unsigned long previousMillisSense = 0;
unsigned long intervalSense = SAMPLE_TIME_MIN * MIN_TO_MS;
unsigned long previousMillisDisplay = 0;
unsigned long intervalDisplay = DISPLAY_TIME_SEC * SEC_TO_MS;
unsigned int time_error = false;
bool showDateTime = false;
bool wifiConnected = true;
bool dhtConnected = true;
bool bmpConnected = true;

// Variables globales de los sensores
float temperatureDHT, humidityDHT, temperatureBMP, pressureBMP, temperatureProm, altitudeBMP;

// Prototipos de funciones
void checkWifi();
void imprimirCentrado(String texto, int linea);
void obtenerHoraNTP();
void mostrarFechaHora();
void update_data_to_google_sheets();
void mostrarDatosSensores_Serial();
void mostrarDatosSensores_LCD();
void actualizarDatosSensores();

void setup()
{
    Serial.begin(BAUD_RATE);

    // Inicializar LCD I2C
    lcd.init();                    // Inicializa el LCD en modo I2C
    lcd.backlight();               // Activa la retroiluminación del LCD
    lcd.begin(LCD_COLS, LCD_ROWS); // Establece el tamaño del LCD
    imprimirCentrado("Inicializando...", LCD_LINE_0);

    // Inicializar DHT11
    dht.begin();
    float temperatureDHT = dht.readTemperature();
    if (isnan(temperatureDHT))
    {
        imprimirCentrado("DHT11 Error", LCD_LINE_0);
        Serial.println("DHT11 no detectado.");
        dhtConnected = false;
    }
    if (dhtConnected)
    {
        Serial.println("DHT11 inicializado.");
    }

    // Inicializar BMP280
    if (!bmp.begin(I2C_ADDRESS_SDO_GND))
    {
        Serial.println("BMP280 no detectado.");
        imprimirCentrado("BMP280 Error", LCD_LINE_0);
        bmpConnected = false;
    }
    if (bmpConnected)
    {
        Serial.println("BMP280 inicializado.");
    }

    // Conexión WiFi
    WiFi.begin(ssid, password);
    Serial.println("Conectando a WiFi...");
    checkWifi();
    actualizarDatosSensores();
}

void loop()
{
    unsigned long currentMillis = millis();

    // Toma de muestras cada tiempo configurado
    if (currentMillis - previousMillisSense >= intervalSense)
    {
        previousMillisSense = currentMillis;
        actualizarDatosSensores(); // Llama a la función para leer y mostrar los
        // datos
    }

    // Rotar entre mostrar datos de sensores y la fecha/hora cada tiempo
    // configurado
    if (currentMillis - previousMillisDisplay >= intervalDisplay)
    {
        previousMillisDisplay = currentMillis;
        if (wifiConnected && !time_error)
        {
            // Toggle entre mostrar la fecha y hora o los datos de los sensores en el
            // LCD, si es que se conectó a WiFi
            showDateTime = !showDateTime;
        }

        lcd.clear();
        if (showDateTime)
        {
            mostrarFechaHora();
        }
        else
        {
            mostrarDatosSensores_LCD();
        }
    }
    delay(DELAY_WATCHDOG); // Para evitar que el Watchdog se active
}

// Función para verificar la conexión a WiFi
void checkWifi()
{
    int retry = 0;
    while (retry < INTENTOS_WIFI && WiFi.status() != WL_CONNECTED)
    {
        delay(DELAY_AFTER_TRY);
        Serial.print(".");
        int visual = retry + 1;
        imprimirCentrado("WiFi try i=" + String(visual), LCD_LINE_0);
        retry++;
    }

    if (retry == INTENTOS_WIFI)
    {
        Serial.println("Error: No se pudo conectar a WiFi.");
        imprimirCentrado("WiFi Error", LCD_LINE_0);
        wifiConnected = false;
        return;
    }

    Serial.println("Conectado a WiFi!");
    wifiConnected = true;
    // Configuración de tiempo usando NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
    obtenerHoraNTP();
}

// Función para centrar texto en el LCD en una línea específica
void imprimirCentrado(String texto, int linea)
{
    int posicionInicio = ceil((16 - texto.length()) / 2.0); // Calcula la posición de inicio

    lcd.clear();                          // Limpia la pantalla
    lcd.setCursor(posicionInicio, linea); // Coloca el cursor en la posición calculada
    lcd.print(texto);                     // Imprime el texto
}

// Función para obtener la hora NTP con límite de intentos
void obtenerHoraNTP()
{
    struct tm timeinfo;
    int retry = 0;
    Serial.println("Esperando la hora...");
    while (!getLocalTime(&timeinfo) && retry < INTENTOS_HORA)
    {
        delay(DELAY_AFTER_TRY);
        Serial.print(".");
        int visual = retry + 1;
        imprimirCentrado("Hora try i=" + String(visual), LCD_LINE_0);
        retry++;
    }

    if (retry == INTENTOS_HORA)
    {
        Serial.println("Error: No se pudo obtener la hora desde el NTP.");
        time_error = true;
    }
    else
    {
        Serial.println("Hora obtenida correctamente!");
    }
}

// Función para mostrar la fecha y hora en el LCD
void mostrarFechaHora()
{
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        lcd.clear();
        lcd.setCursor(DATE_INIT_COL, LCD_LINE_0);
        lcd.printf("%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
        lcd.setCursor(TIME_INIT_COL, LCD_LINE_1);
        lcd.printf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    }
}

// Función para enviar datos a Google Sheets
void update_data_to_google_sheets()
{
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
    if (httpResponseCode > 0)
    {
        String response = http.getString();
        Serial.println("Respuesta del servidor: " + response + "- Código de respuesta: " + String(httpResponseCode));
    }
    else
    {
        Serial.println("Error en la solicitud HTTP: " + http.errorToString(httpResponseCode));
    }
    http.end();
}

// Función para mostrar datos de sensores en el serial
void mostrarDatosSensores_Serial()
{
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        Serial.printf("Timestamp: %02d/%02d/%04d %02d:%02d:%02d\n",
                      timeinfo.tm_mday,
                      timeinfo.tm_mon + 1,
                      timeinfo.tm_year + 1900,
                      timeinfo.tm_hour,
                      timeinfo.tm_min,
                      timeinfo.tm_sec);
    }
    Serial.print("Temperatura: ");
    Serial.print(temperatureBMP);
    Serial.print("C  Humedad: ");
    Serial.print(humidityDHT);
    Serial.print("%  ");
    Serial.print("Presion: ");
    Serial.print(pressureBMP);
    Serial.println(" hPa");
}

// Función para mostrar datos de sensores en el LCD
void mostrarDatosSensores_LCD()
{
    lcd.clear();
    lcd.setCursor(TEMP_HUM_INIT_COL, LCD_LINE_0);
    lcd.printf("T:%dC H:%d%%", (int)temperatureBMP, (int)humidityDHT);
    lcd.setCursor(PRES_INIT_COL, LCD_LINE_1);
    lcd.printf("P:%d hPa", (int)pressureBMP);
}

// Función para leer y mostrar datos de sensores en el serial y en el LCD
void actualizarDatosSensores()
{
    // Leer temperatura y humedad del DHT11 si está conectado
    if (dhtConnected)
    {
        temperatureDHT = dht.readTemperature();
        humidityDHT = dht.readHumidity();
    }
    else
    {
        temperatureDHT = -1;
        humidityDHT = -1;
    }

    if (bmpConnected)
    {
        // Leer presión y temperatura del BMP280
        temperatureBMP = bmp.readTemperature();
        pressureBMP = bmp.readPressure() / 100.0;      // Convertir de Pa a hPa
        altitudeBMP = bmp.readAltitude(HPA_SEA_LEVEL); // Calcular altitud con respecto al nivel del mar
    }
    else
    {
        temperatureBMP = -1;
        pressureBMP = -1;
        altitudeBMP = -1;
    }

    if (dhtConnected && bmpConnected)
    {
        // Calcular promedio de temperatura
        temperatureProm = (temperatureDHT + temperatureBMP) / 2;
    }
    else
    {
        temperatureProm = -1;
    }

    // Imprimir en Serial
    mostrarDatosSensores_Serial();

    // Mostrar en el LCD
    mostrarDatosSensores_LCD();

    if (connectToGateway)
    {
        // Verificar que la conexión se mantuvo
        if (WiFi.status() == WL_CONNECTED)
        {
            wifiConnected = true;
            WiFiClient client;
            ThingSpeak.begin(client); // Reinicia la conexión a ThingSpeak
        }
        else
        {
            Serial.println("Error: Conexión a WiFi perdida. No se puede comunicar con ThingSpeak.");
            wifiConnected = false;
            return;
        }

        if (wifiConnected)
        {
            // Enviar datos a ThingSpeak
            // ThingSpeak.setField(5, temperatureProm);
            ThingSpeak.setField(5, temperatureBMP);
            ThingSpeak.setField(6, humidityDHT);
            ThingSpeak.setField(7, pressureBMP);
            ThingSpeak.setField(8, altitudeBMP);

            int Gateway_Answer = ThingSpeak.writeFields(channelID, WriteAPIKey);

            if (Gateway_Answer == 200)
            {
                Serial.println("Datos enviados a ThingSpeak correctamente.");
            }
            else
            {
                Serial.println("Error al enviar datos a ThingSpeak. Código de error: " + String(Gateway_Answer));
            }
        }
    }

    if (sendToGoogleSheets && wifiConnected)
    {
        // Enviar datos a Google Sheets
        if (samples > 0)
        {
            update_data_to_google_sheets();
            samples--;
        }
    }
}
