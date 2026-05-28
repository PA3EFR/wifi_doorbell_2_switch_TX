# Handleiding ESP32 Remote Deurbel Systeem (2 Melodies)

## 1. Inleiding

Dit document beschrijft de volledige handleiding voor het bouwen van een remote deurbel systeem met behulp van twee ESP32 Lite microcontrollers. Het systeem maakt het mogelijk om vanaf de voordeur of achterdeur een signaal te verzenden dat op zolder een zoemer activeert, zonder dat er bedrading tussen deze locaties nodig is. De communicatie vindt plaats via het lokale WiFi-netwerk, wat betekent dat er geen externe servers, cloudservices of MQTT-brokers aan te pas komen.

Een belangrijke uitbreiding van dit systeem is de bidirectionele communicatie. Nadat de ontvanger op zolder het "RING"- of "PONG"-signaal heeft ontvangen en de melodie heeft geactiveerd, stuurt de ontvanger automatisch een bevestiging ("QSL") terug naar de zender. Wanneer de zender deze bevestiging ontvangt, gaat een groene LED branden op pin 16. Dit geeft visuele feedback aan de persoon bij de deur dat het signaal succesvol is aangekomen op zolder.

Het systeem beschikt over twee drukknoppen op de zender, waarmee verschillende melodieën kunnen worden afgespeeld op de ontvanger. De eerste drukknop (pin 13) verstuurt een "RING"-signaal voor de standaard deurbel-melodie, terwijl de tweede drukknop (pin 2) een "PONG"-signaal verstuurt voor een alternatieve melodie. Dit is bijzonder handig wanneer er meerdere toegangsdeuren zijn, zoals een voordeur en een achterdeur, zodat u aan de hand van het geluid kunt horen welke deur er is bezocht.

Het systeem is ontworpen met nadruk op betrouwbaarheid en eenvoud. UDP wordt gebruikt als communicatieprotocol vanwege de lage latentie en de snelle "fire-and-forget" karakteristiek die bijzonder geschikt is voor toepassingen zoals een deurbel waarbij een snelle reactie belangrijker is dan gegarandeerde levering. Om de betrouwbaarheid te verhogen worden signalen redundant verzonden, wat eventuele packet loss door tijdelijke WiFi-storingen compenseert.

De handleiding is opgedeeld in logische stappen, beginnend met de benodigde materialen en hardware-aansluitingen, gevolgd door de software-installatie en configuratie. Tot slot wordt een testprocedure beschreven om te verifiëren dat alles correct werkt, samen met tips voor het oplossen van veelvoorkomende problemen.

## 2. Systeem Architectuur

Het systeem bestaat uit twee onafhankelijke ESP32 Lite modules die via het lokale WiFi-netwerk met elkaar communiceren. De zendereenheid bevindt zich bij de ingang en is uitgerust met twee drukknoppen die, wanneer deze worden ingedrukt, een UDP-pakket versturen naar de ontvangereenheid op zolder. De ontvanger luistert continu naar binnenkomende pakketten en activeert een melodie zodra een geldig signaal wordt ontvangen. Na de activering stuurt de ontvanger een bevestiging terug naar de zender, die vervolgens een groene LED inschakelt om aan te geven dat het signaal succesvol is verwerkt.

Een belangrijk aspect van het ontwerp is het gebruik van statische IP-adressen. In plaats van te vertrouwen op DHCP-leasevernieuwingen, krijgen beide ESP32-bordjes een vast IP-adres toegewezen binnen het lokale netwerk. Dit elimineert de vertraging die ontstaat wanneer apparaten hun IP-adres moeten vernieuwen en zorgt ervoor dat beide apparaten altijd precies weten naar welke adressen ze moeten communiceren.

De communicatie verloopt via UDP-poort 4210, een willekeurige poort die buiten de standaard gereserveerde ranges valt en daardoor weinig kans heeft op conflicten met andere netwerkservices. De payloads van de signalen zijn de strings "RING" voor het standaard deurbelsignaal, "PONG" voor het alternatieve signaal, en "QSL" voor de bevestiging, wat de verwerking aan beide zijden minimaliseert en de kans op misinterpretatie verkleint.

### 2.1 IP-Adresindeling

De volgende IP-adressen zijn gereserveerd voor dit systeem en moeten binnen uw router of DHCP-server worden geconfigureerd om conflicten met andere apparaten te voorkomen:

| Apparaat | Functie | IP-adres | MAC-adres |
|----------|---------|----------|-----------|
| Zender | Drukknoppen en ACK-LED | 192.168.2.201 | Wordt automatisch toegewezen |
| Ontvanger | Zoemer aansturing en bevestiging | 192.168.2.202 | Wordt automatisch toegewezen |
| Router | Gateway | 192.168.2.254 | - |

Het gateway-adres moet worden aangepast aan uw specifieke routerconfiguratie. Veel routers gebruiken 192.168.2.1 of 192.168.2.254 als standaard gateway-adres. Raadpleeg de documentatie van uw router of de netwerkinstellingen van een reeds verbonden apparaat om het juiste gateway-adres te bepalen.

### 2.2 Communicatieprotocol

Het communicatieprotocol is bidirectioneel en ondersteunt twee verschillende signalen. In de eerste fase verzendt de zender driemaal snel achter elkaar het pakket "RING" of "PONG" naar de ontvanger wanneer een van de drukknoppen wordt ingedrukt. Elke verzending vindt plaats met een interval van 50 milliseconden, wat voldoende tijd geeft voor netwerkverwerking zonder merkbare vertraging voor de gebruiker.

In de tweede fase, direct na ontvangst en verwerking van het signaal, stuurt de ontvanger tweemaal het pakket "QSL" terug naar de zender. De zender wacht maximaal 2 seconden op deze bevestiging. Bij ontvangst van "QSL" activeert de zender de groene LED op pin 16, die 2 seconden blijft branden om visuele feedback te geven. Als er geen bevestiging wordt ontvangen binnen de timeout-periode, wordt een waarschuwing weergegeven in de seriële monitor, maar de melodie op zolder is hoogstwaarschijnlijk al wel afgegaan.

