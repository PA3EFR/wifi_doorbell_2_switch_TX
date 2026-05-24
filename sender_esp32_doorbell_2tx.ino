/**
 * ESP32 Remote Deurbel - Zender (Voordeur)
 * ============================================
 * 
 * Dit is de zendereenheid die bij de voordeur wordt geplaatst.
 * Het systeem heeft twee drukknoppen:
 *   - Pin 13: Verstuurt "RING" voor standaard deurbel-melodie
 *   - Pin 2:  Verstuurt "PONG" voor alternatieve melodie
 * 
 * Na verzending wacht de zender op een bevestiging "QSL" van de ontvanger.
 * Bij ontvangst van QSL wordt de groene LED op pin 16 geactiveerd.
 * 
 * Functionaliteiten:
 * - Twee drukknoppen met software debouncing
 * - Anti-spam beveiliging (2 seconden wachttijd tussen signalen)
 * - Redundante signaalverzending (3 pakketten voor betrouwbaarheid)
 * - Automatische WiFi herverbinding bij verbindingsverlies
 * - Visuele LED feedback (LED op pin 22 = status, LED op pin 16 = bevestiging)
 * - Ontvangstbevestiging (QSL) van ontvanger
 * 
 * Hardware: ESP32 Lite bordje
 * Pin aansluitingen:
 *   - GPIO 13: Drukknop 1 (RING - voordeur)
 *   - GPIO 2:  Drukknop 2 (PONG - achterdeur)
 *   - GPIO 22: Status LED (blauw/geel)
 *   - GPIO 23: Netwerk status LED (brandt bij WiFi verbinding)
 *   - GPIO 16: Bevestigings LED (groen, active high)
 * 
 * Auteur: MiniMax Agent
 * Datum: Januari 2026
 */

// ============================================
// CONFIGURATIE - AANPASSEN AAN UW NETWERK
// ============================================

// WiFi-instellingen
const char* ssid = "Scouternet_Attick_24";                    // <<< Vul uw WiFi-netwerknaam in
const char* password = "22832115";          // <<< Vul uw WiFi-wachtwoord in

// Netwerkinstellingen (statische IP)
IPAddress gateway(192, 168, 170, 1);                    // IP van uw router
IPAddress subnet(255, 255, 255, 0);                      // Subnet mask
IPAddress dns(8, 8, 8, 8);                               // Google DNS (fallback)

// Statische IP-adressen voor de ESP32's
IPAddress ip_sender(192, 168, 170, 201);                // Zender (voordeur)
IPAddress ip_receiver(192, 168, 170, 202);              // Ontvanger (zolder)

// UDP-instellingen
const int udpPort = 4210;                             // Poort voor communicatie
const char* ringPayload = "RING";                     // Signaal payload voor standaard deurbel
const char* pongPayload = "PONG";                     // Signaal payload voor alternatieve melodie
const char* ackPayload = "QSL";                       // Bevestiging payload

// Pin definities
const int BUTTON_RING_PIN = 13;                       // Drukknop RING op GPIO 13
const int BUTTON_PONG_PIN = 2;                        // Drukknop PONG op GPIO 2
const int SENDER_LED_PIN = 22;                        // Status LED op GPIO 22
const int NETWORK_LED_PIN = 23;                         // Netwerk status LED op GPIO 23
const int ACK_LED_PIN = 16;                           // Bevestigings LED (groen) op GPIO 16

// ============================================
// OVERIGE VARIABELEN (NIET AANPASSEN)
// ============================================

#include <WiFi.h>
#include <WiFiUdp.h>

WiFiUDP udp;
WiFiUDP udpReceive;                                   // Separate UDP instance voor ontvangst

// Pin status variabelen voor RING knop
int lastRingButtonState = HIGH;                       // Vorige RING knopstatus (HIGH = niet ingedrukt)
unsigned long lastRingDebounceTime = 0;               // Tijd van laatste statusverandering RING
bool ringButtonProcessed = false;                     // Flag om herhaalde signalering te voorkomen

// Pin status variabelen voor PONG knop
int lastPongButtonState = HIGH;                       // Vorige PONG knopstatus (HIGH = niet ingedrukt)
unsigned long lastPongDebounceTime = 0;               // Tijd van laatste statusverandering PONG
bool pongButtonProcessed = false;                     // Flag om herhaalde signalering te voorkomen

// Timings
const unsigned long DEBOUNCE_DELAY = 50;              // Debounce tijd in milliseconden
const unsigned long ANTI_SPAM_DELAY = 2000;           // Minimum tijd tussen signalen
unsigned long lastSignalTime = 0;                     // Tijd van laatste verzonden signaal

// ACK LED timing
unsigned long ackLedStartTime = 0;
bool ackLedActive = false;
const unsigned long ACK_LED_DURATION = 2000;          // Hoe lang de groene LED blijft branden

// Wachten op ACK
bool waitingForAck = false;
unsigned long ackWaitStartTime = 0;
const unsigned long ACK_TIMEOUT = 2000;               // Timeout voor ACK ontvangst (ms)

