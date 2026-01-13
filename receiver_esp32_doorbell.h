/**
 * ESP32 Remote Deurbel - Ontvanger (Zolder)
 * ============================================
 * 
 * Dit is de ontvangereenheid die op zolder wordt geplaatst.
 * Het systeem luistert naar UDP-pakketten en activeert verschillende
 * melodieën afhankelijk van het ontvangen signaal:
 *   - "RING": Standaard deurbel-melodie (C, E, G, High C)
 *   - "PONG": Alternatieve melodie (F4, A4, C4, F5, C4, A4, F4, 2x)
 * 
 * Na ontvangst van een geldig signaal wordt de melodie geactiveerd
 * en wordt een bevestiging "QSL" teruggestuurd naar de zender.
 * 
 * Functionaliteiten:
 * - UDP-luisteraar voor signaalontvangst
 * - Non-blocking melodie afspeel functie met Arduino tone()
 * - Twee verschillende melodieën (RING en PONG)
 * - Automatische WiFi herverbinding bij verbindingsverlies
 * - Visuele LED feedback
 * - Bevestiging terugsturen naar zender (QSL)
 * - Deurbel-indicator LED knippert 60s na elke activatie
 * 
 * Hardware: ESP32 Lite bordje
 * Pin aansluitingen:
 *   - GPIO 16: Passieve buzzer (melodie)
 *   - GPIO 22: Status LED / Deurbel-indicator
 *   - GPIO 23: Netwerk status LED
 * 
 * Auteur: MiniMax Agent
 * Datum: Januari 2026
 */

// ============================================
// CONFIGURATIE - AANPASSEN AAN UW NETWERK
// ============================================

// WiFi-instellingen
const char* ssid = "Scouternet_Attick_24";              // Uw WiFi-netwerknaam
const char* password = "22832115";                      // Uw WiFi-wachtwoord

// Netwerkinstellingen (statische IP)
IPAddress gateway(192, 168, 2, 254);                    // IP van uw router
IPAddress subnet(255, 255, 255, 0);                     // Subnet mask
IPAddress dns(8, 8, 8, 8);                              // Google DNS (fallback)

// Statische IP-adressen voor de ESP32's
IPAddress ip_sender(192, 168, 2, 201);                  // Zender (voordeur)
IPAddress ip_receiver(192, 168, 2, 202);                // Ontvanger (zolder)

// UDP-instellingen
const int udpPort = 4210;                               // Poort voor communicatie
const char* ringPayload = "RING";                       // Signaal voor standaard melodie
const char* pongPayload = "PONG";                       // Signaal voor alternatieve melodie
const char* ackPayload = "QSL";                         // Bevestiging payload

// ============================================
// PIN EN BUZZER CONFIGURATIE
// ============================================

const int BUZZER_PIN = 16;                              // Buzzer op GPIO 16
const int RECEIVER_LED_PIN = 22;                        // Status LED op GPIO 22
const int NETWORK_LED_PIN = 23;                         // Netwerk status LED op GPIO 23

// ============================================
// MELODIE DEFINITIES
// ============================================
// Frequencies in Hz (A4 = 440 Hz standaard)
const int NOTE_C4 = 262;                                // Midden C (Do)
const int NOTE_E4 = 330;                                // E (Mi)
const int NOTE_G4 = 392;                                // G (Sol)
const int NOTE_C5 = 523;                                // Hoge C (Do octaaf hoger)
const int NOTE_F4 = 349;                                // F (Fa)
const int NOTE_A4 = 440;                                // A (La)
const int NOTE_F5 = 698;                                // Hoge F (Fa octaaf hoger)

// Melodie structuur: {frequentie, duur_in_ms}
struct Note {
    int frequency;
    unsigned long duration;
};

// RING Melodie: C, E, G, High C (1.2 seconden totaal)
const Note RING_MELODY[] = {
    {NOTE_C4, 300},     // C - 0.3 seconden
    {NOTE_E4, 300},     // E - 0.3 seconden
    {NOTE_G4, 300},     // G - 0.3 seconden
    {NOTE_C5, 300}      // Hoge C - 0.3 seconden
};

const int RING_MELODY_LENGTH = sizeof(RING_MELODY) / sizeof(RING_MELODY[0]);