De redundantie in beide richtingen is noodzakelijk omdat UDP een connectionless protocol is dat geen bevestiging van levering geeft. Hoewel WiFi-netwerken over het algemeen betrouwbaar zijn, kunnen tijdelijke storingen, interferentie of netwerkcongestie ervoor zorgen dat individuele pakketten verloren gaan. Door meerdere pakketten te versturen in beide richtingen is de kans dat alle pakketten verloren gaan statistisch verwaarloosbaar.

### 2.3 Melodieën

De ontvanger beschikt over twee verschillende melodieën die afhankelijk van het ontvangen signaal worden afgespeeld. De standaard "RING"-melodie bestaat uit de noten C, E, G en High C, geïnspireerd op het klassieke "ding-dong" geluid van traditionele deurbellen. Deze melodie duurt ongeveer 1,2 seconden en wordt eenmaal afgespeeld bij ontvangst van een "RING"-signaal.

De "PONG"-melodie heeft een moderner karakter en bestaat uit de noten F4, A4, C4, F5, C4, A4 en F4. Deze melodie wordt tweemaal herhaald binnen 2 seconden, wat zorgt voor een herkenbaar en onderscheidend geluid. Deze melodie is bijzonder geschikt voor een achterdeur of een secundaire toegang, zodat u aan de hand van het geluid kunt onderscheiden welke deur er is gebruikt.

Aan de ontvangerzijde wordt een non-blocking aansturing van de zoemer gebruikt. Dit betekent dat de microcontroller niet hoeft te wachten tot de melodie klaar is met afgaan, maar direct kan doorgaan met luisteren naar nieuwe signalen en het verzenden van bevestigingen. Dit is essentieel omdat de ESP32 anders bezet zou zijn met wachten en mogelijk inkomende pakketten zou missen.

### 2.4 Protocolsequenti diagram

De communicatie tussen zender en ontvanger verloopt volgens een vast patroon, ongeacht of het RING- of PONG-signaal wordt verzonden:

1. **Gebruiker drukt op een drukknop** → Zender detecteert drukknop-indrukking (na debouncing)
2. **Zender verzendt "RING" of "PONG"** → Drie UDP-pakketten worden verzonden naar 192.168.2.202:4210
3. **Ontvanger ontvangt signaal** → Respectievelijke melodie wordt geactiveerd
4. **Ontvanger stuurt "QSL"** → Twee bevestigingspakketten worden teruggestuurd naar de zender
5. **Zender ontvangt "QSL"** → Groene LED op pin 16 brandt 2 seconden

## 3. Benodigde Materialen

Voor de realisatie van dit remote deurbel systeem zijn de volgende componenten nodig. De totale kosten blijven relatief laag doordat standaard ESP32 Lite bordjes en eenvoudige componenten worden gebruikt die verkrijgbaar zijn bij reguliere elektronicawinkels of online platforms.

### 3.1 Hoofdcomponenten

De ESP32 Lite, ook wel Lolin32 Lite genoemd, is een compact ontwikkelbordje gebaseerd op de ESP32-WROOM-32 chip. Dit bordje beschikt over geïntegreerde WiFi en Bluetooth functionaliteit, wat het uitstekend geschikt maakt voor IoT-toepassingen zoals deze deurbel. Het bordje heeft voldoende GPIO-pinnen voor de aansluiting van de drukknoppen, zoemer en LED's, en het lage stroomverbruik maakt continue werking mogelijk zonder oververhitting.

Voor de voeding wordt een standaard USB-voedingsadapter van 5V/1A aanbevolen, wat gebruikelijk is voor ESP32-ontwikkelborden. Het is belangrijk om de units permanent van stroom te voorzien via een USB-netzwachtel, in plaats van ze op een powerbank of losse batterijen aan te sluiten. Een deurbel moet altijd beschikbaar zijn en een batterij kan leeg raken op het moment dat u dat niet wilt.

| Component | Specificatie | Aantal | Geschatte prijs |
|-----------|--------------|--------|-----------------|
| ESP32 Lite bordje | Lolin32 of compatible | 2 | €8-15 per stuk |
| Actieve zoemer | 5V met geïntegreerde oscillator | 1 | €1-3 |
| Drukknop | Momentary push-button, NO | 2 | €0,50-2 per stuk |
| LED groen | 5mm of 3mm, voor bevestiging | 1 | €0,20-0,50 |
| LED blauw/geel | 5mm of 3mm, voor status | 1 | €0,20-0,50 |
| Weerstand | 220Ω (LED serieweerstand) | 2 | €0,10-0,50 |
| Weerstand | 10kΩ (optioneel) | 2 | €0,10-0,50 |
| USB-netzwachtel | 5V/1A | 2 | €5-10 |
| Montagemateriaal | Schroeven, kabelgoten | - | €5-15 |
| Verbindingsdraden | Dupont jumper wires | 1 set | €2-5 |

### 3.2 Optionele Componenten

Voor een nettere installatie kunnen optionele componenten worden overwogen. Een klein PCB-bordje maakt het mogelijk om de componenten permanent te solderen in plaats van met losse draden te werken. Een behuizing van kunststof of 3D-geprint materiaal beschermt de elektronica tegen stof en vocht, hoewel dit niet strikt noodzakelijk is voor binnengebruik.

Indien de zendereenheid op een locatie wordt geplaatst waar geen stopcontact beschikbaar is, kan worden gekozen voor een batterijgevoede oplossing. Dit vereist echter aanpassingen in de code om het stroomverbruik te minimaliseren en de levensduur van de batterij te maximaliseren. Een dergelijke implementatie valt buiten de scope van deze handleiding.

## 4. Aansluitschema

De hardware-aansluitingen zijn bewust eenvoudig gehouden om de betrouwbaarheid te maximaliseren en de kans op aansluitfouten te minimaliseren. Beide ESP32 bordjes maken gebruik van de interne pull-up weerstand voor de drukknoppen, wat externe componenten bespaart en de bedrading vereenvoudigt.

