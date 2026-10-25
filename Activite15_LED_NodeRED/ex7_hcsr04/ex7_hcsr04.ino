// Activite 15 - Exercice 7 : ajout d'un capteur ultrason HC-SR04 (avec debug)
// Carte : NodeMCU ESP8266 (compatible ESP32) | Moniteur serie : 115200 bauds
//
// Cablage :
//   HC-SR04 VCC  -> VIN (5 V)          HC-SR04 GND  -> GND
//   HC-SR04 TRIG -> D5 (GPIO14)
//   HC-SR04 ECHO -> D6 (GPIO12) via pont diviseur 1k (ECHO->point) + 2k (point->GND), point -> D6
//   LED (anode)  -> D2 (GPIO4) via resistance 220 ohm -> GND
//
// Topics MQTT :
//   room/lamp      (abonne)  on/off : commande manuelle de la LED
//   room/auto      (abonne)  on/off : mode automatique (LED allumee si objet proche)
//   room/distance  (publie)  distance en cm
//   room/presence  (publie)  1 = objet proche, 0 = rien
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif
#include <PubSubClient.h>

const char* WIFI_SSID   = "Jamila's S25";
const char* WIFI_PASS   = "wedonotcarelol";
const char* MQTT_SERVER = "172.22.128.19";   // IP du PC (ipconfig)
const int   MQTT_PORT   = 1883;

const char* T_LAMP     = "room/lamp";
const char* T_AUTO     = "room/auto";
const char* T_DISTANCE = "room/distance";
const char* T_PRESENCE = "room/presence";

const int LED_PIN  = 4;    // D2
const int TRIG_PIN = 14;   // D5
const int ECHO_PIN = 12;   // D6

const float SEUIL_ON  = 20.0;  // cm : en dessous -> presence
const float SEUIL_OFF = 25.0;  // cm : au dessus  -> fin de presence (hysteresis)
const unsigned long PERIODE_MESURE = 500;   // ms

WiFiClient espClient;
PubSubClient client(espClient);

bool autoMode = false;
bool presence = false;
unsigned long dernierePulse = 0;
unsigned long nbOk = 0, nbKo = 0;   // statistiques de mesure

void setLed(bool on) { digitalWrite(LED_PIN, on ? HIGH : LOW); }

// ---------------------------------------------------------------- capteur
// Distance en cm, ou -1 si mesure invalide (pas d'echo / impulsion trop courte)
float mesureDistance() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  dernierePulse = pulseIn(ECHO_PIN, HIGH, 30000UL);   // timeout 30 ms (~5 m)
  if (dernierePulse < 120) return -1;                 // < ~2 cm : impossible pour un HC-SR04
  return dernierePulse * 0.0343f / 2.0f;
}

float distanceFiltree() {
  float somme = 0; int n = 0;
  for (int i = 0; i < 3; i++) {
    float d = mesureDistance();
    if (d > 0) { somme += d; n++; }
    delay(15);
  }
  return n ? somme / n : -1;
}

// Autotest au demarrage : niveau de repos de ECHO + 5 declenchements avec impulsions brutes
void autotestCapteur() {
  Serial.println("\n[TEST] === Autotest HC-SR04 ===");
  pinMode(ECHO_PIN, INPUT);
  Serial.print("[TEST] Niveau de repos ECHO (D6) = "); Serial.print(digitalRead(ECHO_PIN));
  Serial.println("  (attendu : 0)");
  if (digitalRead(ECHO_PIN) == 1)
    Serial.println("[TEST] ECHO reste a 1 au repos : court-circuit vers 3V3/5V ou diviseur mal cable");

  int valides = 0;
  for (int i = 1; i <= 5; i++) {
    float d = mesureDistance();
    Serial.print("[TEST] Essai "); Serial.print(i);
    Serial.print(" : impulsion = "); Serial.print(dernierePulse); Serial.print(" us -> ");
    if (d > 0) { Serial.print(d, 1); Serial.println(" cm  OK"); valides++; }
    else if (dernierePulse == 0) Serial.println("aucun signal (timeout) : ECHO non connecte ou capteur non alimente");
    else Serial.println("trop court (bruit) : capteur muet, verifier VCC=5V, TRIG, ECHO, GND");
    delay(100);
  }
  Serial.print("[TEST] Resultat : "); Serial.print(valides); Serial.println("/5 mesures valides");
  Serial.println(valides >= 3 ? "[TEST] Capteur OK" : "[TEST] Capteur NON FONCTIONNEL -> voir cablage");
}

// ---------------------------------------------------------------- reseau
void scan_wifi() {
  Serial.println("[DEBUG] Scan des reseaux visibles par l'ESP :");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    Serial.print("  - '"); Serial.print(WiFi.SSID(i)); Serial.print("' ");
    Serial.print(WiFi.RSSI(i)); Serial.print(" dBm, canal "); Serial.println(WiFi.channel(i));
  }
  if (n == 0) Serial.println("  (aucun reseau trouve)");
}