// PONG Melodie: F4, A4, C4, F5, C4, A4, F4 (tweemaal, binnen 2 seconden)
// Elke noot 140ms: 7 noten = 980ms, tweemaal = 1960ms totaal
const Note PONG_MELODY[] = {
    {NOTE_F4, 150},     // F4 - 140ms
    {NOTE_A4, 80},     // A4 - 140ms
    {NOTE_C5, 80},     // C4 - 140ms
    {NOTE_F5, 150},     // F5 - 140ms
    {NOTE_C5, 80},     // C4 - 140ms
    {NOTE_A4, 80},     // A4 - 140ms
    {NOTE_F4, 150},     // F4 - 140ms
    {NOTE_A4, 80},     // A4 - 140ms
    {NOTE_C5, 80},     // C4 - 140ms
    {NOTE_F5, 150},     // F5 - 140ms
    {NOTE_C5, 80},     // C4 - 140ms
    {NOTE_A4, 80},     // A4 - 140ms
    {NOTE_F4, 150},     // F4 - 140ms
    {NOTE_A4, 80},     // A4 - 140ms
    {NOTE_C5, 80},     // C4 - 140ms
    {NOTE_F5, 150},     // F5 - 140ms
    {NOTE_C5, 80},     // C4 - 140ms
    {NOTE_A4, 80},     // A4 - 140ms
};

const int PONG_MELODY_LENGTH = sizeof(PONG_MELODY) / sizeof(PONG_MELODY[0]);

// ============================================
// OVERIGE VARIABELEN (NIET AANPASSEN)
// ============================================

#include <WiFi.h>
#include <WiFiUdp.h>

WiFiUDP udp;
WiFiUDP udpReceive;

// WiFi variabelen
bool wifiWasConnected = false;

// Melodie afspeel variabelen
unsigned long melodyStartTime = 0;
int currentNoteIndex = 0;
int currentMelodyRepetition = 0;
bool isPlayingMelody = false;
const Note* currentMelody = nullptr;
int currentMelodyLength = 0;
int melodyRepetitions = 1;                              // Hoe vaak de melodie moet worden herhaald

// Deurbel-indicator variabelen
bool doorbellIndicatorActive = false;
unsigned long doorbellIndicatorStartTime = 0;
const unsigned long DOORBELL_INDICATOR_DURATION = 60000; // 60 seconden
const unsigned long DOORBELL_LED_INTERVAL = 500;         // 500ms aan, 500ms uit (1 Hz)

void setup() {
    // Seriële communicatie starten
    Serial.begin(115200);
    Serial.println();
    Serial.println("========================================");
    Serial.println("   ESP32 Deurbel Ontvanger - Zolder     ");
    Serial.println("   Met ondersteuning voor RING en PONG  ");
    Serial.println("========================================");
    Serial.println("Opstarten...");
    Serial.println();
    
    // Pinnen configureren
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);                      // Buzzer uit bij opstarten
    
    pinMode(RECEIVER_LED_PIN, OUTPUT);
    pinMode(NETWORK_LED_PIN, OUTPUT);
    digitalWrite(RECEIVER_LED_PIN, LOW);                // LED uit bij opstarten
    digitalWrite(NETWORK_LED_PIN, LOW);                 // Netwerk LED uit bij opstarten
    
    Serial.println("Pinnen geconfigureerd:");
    Serial.print("  - Buzzer: GPIO ");
    Serial.println(BUZZER_PIN);
    Serial.print("  - Status/Indicator LED: GPIO ");
    Serial.println(RECEIVER_LED_PIN);
    Serial.print("  - Netwerk status LED: GPIO ");
    Serial.println(NETWORK_LED_PIN);
    Serial.println();
    
    // Status LED knipperen tijdens WiFi verbinding
    Serial.println("Verbinden met WiFi...");
    for (int i = 0; i < 10; i++) {
        digitalWrite(RECEIVER_LED_PIN, !digitalRead(RECEIVER_LED_PIN));
        delay(100);
    }
    
    // Statisch IP configureren
    Serial.println("Statische IP configureren...");
    if (!WiFi.config(ip_receiver, gateway, subnet, dns)) {
        Serial.println("FOUT: Kon statische IP niet configureren!");
        while (true);                                  // Blokkeer bij fout
    }
    Serial.print("  - IP adres: ");
    Serial.println(ip_receiver);
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
        digitalWrite(RECEIVER_LED_PIN, !digitalRead(RECEIVER_LED_PIN)); // Knipper tijdens verbinden
    }
    
    // Verbonden - LED aan
    digitalWrite(RECEIVER_LED_PIN, HIGH);
    digitalWrite(NETWORK_LED_PIN, HIGH);                // Netwerk LED permanent aan
    wifiWasConnected = true;
    Serial.println();
    Serial.println();
    Serial.println("WiFi verbonden!");
    Serial.println("----------------------------------------");
    Serial.print("IP adres: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC adres: ");
    Serial.println(WiFi.macAddress());
    Serial.print("Luisteren op poort: ");
    Serial.println(udpPort);
    Serial.print("Verwacht signalen van: ");
    Serial.println(ip_sender);
    Serial.println("----------------------------------------");
    Serial.println();
    Serial.println("Ondersteunde signalen:");
    Serial.println("  - RING: Standaard melodie (C, E, G, High C)");
    Serial.println("  - PONG: Alternatieve melodie (F4, A4, C4, F5, C4, A4, F4, 2x)");
    Serial.println();
    Serial.println("Systeem is klaar voor gebruik!");
    Serial.println();
    
    // UDP luisteraar starten
    udpReceive.begin(udpPort);
}