### 4.1 Zendereenheid (Voordeur)

De zendereenheid is voorzien van twee drukknoppen die elk met één pin zijn verbonden. De eerste drukknop wordt aangesloten op GPIO 13 met de andere aansluiting naar GND. De interne pull-up weerstand van de ESP32 wordt geactiveerd via de pinMode-instelling INPUT_PULLUP, wat betekent dat de pin standaard HIGH is en naar LOW gaat wanneer de knop wordt ingedrukt. De tweede drukknop wordt aangesloten op GPIO 2, eveneens met de andere aansluiting naar GND en met de interne pull-up weerstand geactiveerd.

De status LED op GPIO 22 is aangesloten met een 220Ω serieweerstand naar de anode van de LED, terwijl de cathode naar GND gaat. De serieweerstand is noodzakelijk om de stroom door de LED te begrenzen en zowel de LED als de ESP32 te beschermen. Een blauwe of gele LED wordt aanbevolen voor de statusindicatie.

De groene bevestigings LED wordt aangesloten op GPIO 16, eveneens met een 220Ω serieweerstand. Deze LED brandt gedurende 2 seconden wanneer de zender een "QSL"-bevestiging ontvangt van de ontvanger. Dit geeft de persoon bij de deur visuele feedback dat het signaal succesvol is aangekomen op zolder.

```
Zendereenheid - Aansluitschema
==============================

ESP32 Lite Bordje
┌─────────────────────────────────────────────────────────┐
│                                                         │
│   3.3V ───────────────────────────────────────────────  │
│                                                         │
│   GPIO 13 ───────┬──────────────────────────────────    │
│                  │                                     │
│              ┌───┴───┐                                 │
│              │ Druk- │  (RING - Voordeur)              │
│              │ knop  │                                 │
│              └───┬───┘                                 │
│                  │                                     │
│   GND ───────────┴──────────────────────────────────    │
│                                                         │
│   GPIO 2 ────────┬──────────────────────────────────    │
│                  │                                     │
│              ┌───┴───┐                                 │
│              │ Druk- │  (PONG - Achterdeur)            │
│              │ knop  │                                 │
│              └───┬───┘                                 │
│                  │                                     │
│   GND ───────────┴──────────────────────────────────    │
│                                                         │
│   GPIO 22 ───────┬────[220Ω]────┬──[Status LED+] 3.3V   │
│                  │              │                         │
│   GND ───────────┴──────────────┴──[Status LED-] GND    │
│                                                         │
│   GPIO 16 ───────┬────[220Ω]────┬──[Groene LED+] 3.3V   │
│                  │              │                         │
│   GND ───────────┴──────────────┴──[Groene LED-] GND    │
│                                                         │
│   5V ────────────────────────────────────────────────   │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 4.2 Ontvangereenheid (Zolder)

De actieve zoemer wordt aangesloten op GPIO 16, waarbij een HIGH-signaal de zoemer activeert en een LOW-signaal de zoemer uitschakelt. Actieve zoemers hebben een ingebouwde oscillator en vereisen alleen een gelijkspanningssignaal om geluid te produceren, wat de aansturing vereenvoudigt ten opzichte van passieve zoemers die een PWM-signaal nodig hebben.

De meeste actieve zoemers werken op 5V, wat de standaard USB-voedingsspanning is. Sommige kleinere zoemers werken ook op 3.3V, wat direct vanaf de ESP32 kan worden gevoed. Raadpleeg de specificaties van uw specifieke zoemer om de juiste voedingsspanning te bepalen.

```
Ontvangereenheid - Aansluitschema
=================================

ESP32 Lite Bordje
┌─────────────────────────────────────────────────────────┐
│                                                         │
│   5V ────────┬──────────────────────────────────────    │
│              │                                         │
│          ┌───┴───┐                                     │
│          │ Actief│                                     │
│          │ Zoemer│                                     │
│          └───┬───┘                                     │
│              │                                         │
│   GPIO 16 ───┴──────────────────────────────────────    │
│                                                         │
│   GPIO 22 ───────┬────[220Ω]────┬──[LED+]── 3.3V/5V     │
│                  │              │                         │
│   GND ───────────┴──────────────┴──[LED-]── GND          │
│                                                         │
│   5V ────────────────────────────────────────────────   │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 4.3 Voedingsschema

Beide units worden gevoed via een USB-kabel aangesloten op een 5V/1A netswachtel. De ESP32 Lite bordjes hebben een ingebouwde spanningsregelaar die de 5V USB-spanning omzet naar 3.3V voor de processor en de GPIO-pinnen. Dit betekent dat de zoemer direct vanaf de 5V USB-spanning kan worden gevoed, terwijl de GPIO-pin van de ESP32 alleen het aan/uit-signaal levert.

Het is belangrijk om een netswachtel te gebruiken die voldoende stroom kan leveren. Hoewel de ESP32 zelf weinig stroom verbruikt, kan een actieve zoemer piekstromen trekken die een goedkope of zwakke netswachtel kunnen overbelasten. Een netswachtel van minimaal 1A wordt daarom aanbevolen.

## 5. Software Installatie

De software voor het deurbel systeem bestaat uit twee afzonderlijke Arduino-sketches: één voor de zendereenheid en één voor de ontvangereenheid. Beide sketches bevatten alle benodigde code inclusief configuratie-instellingen die aan het begin van het bestand kunnen worden aangepast.

### 5.1 Voorbereiding

Voordat de software kan worden geüpload, moet de Arduino IDE worden geconfigureerd voor het werken met ESP32-bordjes. Volg de onderstaande stappen om de ontwikkelomgeving in te stellen:

Het toevoegen van ESP32-ondersteuning aan de Arduino IDE begint met het openen van Voorkeuren via het menu Arduino en het toevoegen van de ESP32 board manager URL. Vervolgens wordt via Boordbeheerder het ESP32-platform gedownload en geïnstalleerd. Na de installatie verschijnen de verschillende ESP32-bordjes in de lijst met beschikbare borden.