void setup_wifi() {
  Serial.print("Connexion WiFi a "); Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
    if (++tries % 20 == 0) {
      Serial.print("\n[DEBUG] WiFi status="); Serial.print(WiFi.status());
      Serial.println(" (1=SSID introuvable, 4=echec/mauvais mot de passe, 6=deconnecte)");
      if (WiFi.status() == 1) scan_wifi();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
  }
  Serial.print("\nWiFi OK, IP ESP : "); Serial.println(WiFi.localIP());
  Serial.print("RSSI : "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
  Serial.print("Broker MQTT vise : "); Serial.print(MQTT_SERVER);
  Serial.print(":"); Serial.println(MQTT_PORT);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  Serial.print("[MQTT] "); Serial.print(topic); Serial.print(" = "); Serial.println(msg);

  if (String(topic) == T_LAMP) {
    if (msg.equalsIgnoreCase("on"))       { setLed(true);  Serial.println("[DEBUG] LED -> ON"); }
    else if (msg.equalsIgnoreCase("off")) { setLed(false); Serial.println("[DEBUG] LED -> OFF"); }
    else Serial.println("[DEBUG] message ignore (attendu : on / off)");
  } else if (String(topic) == T_AUTO) {
    autoMode = msg.equalsIgnoreCase("on");
    Serial.print("[DEBUG] Mode automatique : "); Serial.println(autoMode ? "ON" : "OFF");
    if (!autoMode) setLed(false);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connexion MQTT...");
#if defined(ESP8266)
    String id = "esp8266-" + String(ESP.getChipId(), HEX);
#else
    String id = "esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
#endif
    if (client.connect(id.c_str())) {
      Serial.println("OK");
      client.subscribe(T_LAMP);
      client.subscribe(T_AUTO);
      Serial.print("Abonne a "); Serial.print(T_LAMP); Serial.print(" et "); Serial.println(T_AUTO);
    } else {
      Serial.print("echec rc="); Serial.print(client.state());
      Serial.println("  (-2=broker injoignable: IP/pare-feu, -4=timeout, 5=non autorise)");
      Serial.print("[DEBUG] WiFi="); Serial.print(WiFi.status() == WL_CONNECTED ? "OK" : "PERDU");
      Serial.print(" IP="); Serial.println(WiFi.localIP());
      delay(2000);
    }
  }
}

// ---------------------------------------------------------------- programme
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  setLed(false);
  Serial.println("\n=== Exercice 7 : HC-SR04 ===");

  // Test LED : 3 clignotements
  for (int i = 0; i < 3; i++) { setLed(true); delay(200); setLed(false); delay(200); }
  Serial.println("[DEBUG] Test LED termine (pas de clignotement : verifier cablage/polarite/pin D2)");

  autotestCapteur();
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  static unsigned long last = 0;
  if (millis() - last < PERIODE_MESURE) return;
  last = millis();

  float d = distanceFiltree();
  if (d < 0) {
    nbKo++;
    Serial.print("[DEBUG] Mesure invalide (impulsion ECHO = "); Serial.print(dernierePulse);
    Serial.println(" us). Verifier : VCC=5V (VIN), TRIG->D5, ECHO->D6 (via diviseur), GND commun");
  } else {
    nbOk++;
    Serial.print("Distance : "); Serial.print(d, 1); Serial.println(" cm");
    char buf[12];
    dtostrf(d, 1, 1, buf);
    bool ok = client.publish(T_DISTANCE, buf);
    if (!ok) Serial.println("[DEBUG] ECHEC publication MQTT room/distance");

    bool nouvelle = presence ? (d < SEUIL_OFF) : (d < SEUIL_ON);
    if (nouvelle != presence) {
      presence = nouvelle;
      client.publish(T_PRESENCE, presence ? "1" : "0");
      Serial.print("[DEBUG] Presence : "); Serial.println(presence ? "OUI" : "NON");
      if (autoMode) { setLed(presence); Serial.println(presence ? "[DEBUG] Auto : LED ON" : "[DEBUG] Auto : LED OFF"); }
    }
  }

  static unsigned long hb = 0;
  if (millis() - hb > 5000) {   // battement de coeur toutes les 5 s
    hb = millis();
    Serial.print("[DEBUG] vivant | WiFi="); Serial.print(WiFi.status() == WL_CONNECTED);
    Serial.print(" MQTT="); Serial.print(client.connected());
    Serial.print(" auto="); Serial.print(autoMode);
    Serial.print(" mesures OK/KO="); Serial.print(nbOk); Serial.print("/"); Serial.println(nbKo);
  }
}