void setup() {
    // Seriële communicatie starten voor debugging
    Serial.begin(115200);
    Serial.println();
    Serial.println("========================================");
    Serial.println("   ESP32 Deurbel Zender - Voordeur     ");
    Serial.println("   Met twee drukknoppen (RING + PONG)  ");
    Serial.println("========================================");
    Serial.println("Opstarten...");
    Serial.println();
    
    // Pinnen configureren
    pinMode(BUTTON_RING_PIN, INPUT_PULLUP);           // Interne pull-up weerstand inschakelen
    pinMode(BUTTON_PONG_PIN, INPUT_PULLUP);           // Interne pull-up weerstand inschakelen
    pinMode(SENDER_LED_PIN, OUTPUT);
    pinMode(NETWORK_LED_PIN, OUTPUT);
    pinMode(ACK_LED_PIN, OUTPUT);
    digitalWrite(SENDER_LED_PIN, LOW);                // LED uit bij opstarten
    digitalWrite(NETWORK_LED_PIN, LOW);               // Netwerk LED uit bij opstarten
    digitalWrite(ACK_LED_PIN, LOW);                   // Bevestigings LED uit bij opstarten
    
    Serial.println("Pinnen geconfigureerd:");
    Serial.print("  - RING drukknop: GPIO ");
    Serial.println(BUTTON_RING_PIN);
    Serial.print("  - PONG drukknop: GPIO ");
    Serial.println(BUTTON_PONG_PIN);
    Serial.print("  - Status LED: GPIO ");
    Serial.println(SENDER_LED_PIN);
    Serial.print("  - Netwerk status LED: GPIO ");
    Serial.println(NETWORK_LED_PIN);
    Serial.print("  - Bevestigings LED (groen): GPIO ");
    Serial.println(ACK_LED_PIN);
    Serial.println();
    
    // Status LED knipperen tijdens WiFi verbinding
    Serial.println("Verbinden met WiFi...");
    for (int i = 0; i < 10; i++) {
        digitalWrite(SENDER_LED_PIN, !digitalRead(SENDER_LED_PIN));
        delay(100);
    }
    
    // Statisch IP configureren
    Serial.println("Statische IP configureren...");
    if (!WiFi.config(ip_sender, gateway, subnet, dns)) {
        Serial.println("FOUT: Kon statische IP niet configureren!");
        while (true);                                // Blokkeer bij fout
    }
    Serial.print("  - IP adres: ");
    Serial.println(ip_sender);
    Serial.print("  - Gateway: ");
    Serial.println(gateway);
    Serial.println();
    
    // Verbinden met WiFi
    Serial.print("Verbinden met WiFi-netwerk: ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);
    
    // Wachten tot verbonden
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        digitalWrite(SENDER_LED_PIN, !digitalRead(SENDER_LED_PIN)); // Knipper tijdens verbinden
    }
    
    // Verbonden - LED aan
    digitalWrite(SENDER_LED_PIN, HIGH);
    digitalWrite(NETWORK_LED_PIN, HIGH);              // Netwerk LED permanent aan
    Serial.println();
    Serial.println();
    Serial.println("WiFi verbonden!");
    Serial.println("----------------------------------------");
    Serial.print("IP adres: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC adres: ");
    Serial.println(WiFi.macAddress());
    Serial.print("Zend naar: ");
    Serial.print(ip_receiver);
    Serial.print(":");
    Serial.println(udpPort);
    Serial.println("----------------------------------------");
    
    // UDP luisteraar starten voor ontvangst van QSL
    udpReceive.begin(udpPort);
    Serial.println("Luisteren op poort " + String(udpPort) + " voor bevestigingen");
    Serial.println();
    Serial.println("Systeem is klaar voor gebruik!");
    Serial.println("  - Druk op GPIO 13 voor RING (standaard melodie)");
    Serial.println("  - Druk op GPIO 2  voor PONG (alternatieve melodie)");
    Serial.println();
}

void loop() {
    // WiFi verbindingsstatus controleren
    if (WiFi.status() != WL_CONNECTED) {
        handleDisconnection();
        return;
    }
    
    // Controleren op inkomende UDP pakketten (QSL bevestigingen)
    checkForAck();
    
    // Update ACK LED timer
    updateAckLed();
    
    // RING drukknop uitlezen
    handleRingButton();
    
    // PONG drukknop uitlezen
    handlePongButton();
    
    // Check ACK timeout
    if (waitingForAck && (millis() - ackWaitStartTime > ACK_TIMEOUT)) {
        Serial.println("WAARSCHUWING: Geen bevestiging (QSL) ontvangen van ontvanger!");
        waitingForAck = false;
    }
}