De exacte URL voor de ESP32 board manager is https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json. Deze URL moet worden toegevoegd aan het veld "Additional Boards Manager URLs" in de Voorkeuren. Na het herstarten van Arduino IDE kan via Boordbeheerder worden gezocht naar "ESP32" en het pakket worden geïnstalleerd.

### 5.2 Bordselectie en Poort

Na de installatie van het ESP32-platform selecteert u het juiste bord via het menu Hulpmiddelen > Bord. Voor de meeste Lolin32 Lite bordjes is "ESP32 Dev Module" de juiste keuze, hoewel sommige specifieke bordjes een eigen optie hebben. Raadpleeg de documentatie van uw bordje als u niet zeker bent van de juiste selectie.

De volgende instellingen worden aanbevolen voor optimale werking:

| Instelling | Waarde | Toelichting |
|------------|--------|-------------|
| Bord | ESP32 Dev Module | Of specifiek bordtype |
| Upload Speed | 115200 | Balans tussen snelheid en betrouwbaarheid |
| CPU Frequency | 240MHz | Maximale snelheid |
| Flash Mode | DOUT | Compatibiliteit met meeste bordjes |
| Flash Size | 4MB | Voldoende voor alle sketches |

De COM-poort selecteert u via Hulpmiddelen > Poort. Na het aansluiten van de ESP32 via USB verschijnt een nieuwe poort in de lijst, meestal aangeduid als COM3, COM4 of hoger onder Windows, of als /dev/ttyUSB0 onder Linux. Als er meerdere poorten worden getoond en u niet zeker weet welke de juiste is, kunt u de ESP32 loskoppelen en opnieuw aansluiten terwijl u let op welke poort verdwijnt en weer verschijnt.

### 5.3 Code Uploaden

Voor het uploaden van de code naar de zendereenheid opent u het bestand sender_esp32_doorbell.h in de Arduino IDE. Kopieer de volledige inhoud naar een nieuw sketch-bestand en sla dit op als sender_esp32_doorbell.ino. Pas vervolgens de WiFi-instellingen aan het begin van het bestand aan door uw netwerknaam in te vullen bij ssid en uw wachtwoord bij password.

De gateway-instelling moet overeenkomen met het IP-adres van uw router. Dit is vaak 192.168.2.1 of 192.168.2.254, maar kan afwijken afhankelijk van uw netwerkconfiguratie. Raadpleeg de routerdocumentatie of de netwerkinstellingen van een aangesloten apparaat als u niet zeker bent van het juiste adres.

Nadat alle instellingen zijn aangepast, upload u de sketch naar de ESP32 via Sketch > Upload. Tijdens het uploaden knippert de blauwe LED op de ESP32 snel. Als de upload is voltooid, opent u de Seriële Monitor via Hulpmiddelen > Seriële Monitor en stelt u de snelheid in op 115200 baud. U zou berichten moeten zien over het verbinden met WiFi en het verkrijgen van het IP-adres.

Herhaal dit proces voor de ontvangereenheid met het bestand receiver_esp32_doorbell.h. Let op dat beide sketches dezelfde WiFi-instellingen moeten gebruiken, maar dat ze elk naar hun eigen ESP32 worden geüpload.

## 6. Configuratie

De configuratie van het systeem bestaat uit het aanpassen van de netwerkinstellingen aan uw specifieke WiFi-netwerk en het eventueel aanpassen van timings en gevoeligheden aan uw voorkeuren.

### 6.1 WiFi-instellingen

In het begin van beide sketch-bestanden vindt u een sectie met configuratie-instellingen die moet worden aangepast aan uw netwerk. De meest kritische instellingen zijn de netwerknaam (ssid) en het wachtwoord (password), zonder welke geen verbinding met het WiFi-netwerk tot stand kan komen.

```cpp
// WiFi-instellingen
const char* ssid = "UW_WIFI_SSID";                    // Vul uw WiFi-netwerknaam in
const char* password = "UW_WIFI_WACHTWOORD";          // Vul uw WiFi-wachtwoord in
```

Let op dat het wachtwoord hoofdlettergevoelig is en exact moet overeenkomen met het wachtwoord dat u gebruikt om andere apparaten met het netwerk te verbinden. Veel netwerken gebruiken WPA2 of WPA3-beveiliging, wat door de ESP32 wordt ondersteund.

### 6.2 Netwerkadressering

De gateway-instelling moet overeenkomen met het IP-adres van uw router. Dit is het adres dat apparaten gebruiken om communicatie buiten het lokale netwerk te routeren. Bij Nederlandse internetproviders is dit vaak 192.168.2.1 of 192.168.2.254, maar kan variëren.

```cpp
// Netwerkinstellingen (statische IP)
IPAddress gateway(192, 168, 2, 254);                  // IP van uw router
IPAddress subnet(255, 255, 255, 0);                   // Subnet mask
IPAddress dns(8, 8, 8, 8);                            // Google DNS (fallback)
```

Het subnet mask 255.255.255.0 is geschikt voor de meeste thuisnetwerken met maximaal 254 adressen in het lokale netwerk. De DNS-instelling van 8.8.8.8 is de publieke DNS-server van Google en wordt gebruikt als fallback wanneer de router geen DNS-diensten levert.

### 6.3 IP-adres reservering

Hoewel de sketches statische IP-adressen configureren, wordt het aanbevolen om deze adressen ook te reserveren in de DHCP-server van uw router. Dit voorkomt conflicten als de DHCP-server per ongeluk hetzelfde adres zou toewijzen aan een ander apparaat.

Om een DHCP-reservering toe te voegen, logt u in op de webinterface van uw router en navigeert u naar de DHCP-instellingen. Zoek naar een optie zoals "DHCP Reservation", "Static DHCP" of "Address Reservation". Voeg de MAC-adressen van beide ESP32-bordjes toe en wijs de gewenste IP-adressen toe. De MAC-adressen worden weergegeven in de seriële monitor bij het opstarten van de ESP32.