void loop() {
    // WiFi verbindingsstatus controleren
    if (WiFi.status() != WL_CONNECTED) {
        if (wifiWasConnected) {
            Serial.println("Waarschuwing: WiFi verbinding verbroken!");
            wifiWasConnected = false;
        }
        handleDisconnection();
        return;
    } else {
        if (!wifiWasConnected) {
            Serial.println("WiFi weer verbonden!");
            wifiWasConnected = true;
            digitalWrite(NETWORK_LED_PIN, HIGH);        // Netwerk LED weer inschakelen
        }
    }
    
    // Controleren op inkomende UDP pakketten
    checkForUdpPacket();
    
    // Non-blocking melodie update
    updateMelody();
    
    // Non-blocking deurbel-indicator update
    updateDoorbellIndicator();
}

void checkForUdpPacket() {
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
        
        // Check of het een RING signaal is
        if (strcmp(packetBuffer, ringPayload) == 0) {
            Serial.println(">>> RING SIGNAAL ONTVANGEN! <<<");
            startMelody(RING_MELODY, RING_MELODY_LENGTH, 1, "RING");
            sendAck();
            startDoorbellIndicator();
        }
        // Check of het een PONG signaal is
        else if (strcmp(packetBuffer, pongPayload) == 0) {
            Serial.println(">>> PONG SIGNAAL ONTVANGEN! <<<");
            startMelody(PONG_MELODY, PONG_MELODY_LENGTH, 2, "PONG");
            sendAck();
            startDoorbellIndicator();
        }
    }
}

void sendAck() {
    // Stuur QSL bevestiging 2 keer voor redundantie
    for (int i = 0; i < 2; i++) {
        udp.beginPacket(ip_sender, udpPort);
        udp.print(ackPayload);
        udp.endPacket();
        
        Serial.print("QSL pakket ");
        Serial.print(i + 1);
        Serial.println(" verzonden");
        
        delay(20);  // Korte vertraging tussen pakketten
    }
    
    Serial.println("Bevestiging verzonden");
}

void startMelody(const Note* melody, int length, int repetitions, const char* melodyName) {
    // Voorkom dat melodie opnieuw start als hij al speelt
    if (!isPlayingMelody) {
        Serial.println();
        Serial.println("========================================");
        if (strcmp(melodyName, "RING") == 0) {
            Serial.println("         *** DING DONG! ***            ");
        } else {
            Serial.println("         *** PONG PONG! ***            ");
        }
        Serial.println("========================================");
        Serial.println();
        
        Serial.print("Start ");
        Serial.print(melodyName);
        Serial.println(" melodie afspeel:");
        Serial.print("  - Aantal noten: ");
        Serial.println(length);
        Serial.print("  - Herhalingen: ");
        Serial.println(repetitions);
        
        // Eerste noot starten
        currentMelody = melody;
        currentMelodyLength = length;
        currentNoteIndex = 0;
        currentMelodyRepetition = 0;
        melodyRepetitions = repetitions;
        melodyStartTime = millis();
        isPlayingMelody = true;
        
        // Speel eerste noot met tone()
        const Note& firstNote = currentMelody[currentNoteIndex];
        Serial.print("  - Noot 1: ");
        Serial.print(getNoteName(firstNote.frequency));
        Serial.print(" (");
        Serial.print(firstNote.frequency);
        Serial.print(" Hz) - ");
        Serial.print(firstNote.duration);
        Serial.println(" ms");
        
        tone(BUZZER_PIN, firstNote.frequency);
        
        digitalWrite(RECEIVER_LED_PIN, LOW);             // LED uit tijdens melodie
    } else {
        Serial.print("Melodie wordt al afgespeeld, ");
        Serial.print(melodyName);
        Serial.println(" signaal wordt nog steeds verwerkt");
    }
}