void handleRingButton() {
    int reading = digitalRead(BUTTON_RING_PIN);
    
    // Debounce logica: als de status is veranderd, reset de timer
    if (reading != lastRingButtonState) {
        lastRingDebounceTime = millis();
    }
    
    // Na de debounce-tijd: is de status nog steeds stabiel?
    if ((millis() - lastRingDebounceTime) > DEBOUNCE_DELAY) {
        // Knop is ingedrukt (LOW bij pull-up) en nog niet verwerkt
        if (reading == LOW && !ringButtonProcessed) {
            // Check anti-spam timing
            if (millis() - lastSignalTime > ANTI_SPAM_DELAY) {
                sendDoorbellSignal(ringPayload, "RING");
                lastSignalTime = millis();
            } else {
                Serial.println("Anti-spam: RING signaal geblokkeerd (nog geen 2 seconden)");
            }
            ringButtonProcessed = true;
        }
        
        // Reset de verwerkingsflag wanneer knop wordt losgelaten
        if (reading == HIGH) {
            ringButtonProcessed = false;
        }
    }
    
    // Huidige status opslaan voor volgende iteratie
    lastRingButtonState = reading;
}

void handlePongButton() {
    int reading = digitalRead(BUTTON_PONG_PIN);
    
    // Debounce logica: als de status is veranderd, reset de timer
    if (reading != lastPongButtonState) {
        lastPongDebounceTime = millis();
    }
    
    // Na de debounce-tijd: is de status nog steeds stabiel?
    if ((millis() - lastPongDebounceTime) > DEBOUNCE_DELAY) {
        // Knop is ingedrukt (LOW bij pull-up) en nog niet verwerkt
        if (reading == LOW && !pongButtonProcessed) {
            // Check anti-spam timing
            if (millis() - lastSignalTime > ANTI_SPAM_DELAY) {
                sendDoorbellSignal(pongPayload, "PONG");
                lastSignalTime = millis();
            } else {
                Serial.println("Anti-spam: PONG signaal geblokkeerd (nog geen 2 seconden)");
            }
            pongButtonProcessed = true;
        }
        
        // Reset de verwerkingsflag wanneer knop wordt losgelaten
        if (reading == HIGH) {
            pongButtonProcessed = false;
        }
    }
    
    // Huidige status opslaan voor volgende iteratie
    lastPongButtonState = reading;
}

void sendDoorbellSignal(const char* payload, const char* signalName) {
    Serial.print(">>> ");
    Serial.print(signalName);
    Serial.println(" ingedrukt! Signaal wordt verzonden...");
    waitingForAck = true;
    ackWaitStartTime = millis();
    
    // Verstuur het signaal 3 keer voor redundantie (UDP is niet gegarandeerd)
    for (int i = 0; i < 3; i++) {
        udp.beginPacket(ip_receiver, udpPort);
        udp.print(payload);
        udp.endPacket();
        
        // Visuele feedback: korte LED flits
        digitalWrite(SENDER_LED_PIN, LOW);
        delay(50);
        digitalWrite(SENDER_LED_PIN, HIGH);
        delay(50);
        
        Serial.print("  Pakket ");
        Serial.print(i + 1);
        Serial.println(" verzonden");
    }
    
    Serial.print(">>> Alle ");
    Serial.print(signalName);
    Serial.println(" signalen verzonden naar ontvanger");
    Serial.print("  Doel: ");
    Serial.print(ip_receiver);
    Serial.print(":");
    Serial.println(udpPort);
    Serial.println("  Wachten op bevestiging (QSL)...");
    Serial.println();
}

void checkForAck() {
    char packetBuffer[255];
    int packetSize = udpReceive.parsePacket();
    
    if (packetSize) {
        int len = udpReceive.read(packetBuffer, 255);
        if (len > 0) {
            packetBuffer[len] = 0;
        }
        
        Serial.print("Ontvangen: ");
        Serial.print(packetBuffer);
        Serial.print(" van ");
        Serial.println(udpReceive.remoteIP().toString());
        
        // Check of het een QSL bevestiging is
        if (strcmp(packetBuffer, ackPayload) == 0) {
            Serial.println(">>> BEVESTIGING ONTVANGEN: QSL <<<");
            activateAckLed();
            waitingForAck = false;
        }
    }
}

void activateAckLed() {
    digitalWrite(ACK_LED_PIN, HIGH);
    ackLedStartTime = millis();
    ackLedActive = true;
    Serial.println("Bevestigings LED geactiveerd (groen op pin 16)");
}

void updateAckLed() {
    if (ackLedActive && (millis() - ackLedStartTime > ACK_LED_DURATION)) {
        digitalWrite(ACK_LED_PIN, LOW);
        ackLedActive = false;
        Serial.println("Bevestigings LED gedeactiveerd");
    }
}

void handleDisconnection() {
    static unsigned long lastReconnectAttempt = 0;
    
    // Elke 5 seconden opnieuw proberen
    if (millis() - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = millis();
        Serial.println("WiFi verbinding verloren! Opnieuw verbinden...");
        
        digitalWrite(SENDER_LED_PIN, LOW);            // LED uit bij verbindingsproblemen
        digitalWrite(NETWORK_LED_PIN, LOW);           // Netwerk LED uit
        digitalWrite(ACK_LED_PIN, LOW);               // Bevestigings LED uit
        
        waitingForAck = false;
        ackLedActive = false;
        
        WiFi.disconnect();
        WiFi.reconnect();
    }
}