### 6.4 Optionele aanpassingen

Indien gewenst kunnen de volgende parameters worden aangepast aan uw voorkeuren:

| Parameter | Standaard waarde | Bereik | Beschrijving |
|-----------|------------------|--------|--------------|
| DEBOUNCE_DELAY | 50 | 10-200 ms | Tijd om ruis te filteren |
| ANTI_SPAM_DELAY | 2000 | 500-5000 ms | Min tijd tussen signalen |
| ACK_LED_DURATION | 2000 | 500-5000 ms | Hoe lang de groene LED brandt |
| ACK_TIMEOUT | 2000 | 1000-5000 ms | Timeout voor ACK ontvangst |
| DOORBELL_INDICATOR_DURATION | 60000 | 30000-300000 ms | Hoe lang de indicator LED knippert |
| udpPort | 4210 | 1024-65535 | Communicatiepoort |

Het verlagen van DEBOUNCE_DELAY maakt de drukknop gevoeliger, maar kan ook leiden tot onbedoelde triggers door elektrische ruis. Het verhogen van ANTI_SPAM_DELAY voorkomt herhaalde signalen als de knop wordt vastgehouden, maar kan hinderlijk zijn als u snel meerdere keren wilt bellen.

## 7. Testprocedure

Na het aansluiten van alle componenten en het uploaden van de juiste code naar beide units, is het belangrijk om systematisch te verifiëren dat alles correct werkt. De onderstaande testprocedure doorloopt alle kritische functies en identificeert eventuele problemen voordat het systeem in gebruik wordt genomen.

### 7.1 Test ontvanger

Begin met het aansluiten van de ontvangereenheid op zolder en open de seriële monitor op 115200 baud. Wacht tot u de volgende berichten ziet verschijnen:

```
========================================
   ESP32 Deurbel Ontvanger - Zolder     
   Met ondersteuning voor RING en PONG  
========================================
Opstarten...
Pinnen geconfigureerd:
  - Buzzer: GPIO 16
  - Status/Indicator LED: GPIO 22
  - Netwerk status LED: GPIO 23

Verbinden met WiFi...
.........
WiFi verbonden!
----------------------------------------
IP adres: 192.168.2.202
MAC adres: xx:xx:xx:xx:xx:xx
Luisteren op poort: 4210
Verwacht signalen van: 192.168.2.201
----------------------------------------

Ondersteunde signalen:
  - RING: Standaard melodie (C, E, G, High C)
  - PONG: Alternatieve melodie (F4, A4, C4, F5, C4, A4, F4, 2x)

Systeem is klaar voor gebruik!
```

Het IP-adres moet 192.168.2.202 zijn. Als de ontvanger een ander adres krijgt of niet kan verbinden, controleer dan de WiFi-instellingen en de gateway-configuratie. De status LED moet continu branden, wat aangeeft dat de WiFi-verbinding actief is en de ontvanger klaar is om signalen te ontvangen.

### 7.2 Test zender

Sluit vervolgens de zendereenheid aan bij de deur en open eveneens de seriële monitor. Na het opstarten zou u vergelijkbare berichten moeten zien, maar met IP-adres 192.168.2.201 en vermelding van beide drukknoppen:

```
========================================
   ESP32 Deurbel Zender - Voordeur       
   Met twee drukknoppen (RING + PONG)  
========================================
Opstarten...
Pinnen geconfigureerd:
  - RING drukknop: GPIO 13
  - PONG drukknop: GPIO 2
  - Status LED: GPIO 22
  - Bevestigings LED (groen): GPIO 16

Verbinden met WiFi...
WiFi verbonden!
----------------------------------------
IP adres: 192.168.2.201
MAC adres: xx:xx:xx:xx:xx:xx
Zend naar: 192.168.2.202:4210
Luisteren op poort 4210 voor bevestigingen
----------------------------------------
Systeem is klaar voor gebruik!
  - Druk op GPIO 13 voor RING (standaard melodie)
  - Druk op GPIO 2  voor PONG (alternatieve melodie)
```

Controleer dat beide units verbonden zijn met hetzelfde WiFi-netwerk en dat ze elkaar kunnen "zien" op het netwerk. U kunt dit verifiëren door vanaf een computer een ping-opdracht uit te voeren naar beide IP-adressen.

### 7.3 Integratietest RING-signaal

Nu beide units operationeel zijn, drukt u op de RING-drukknop (GPIO 13) bij de voordeur. In de seriële monitor van de zender zou u de volgende berichten moeten zien:

```
>>> RING ingedrukt! Signaal wordt verzonden...
  Pakket 1 verzonden
  Pakket 2 verzonden
  Pakket 3 verzonden
>>> Alle RING signalen verzonden naar ontvanger
  Doel: 192.168.2.202:4210
  Wachten op bevestiging (QSL)...

Ontvangen: QSL van 192.168.2.202
>>> BEVESTIGING ONTVANGEN: QSL <<<
Bevestigings LED geactiveerd (groen op pin 16)
Bevestigings LED gedeactiveerd
```

Gelijkertijd zou in de seriële monitor van de ontvanger het volgende moeten verschijnen:

```
Ontvangen: RING van 192.168.2.201
>>> RING SIGNAAL ONTVANGEN! <<<

========================================
         *** DING DONG! ***
========================================

Start RING melodie afspeel:
  - Aantal noten: 4
  - Herhalingen: 1
  - Noot 1: C (Do) (262 Hz) - 300 ms
  - Noot 2: E (Mi) (330 Hz) - 300 ms
  - Noot 3: G (Sol) (392 Hz) - 300 ms
  - Noot 4: High C (Do hoog) (523 Hz) - 300 ms
  - Melodie voltooid!
QSL pakket 1 verzonden
QSL pakket 2 verzonden
Bevestiging verzonden
Deurbel-indicator geactiveerd (60s knipperen)
```

