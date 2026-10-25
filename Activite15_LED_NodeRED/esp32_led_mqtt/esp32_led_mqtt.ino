// Activite 15 - Controle d'une LED a distance (Node-RED + MQTT)
// Cartes : ESP32 ou NodeMCU ESP8266 (bibliotheque : PubSubClient)
// Moniteur serie : 115200 bauds
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif
#include <PubSubClient.h>

const char* WIFI_SSID   = "Jamila's S25";            // a modifier (reseau 2.4 GHz pour ESP8266)
const char* WIFI_PASS   = "wedonotcarelol";        // a modifier
const char* MQTT_SERVER = "172.22.128.19";    // IP du PC (Mosquitto) - a modifier (ipconfig)
const int   MQTT_PORT   = 1883;
const char* MQTT_TOPIC  = "room/lamp";

const int LED_PIN = 4;   // GPIO4 (= D2 sur NodeMCU) ; variante : LED_BUILTIN

WiFiClient espClient;
PubSubClient client(espClient);

// Liste les reseaux 2.4 GHz visibles par la carte (debug SSID introuvable)
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
    if (++tries % 20 == 0) {   // toutes les 10 s
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

  if (msg.equalsIgnoreCase("on")) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("[DEBUG] LED -> ON");
  } else if (msg.equalsIgnoreCase("off")) {
    digitalWrite(LED_PIN, LOW);
    Serial.println("[DEBUG] LED -> OFF");
  } else {
    Serial.println("[DEBUG] message ignore (attendu : on / off)");
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
      client.subscribe(MQTT_TOPIC);
      Serial.print("Abonne a "); Serial.println(MQTT_TOPIC);
    } else {
      Serial.print("echec rc="); Serial.print(client.state());
      Serial.println("  (-2=broker injoignable: IP/pare-feu, -4=timeout, 5=non autorise)");
      Serial.print("[DEBUG] WiFi="); Serial.print(WiFi.status() == WL_CONNECTED ? "OK" : "PERDU");
      Serial.print(" IP="); Serial.println(WiFi.localIP());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.println("\n=== Demarrage ===");
  // Test LED/cablage : 3 clignotements au demarrage
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH); delay(200);
    digitalWrite(LED_PIN, LOW);  delay(200);
  }
  Serial.println("[DEBUG] Test LED termine (pas de clignotement : verifier cablage/polarite/pin)");
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  static unsigned long last = 0;
  if (millis() - last > 5000) {   // battement de coeur toutes les 5 s
    last = millis();
    Serial.print("[DEBUG] vivant | WiFi="); Serial.print(WiFi.status() == WL_CONNECTED);
    Serial.print(" MQTT="); Serial.println(client.connected());
  }
}
