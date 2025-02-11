#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include "DHT.h"

// Firebase dodatci
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Wi-Fi podaci
#define WIFI_SSID "c073e6"
#define WIFI_PASSWORD "270138318"

// Firebase API i URL baze
#define API_KEY "AIzaSyC_PlrskkCE7ETSIy6HrdnOxe-8ZKT_h0U"
#define DATABASE_URL "https://project1-e449f-default-rtdb.firebaseio.com/"

// DHT pin i tip senzora
#define DHTPIN D7
#define DHTTYPE DHT11

// LED pin
#define LED_PIN D1

// Buzzer pin
#define BUZZER_PIN D5 // Zamijenite 'D2' odgovarajućim pinom na koji je spojen buzzer

DHT dht(DHTPIN, DHTTYPE);

// Firebase objekti
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT); // Postavljanje LED pina kao izlaznog
  digitalWrite(LED_PIN, LOW); // Isključivanje LED na početku
  pinMode(BUZZER_PIN, OUTPUT); // Postavljanje Buzzer pina kao izlaznog

  // Povezivanje na Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Firebase konfiguracija
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Registracija korisnika na Firebase
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase sign-up successful");
    signupOK = true;
  } else {
    Serial.printf("Firebase sign-up failed: %s\n", config.signer.signupError.message.c_str());
  }

  // Token status callback
  config.token_status_callback = tokenStatusCallback;

  // Inicijalizacija Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Inicijalizacija DHT senzora
  dht.begin();

  // Pokretanje Firebase stream-a za LED kontrolu
  if (!Firebase.RTDB.beginStream(&fbdo, "/ledStatus")) {
    Serial.println("Failed to start Firebase stream: " + fbdo.errorReason());
  }
}

void loop() {
  // Upravljanje LED diodom na osnovu Firebase stream-a
  if (Firebase.RTDB.readStream(&fbdo)) {
    if (fbdo.streamAvailable()) {
      String ledStatus = fbdo.stringData();
      Serial.println("Primljen LED status: " + ledStatus);
      if (ledStatus == "ON") {
        digitalWrite(LED_PIN, HIGH); // Uključi LED
      } else if (ledStatus == "OFF") {
        digitalWrite(LED_PIN, LOW); // Isključi LED
      }
    }
  } else if (fbdo.httpCode() == FIREBASE_ERROR_HTTP_CODE_REQUEST_TIMEOUT) {
    Serial.println("Stream timeout, pokušavam ponovo...");
    Firebase.RTDB.beginStream(&fbdo, "/ledStatus");
  }

  // Slanje podataka sa DHT senzora na Firebase
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Greška pri očitavanju podataka sa DHT senzora!");
      return;
    }

    // Slanje temperature na Firebase
    if (Firebase.RTDB.setFloat(&fbdo, "/senzor/temperatura", temperature)) {
      Serial.println("Temperature sent to Firebase successfully: " + String(temperature) + " °C");
    } else {
      Serial.println("Failed to send temperature: " + fbdo.errorReason());
    }

    // Slanje vlažnosti na Firebase
    if (Firebase.RTDB.setFloat(&fbdo, "/senzor/vlaznost", humidity)) {
      Serial.println("Humidity sent to Firebase successfully: " + String(humidity) + " %");
    } else {
      Serial.println("Failed to send humidity: " + fbdo.errorReason());
    }

    // Aktivacija alarma ako temperatura pređe 25 °C
    if (temperature > 24.0) {
      Serial.println("Temperatura prelazi 25 °C, aktiviram alarm!");

      // Aktivacija buzzera
      for (int i = 0; i < 10; i++) {
        digitalWrite(BUZZER_PIN, HIGH);  // Uključite buzzer
        delay(100);                      // Pričekajte 100 ms
        digitalWrite(BUZZER_PIN, LOW);   // Isključite buzzer
        delay(100);                      // Pričekajte 100 ms
      }
    }
  }
}