De zoemer op de ontvanger moet de standaard deurbel-melodie (C, E, G, High C) afspelen en de status LED moet kort uitgaan tijdens het afgaan van de zoemer. Op de zender moet de groene LED op pin 16 gedurende 2 seconden branden als teken dat het signaal succesvol is aangekomen.

### 7.4 Integratietest PONG-signaal

Druk vervolgens op de PONG-drukknop (GPIO 2) om de alternatieve melodie te testen. In de seriële monitor van de zender zou u vergelijkbare berichten moeten zien, maar met "PONG" in plaats van "RING":

```
>>> PONG ingedrukt! Signaal wordt verzonden...
  Pakket 1 verzonden
  Pakket 2 verzonden
  Pakket 3 verzonden
>>> Alle PONG signalen verzonden naar ontvanger
  Doel: 192.168.2.202:4210
  Wachten op bevestiging (QSL)...

Ontvangen: QSL van 192.168.2.202
>>> BEVESTIGING ONTVANGEN: QSL <<<
Bevestigings LED geactiveerd (groen op pin 16)
Bevestigings LED gedeactiveerd
```

In de seriële monitor van de ontvanger:

```
Ontvangen: PONG van 192.168.2.201
>>> PONG SIGNAAL ONTVANGEN! <<<

========================================
         *** PONG PONG! ***
========================================

Start PONG melodie afspeel:
  - Aantal noten: 7
  - Herhalingen: 2
  - Noot 1: F (Fa) (349 Hz) - 140 ms
  - Noot 2: A (La) (440 Hz) - 140 ms
  - Noot 3: C (Do) (262 Hz) - 140 ms
  - Noot 4: High F (Fa hoog) (698 Hz) - 140 ms
  - Noot 5: C (Do) (262 Hz) - 140 ms
  - Noot 6: A (La) (440 Hz) - 140 ms
  - Noot 7: F (Fa) (349 Hz) - 140 ms
  - Herhaling 2 starten
  - Noot 1: F (Fa) (349 Hz) - 140 ms
  - Noot 2: A (La) (440 Hz) - 140 ms
  - Noot 3: C (Do) (262 Hz) - 140 ms
  - Noot 4: High F (Fa hoog) (698 Hz) - 140 ms
  - Noot 5: C (Do) (262 Hz) - 140 ms
  - Noot 6: A (La) (440 Hz) - 140 ms
  - Noot 7: F (Fa) (349 Hz) - 140 ms
  - Melodie voltooid!
QSL pakket 1 verzonden
QSL pakket 2 verzonden
Bevestiging verzonden
Deurbel-indicator geactiveerd (60s knipperen)
```

De PONG-melodie wordt tweemaal herhaald binnen ongeveer 2 seconden, wat een herkenbaar en onderscheidend geluid oplevert. Dit stelt u in staat om aan de hand van het geluid te bepalen welke deur er is gebruikt.

### 7.5 Bevestigings LED test

Een belangrijk onderdeel van de test is het verifiëren van de bevestigings LED. Deze groene LED op pin 16 van de zender moet de volgende gedrag vertonen:

De LED brandt niet wanneer het systeem in rust is. Zodra een drukknop wordt ingedrukt, knippert de status LED op pin 22 kort om aan te geven dat het signaal wordt verzonden. Wanneer vervolgens de "QSL"-bevestiging wordt ontvangen, gaat de groene LED op pin 16 branden en blijft 2 seconden branden voordat hij automatisch uitgaat.

Als de groene LED niet brandt terwijl de melodie op zolder wel afgaat, controleer dan de aansluiting van de LED op GPIO 16 en de serieweerstand. Het kan ook zijn dat de bevestiging niet wordt ontvangen, wat zichtbaar zou moeten maken in de seriële monitor met de waarschuwing "WAARSCHUWING: Geen bevestiging (QSL) ontvangen van ontvanger!".

### 7.6 Deurbel-indicator test

Na elke druk op een knop moet de LED op de ontvanger 60 seconden lang knipperen om visueel aan te geven dat er iemand heeft gebeld. Deze indicator helpt，尤其是在 wanneer de zoemer niet hoorbaar is of wanneer u even weg was van de ontvanger.

Tijdens de testperiode kunt u verifiëren dat de LED begint met knipperen direct nadat de melodie is afgelopen en dat het knipperen stopt na 60 seconden. Het knipperpatroon is 500 milliseconden aan en 500 milliseconden uit, wat overeenkomt met een frequentie van 1 Hz.

### 7.7 Bereiktest

Plaats beide units op hun definitieve locaties en voer opnieuw de integratietest uit voor beide drukknoppen. UDP-signalen gaan door muren, maar de WiFi-signaalsterkte kan een beperkende factor zijn. Als het signaal niet aankomt, probeer dan een WiFi-extender of overweeg het verplaatsen van de router naar een centrale locatie.

U kunt de signaalsterkte controleren door aan de zenderzijde de volgende regel toe te voegen aan de loop-functie:

```cpp
Serial.print("RSSI: ");
Serial.println(WiFi.RSSI());
```

Een RSSI-waarde boven -70 dBm wordt over het algemeen als voldoende beschouwd voor betrouwbare communicatie. Waarden beneden -80 dBm kunnen leiden tot onbetrouwbare prestaties, waarbij de bevestiging vaker niet zou kunnen worden ontvangen.

## 8. Probleemoplossing

Ondanks de eenvoud van het ontwerp kunnen zich problemen voordoen tijdens de installatie of het gebruik. De onderstaande sectie beschrijft veelvoorkomende problemen en hun oplossingen.

### 8.1 WiFi-verbindingsproblemen

Het meest voorkomende probleem is het niet tot stand komen van een WiFi-verbinding. Controleer eerst dat de netwerknaam en het wachtwoord correct zijn ingevoerd en dat er geen typefouten in staan. WiFi-wachtwoorden zijn hoofdlettergevoelig, dus "MijnNetwerk" is anders dan "mijnnetwerk".