void updateMelody() {
    if (!isPlayingMelody) return;
    
    unsigned long elapsedTime = millis() - melodyStartTime;
    const Note& currentNote = currentMelody[currentNoteIndex];
    
    // Controleer of de huidige noot is afgelopen
    if (elapsedTime >= currentNote.duration) {
        // Ga naar de volgende noot
        currentNoteIndex++;
        
        // Reset timer voor de volgende noot
        melodyStartTime = millis();
        
        // Check of we alle noten van de melodie hebben gehad
        if (currentNoteIndex >= currentMelodyLength) {
            // Check of we nog herhalingen hebben
            if (currentMelodyRepetition < melodyRepetitions - 1) {
                // Naar de volgende herhaling
                currentMelodyRepetition++;
                currentNoteIndex = 0;
                
                Serial.print("  - Herhaling ");
                Serial.print(currentMelodyRepetition + 1);
                Serial.println(" starten");
            } else {
                // Melodie is klaar
                stopMelody();
                return;
            }
        }
        
        // Speel de volgende noot af met tone()
        const Note& nextNote = currentMelody[currentNoteIndex];
        Serial.print("  - Noot ");
        Serial.print(currentNoteIndex + 1);
        Serial.print(": ");
        Serial.print(getNoteName(nextNote.frequency));
        Serial.print(" (");
        Serial.print(nextNote.frequency);
        Serial.print(" Hz) - ");
        Serial.print(nextNote.duration);
        Serial.println(" ms");
        
        tone(BUZZER_PIN, nextNote.frequency);
    }
}

void stopMelody() {
    isPlayingMelody = false;
    currentNoteIndex = 0;
    currentMelodyRepetition = 0;
    currentMelody = nullptr;
    
    // Buzzer uitschakelen
    noTone(BUZZER_PIN);
    
    Serial.println("  - Melodie voltooid!");
    Serial.println("========================================");
    Serial.println();
}

void startDoorbellIndicator() {
    // Start deurbel-indicator LED knipperen
    doorbellIndicatorActive = true;
    doorbellIndicatorStartTime = millis();
    digitalWrite(RECEIVER_LED_PIN, HIGH);               // LED aan bij start
    
    Serial.println("Deurbel-indicator geactiveerd (60s knipperen)");
}

void updateDoorbellIndicator() {
    if (!doorbellIndicatorActive) return;
    
    unsigned long elapsedTime = millis() - doorbellIndicatorStartTime;
    
    // Controleer of de 60 seconden zijn verstreken
    if (elapsedTime >= DOORBELL_INDICATOR_DURATION) {
        // Indicator uitschakelen
        doorbellIndicatorActive = false;
        digitalWrite(RECEIVER_LED_PIN, HIGH);           // LED weer aan (ruststand)
        Serial.println("Deurbel-indicator beeindigd");
        return;
    }
    
    // LED knipperen met 1 Hz frequentie (500ms aan, 500ms uit)
    unsigned long cycleTime = elapsedTime % 1000;
    
    if (cycleTime < 500) {
        digitalWrite(RECEIVER_LED_PIN, HIGH);           // LED aan
    } else {
        digitalWrite(RECEIVER_LED_PIN, LOW);            // LED uit
    }
}

String getNoteName(int frequency) {
    // Converteer frequentie naar notennaam
    switch (frequency) {
        case NOTE_C4:  return "C (Do)";
        case NOTE_E4:  return "E (Mi)";
        case NOTE_G4:  return "G (Sol)";
        case NOTE_C5:  return "High C (Do hoog)";
        case NOTE_F4:  return "F (Fa)";
        case NOTE_A4:  return "A (La)";
        case NOTE_F5:  return "High F (Fa hoog)";
        default:       return "Onbekend";
    }
}

void handleDisconnection() {
    static unsigned long lastReconnectAttempt = 0;
    
    // LED knippert langzaam bij verbindingsproblemen
    if (millis() - lastReconnectAttempt > 1000) {
        lastReconnectAttempt = millis();
        digitalWrite(RECEIVER_LED_PIN, !digitalRead(RECEIVER_LED_PIN));
        digitalWrite(NETWORK_LED_PIN, LOW);            // Netwerk LED uit tijdens verbindingsproblemen
    }
    
    // Wacht even en probeer opnieuw
    delay(100);
    
    WiFi.disconnect();
    WiFi.reconnect();
}
