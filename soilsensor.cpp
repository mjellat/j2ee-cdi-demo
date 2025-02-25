#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "esp32/ulp.h"
#include "time.h"

// I2C-Bus Definitionen
#define I2C_SDA 21
#define I2C_SCL 22

// ADC-Pin für Soil Moisture Sensor
#define SOIL_MOISTURE_PIN 4

// I2C Adressen
#define SI7021_ADDR  0x40
#define TMP102_ADDR  0x48

// RTC-Speicheradressen
RTC_DATA_ATTR float temp_si7021;
RTC_DATA_ATTR float temp_tmp102;
RTC_DATA_ATTR int soil_moisture;

// Struktur für ESP-NOW Daten
typedef struct struct_message {
    float temp_si7021;
    float temp_tmp102;
    int soil_moisture;
} struct_message;
struct_message myData;

// Ziel-MAC-Adresse des Empfänger-ESP32
uint8_t broadcastAddress[] = {0x24, 0x6F, 0x28, 0xA1, 0xBC, 0xD2};  // MAC des Empfängers anpassen

// Zeitsteuerung
const int WAKE_HOUR_1 = 7;
const int WAKE_HOUR_2 = 17;
const int WAKE_MINUTE = 0;

// Funktion zur Zeit-Synchronisation
void setupTime() {
    configTime(0, 0, "pool.ntp.org");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Zeit konnte nicht abgerufen werden.");
        return;
    }
    Serial.print("Aktuelle Zeit: ");
    Serial.println(&timeinfo, "%H:%M:%S");
}

// ESP-NOW Callback
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Daten gesendet: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Erfolgreich" : "Fehlgeschlagen");
}

// ESP-NOW Setup
void setupESPNow() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() == ESP_OK) {
        Serial.println("ESP-NOW initialisiert.");
    } else {
        Serial.println("ESP-NOW Init fehlgeschlagen.");
        ESP.restart();
    }
    esp_now_register_send_cb(OnDataSent);
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Hinzufügen des Peers fehlgeschlagen.");
        return;
    }
}

// ULP-Programm laden und konfigurieren
void ulp_setup() {
    ulp_set_wakeup_period(0, 3600 * 1000000); // Stündlich aufwachen
    ulp_load_binary(0, ulp_bin_start, (ulp_bin_end - ulp_bin_start) / sizeof(uint32_t));
    ulp_run((&ulp_entry - RTC_SLOW_MEM) / sizeof(uint32_t));
}

// ULP-Daten aus RTC-Speicher auslesen und per ESP-NOW senden
void read_and_send_ULP_Data() {
    myData.temp_si7021 = ulp_temp_si7021 / 100.0;
    myData.temp_tmp102 = ulp_temp_tmp102 / 100.0;
    myData.soil_moisture = ulp_soil_moisture;

    Serial.printf("SI7021 Temperature: %.2f C\n", myData.temp_si7021);
    Serial.printf("TMP102 Temperature: %.2f C\n", myData.temp_tmp102);
    Serial.printf("Soil Moisture: %d\n", myData.soil_moisture);

    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    Serial.println(result == ESP_OK ? "Senden erfolgreich" : "Senden fehlgeschlagen");
}

// Deep Sleep bis zur nächsten geplanten Zeit
void enterDeepSleep() {
    Serial.println("Gehe in Deep Sleep...");
    esp_sleep_enable_ulp_wakeup();
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA, I2C_SCL);  // I2C Pins festlegen
    pinMode(SOIL_MOISTURE_PIN, INPUT);

    setupTime();
    ulp_setup();
    setupESPNow();

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        int current_hour = timeinfo.tm_hour;
        int current_minute = timeinfo.tm_min;

        if ((current_hour == WAKE_HOUR_1 && current_minute == WAKE_MINUTE) ||
            (current_hour == WAKE_HOUR_2 && current_minute == WAKE_MINUTE)) {
            read_and_send_ULP_Data();
        }
    }

    enterDeepSleep();
}

void loop() {
    // Leerer Loop, da alles in setup() erledigt wird
}