Controleer ook dat het ESP32-bordje binnen het bereik van de WiFi-router is en dat er voldoende signaalsterkte is. Dikke betonnen muren, metalen structuren en andere elektronische apparaten kunnen het WiFi-signaal verstoren. Als de signaalsterkte zwak is, overweeg dan het gebruik van een WiFi-extender of het verplaatsen van de router.

Sommige routers hebben "AP-isolatie" of "client-isolatie" ingeschakeld, wat voorkomt dat apparaten op hetzelfde netwerk direct met elkaar communiceren. Deze functie moet worden uitgeschakeld in de routerinstellingen om het deurbel systeem te laten werken. Raadpleeg de routerdocumentatie voor instructies over het vinden en uitschakelen van deze instelling.

### 8.2 Geen signaalontvangst

Als de WiFi-verbinding wel tot stand komt maar de ontvanger niet reageert op drukknopindrukkingen, controleer dan de volgende zaken. Zorg ervoor dat beide units met hetzelfde WiFi-netwerk zijn verbonden en niet met verschillende netwerken of gastnetwerken. Gastnetwerken zijn vaak geïsoleerd van het hoofdnetwerk en staan directe communicatie niet toe.

Verifieer dat de UDP-poort niet wordt geblokkeerd door een firewall op de router. Hoewel poort 4210 een niet-standaard poort is, blokkeren sommige routers onbekende inkomende verbindingen. Controleer de firewall-instellingen van uw router en zorg ervoor dat UDP-verkeer op poort 4210 is toegestaan.

Controleer ook dat de IP-adressen in de sketches correct zijn geconfigureerd. De zender moet het IP-adres van de ontvanger kennen om het signaal te verzenden. Als deze adressen verkeerd zijn geconfigureerd, wordt het pakket naar het verkeerde apparaat of een niet-bestaand adres verzonden.

### 8.3 Geen bevestiging ontvangen

Als de melodie op zolder wel afgaat maar de groene LED op de zender niet brandt, dan wordt de "QSL"-bevestiging niet ontvangen. Dit kan verschillende oorzaken hebben. Controleer eerst de seriële monitor van de ontvanger om te verifiëren dat de bevestiging daadwerkelijk wordt verzonden. U zou berichten moeten zien zoals "QSL pakket 1 verzonden naar 192.168.2.201".

Als de bevestiging wordt verzonden maar niet wordt ontvangen, controleer dan of er firewallregels zijn die UDP-verkeer in de omgekeerde richting blokkeren. Het kan ook zijn dat de WiFi-verbinding tijdelijk wegvalt of instabiel is, wat vaker voorkomt op grotere afstanden of bij zwakke signaalsterkte.

Verhoog eventueel de ACK_TIMEOUT in de sketch van 2000 naar 3000 of 4000 milliseconden om de zender meer tijd te geven om de bevestiging te ontvangen. Dit kan helpen bij langzamere netwerken of tijdelijke storingen.

### 8.4 Valse triggers

Als de deurbel afgaat zonder dat een knop wordt ingedrukt, is er waarschijnlijk sprake van ruis op een van de drukknopingangen. Hoewel de interne pull-up weerstand normaal gesproken stabiel is, kunnen externe factoren zoals lange kabeltrajecten of nabije elektrische apparaten storing veroorzaken.

Verhoog de DEBOUNCE_DELAY in de sketch van 50 naar 100 of zelfs 200 milliseconden om meer ruis te filteren. U kunt ook een externe pull-down weerstand van 10kΩ toevoegen parallel aan elke drukknop voor extra stabiliteit. Een condensator van 100nF over elke drukknop kan ook helpen om hoge-frequente ruis te filteren.

### 8.5 Zoemer werkt niet

Als de zoemer niet afgaat terwijl de seriële monitor wel signaalontvangst aangeeft, controleer dan de aansluitingen. Actieve zoemers hebben een polariteit en werken alleen wanneer ze correct zijn aangesloten. Verwissel de + en - aansluitingen als de zoemer niet werkt.

Controleer ook dat de zoemer werkt door hem direct op 5V en GND aan te sluiten, buiten de ESP32 om. Als de zoemer dan wel geluid maakt, ligt het probleem in de GPIO-aansturing of de software. Verifieer dat de BUZZER_PIN correct is gedefinieerd in de sketch en overeenkomt met de fysieke aansluiting.

### 8.6 Groene LED werkt niet

Als de groene bevestigings LED op pin 16 niet brandt, controleer dan de aansluiting en de serieweerstand. De LED moet correct zijn aangesloten met de anode naar GPIO 16 via een 220Ω weerstand en de cathode naar GND. Let op de polariteit van de LED: de langere poot is de anode (+).

Controleer ook of de LED defect is door hem direct aan te sluiten op 3.3V en GND. Als de LED dan wel brandt, ligt het probleem in de GPIO-aansturing. Verifieer dat ACK_LED_PIN correct is gedefinieerd als 16 in de sketch.

### 8.7 PONG-melodie speelt niet tweemaal

Als de PONG-melodie slechts eenmaal wordt afgespeeld in plaats van tweemaal, controleer dan de waarde van melodyRepetitions in de startMelody functie. Voor de PONG-melodie moet deze waarde 2 zijn, wat aangeeft dat de melodie twee keer moet worden herhaald.

Controleer ook of de melodie niet wordt onderbroken door een nieuw signaal. Als er tijdens het afspelen van de melodie een nieuw RING- of PONG-signaal wordt ontvangen, wordt het nieuwe signaal wel verwerkt en de bevestiging verzonden, maar de melodie wordt niet onderbroken en opnieuw gestart. Dit is de bedoelde werking om te voorkomen dat de melodie steeds opnieuw begint bij elke druk op de knop.

## 9. Onderhoud en Uitbreidingen

Het deurbel systeem is ontworpen voor betrouwbaarheid met minimaal onderhoud. Door de robuuste software met automatische herverbinding en foutafhandeling zou het systeem maandenlang probleemloos moeten werken zonder tussenkomst.

### 9.1 Periodiek onderhoud

Hoewel het systeem automatisch herstelt van de meeste problemen, is het raadzaam om periodiek de werking te verifiëren. Dit kan door eenvoudigweg op beide deurbelknoppen te drukken en te controleren of de melodieën op zolder hoorbaar zijn en de groene LED op de voordeur brandt. Een maandelijkse test is voldoende om te verifiëren dat alle componenten nog correct functioneren.

Indien de units op een stoffige of vochtige locatie zijn geplaatst, kan periodieke reiniging nodig zijn. Gebruik een droge doek om stof te verwijderen en vermijd het gebruik van vloeibare reinigingsmiddelen in de buurt van de elektronica.

### 9.2 Automatische herstart

Voor langdurig betrouwbaar gebruik kan een periodieke automatische herstart worden geïmplementeerd. Microcontrollers kunnen na langdurig continu gebruik last krijgen van geheugenlekkages of vastgelopen netwerkstack. Een wekelijkse herstart voorkomt dit probleem en zorgt ervoor dat het systeem fris start.

De volgende code kan worden toegevoegd aan beide sketches om een wekelijkse herstart te implementeren:

```cpp
// Variabele voor herstart-timer
unsigned long lastRestartTime = 0;
const unsigned long RESTART_INTERVAL = 604800000; // 7 dagen in milliseconden

// Voeg dit toe aan de loop-functie:
if (millis() - lastRestartTime > RESTART_INTERVAL) {
    Serial.println("Geplande herstart...");
    ESP.restart();
}
```

### 9.3 Optionele uitbreidingen

Voor gebruikers die het systeem verder willen aanpassen, zijn diverse uitbreidingen mogelijk. Een interessante toevoeging zou het implementeren van meer melodieën zijn voor verschillende deuren of kamers. Met de huidige structuur kunnen eenvoudig extra melodieën worden toegevoegd door nieuwe melodie-definities te maken en de ontvangstcode uit te breiden.

Een andere uitbreiding is het toevoegen van batterijbewaking voor de zendereenheid, mocht deze op een locatie worden geplaatst waar geen stopcontact beschikbaar is. Een spanningsdeler aangesloten op een analoge pin kan de batterijspanning monitoren en een waarschuwing versturen wanneer de batterij bijna leeg is.

Voor integratie met smart home systemen kan de ontvanger worden uitgebreid met HTTP-requests naar een home automation server, mits deze in hetzelfde netwerk opereert. Dit stelt u in staat om bijvoorbeeld slimme lampen te laten knipperen wanneer er wordt gebeld, of om een melding te versturen naar uw telefoon. Een dergelijke implementatie valt echter buiten de scope van deze handleiding en vereist aanvullende kennis van het specifieke home automation systeem.

## 10. Technische Specificaties

Deze sectie geeft een overzicht van de volledige technische specificaties van het systeem voor referentie en toekomstig onderhoud.

### 10.1 Netwerkspecificaties

| Parameter | Waarde |
|-----------|--------|
| Protocol | UDP |
| Poort | 4210 |
| RING Payload | "RING" (3x verzonden) |
| PONG Payload | "PONG" (3x verzonden) |
| QSL Payload | "QSL" (2x verzonden) |
| RING redundantie | 3 pakketten per druk |
| PONG redundantie | 3 pakketten per druk |
| QSL redundantie | 2 pakketten per bevestiging |
| Inter-pakket interval | 50 ms (signalen), 20 ms (ACK) |

### 10.2 Pin-configuratie

| Unit | Pin | Functie | Modus |
|------|-----|---------|-------|
| Zender | GPIO 13 | Drukknop RING | INPUT_PULLUP |
| Zender | GPIO 2 | Drukknop PONG | INPUT_PULLUP |
| Zender | GPIO 22 | Status LED | OUTPUT |
| Zender | GPIO 16 | Bevestigings LED (groen) | OUTPUT |
| Ontvanger | GPIO 16 | Zoemer | OUTPUT |
| Ontvanger | GPIO 22 | Status LED | OUTPUT |
| Ontvanger | GPIO 23 | Netwerk status LED | OUTPUT |

### 10.3 Melodie Specificaties

| Melodie | Noten | Frequentie (Hz) | Duur (ms) | Herhalingen | Totale duur |
|---------|-------|-----------------|-----------|-------------|-------------|
| RING | C4 | 262 | 300 | 1 | 1200 ms |
| RING | E4 | 330 | 300 | 1 | - |
| RING | G4 | 392 | 300 | 1 | - |
| RING | C5 | 523 | 300 | 1 | - |
| PONG | F4 | 349 | 140 | 2 | 1960 ms |
| PONG | A4 | 440 | 140 | 2 | - |
| PONG | C4 | 262 | 140 | 2 | - |
| PONG | F5 | 698 | 140 | 2 | - |
| PONG | C4 | 262 | 140 | 2 | - |
| PONG | A4 | 440 | 140 | 2 | - |
| PONG | F4 | 349 | 140 | 2 | - |

### 10.4 Stroomverbruik

| Toestand | Stroomverbruik |
|----------|----------------|
| In rust (WiFi verbonden) | 80-100 mA |
| Tijdens signaalverzending | 120-150 mA |
| Tijdens zoemen | 150-200 mA |
| Met groene LED aan | +20-30 mA |
| In diepe slaag (niet geïmplementeerd) | - |

### 10.5 Timings

| Functie | Waarde |
|---------|--------|
| Debounce vertraging | 50 ms |
| Anti-spam interval | 2000 ms |
| Bevestigings LED duur | 2000 ms |
| ACK timeout | 2000 ms |
| WiFi reconnect interval | 5000 ms |
| Deurbel-indicator duur | 60000 ms |
| Deurbel-indicator interval | 500 ms (1 Hz) |

Dit document is opgesteld om u te begeleiden bij het bouwen, installeren en onderhouden van uw ESP32 remote deurbel systeem met bidirectionele communicatie en twee drukknoppen. Bij vragen of problemen die niet in deze handleiding worden behandeld, raadpleeg dan de Arduino- en ESP32-community forums voor aanvullende ondersteuning.
