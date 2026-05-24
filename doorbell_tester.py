"""
ESP32 Doorbell Laptop Tester - Verbeterde Versie
=================================================

Dit script stelt u in staat om vanaf uw laptop de deurbel te testen.
Nu met betrouwbare ping-tests en betere foutafhandeling.

Gebruik:
    python doorbell_tester.py

Vereisten:
    pip install colorama

Auteur: MiniMax Agent
Datum: Mei 2026
"""

import socket
import time
import sys
import os
import subprocess
from datetime import datetime

# Probeer colorama te importeren voor kleuren (optioneel)
try:
    from colorama import init, Fore, Back, Style
    init(autoreset=True)
    COLORS = True
except ImportError:
    COLORS = False
    class Fore:
        RED = WHITE = GREEN = YELLOW = CYAN = MAGENTA = BLUE = RESET = ""
    class Style:
        BRIGHT = RESET_ALL = ""
    class Back:
        BLACK = RESET = ""

# ============================================
# CONFIGURATIE - AANPASSEN INDEN NODIG
# ============================================

# IP adres van de deurbel ontvanger
RECEIVER_IP = "192.168.170.202"

# Poort van de deurbel ontvanger
RECEIVER_PORT = 4210

# IP adres van de deurbel zender
SENDER_IP = "192.168.170.201"

# Poort waarop we luisteren voor QSL bevestigingen
LISTEN_PORT = 4211

# ============================================
# KLEUR DEFINITIES
# ============================================

if COLORS:
    def print_success(text): print(f"{Fore.GREEN}{text}{Style.RESET_ALL}")
    def print_error(text): print(f"{Fore.RED}{text}{Style.RESET_ALL}")
    def print_warning(text): print(f"{Fore.YELLOW}{text}{Style.RESET_ALL}")
    def print_info(text): print(f"{Fore.CYAN}{text}{Style.RESET_ALL}")
    def print_header(text): print(f"{Fore.BLUE}{Style.BRIGHT}{text}{Style.RESET_ALL}")
    def print_melody(text): print(f"{Fore.MAGENTA}{text}{Style.RESET_ALL}")
else:
    def print_success(text): print(text)
    def print_error(text): print(text)
    def print_warning(text): print(text)
    def print_info(text): print(text)
    def print_header(text): print(text)
    def print_melody(text): print(text)

# ============================================
# HELPER FUNCTIES
# ============================================

def clear_screen():
    """Wis het scherm"""
    os.system('cls' if os.name == 'nt' else 'clear')

def print_banner():
    """Print het banner"""
    clear_screen()
    print_header("=" * 60)
    print_header("   ESP32 Deurbel Laptop Tester")
    print_header("   Monitor & Controle Systeem v2")
    print_header("=" * 60)
    print()
    print_info(f"Ontvanger IP: {RECEIVER_IP}:{RECEIVER_PORT}")
    print_info(f"Zender IP:    {SENDER_IP}")
    print_info(f"Laptop IP:    {get_local_ip()}")
    print()
    print_header("-" * 60)

def get_local_ip():
    """Haal het lokale IP adres van deze laptop op"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "onbekend"

def ping_host(ip, timeout=1):
    """
    Ping een host en return True als deze bereikbaar is.
    Werkt op Windows, Linux en Mac.
    """
    try:
        # Bepaal het ping commando afhankelijk van het OS
        if os.name == 'nt':  # Windows
            cmd = ['ping', '-n', '1', '-w', str(timeout * 1000), ip]
        else:  # Linux/Mac
            cmd = ['ping', '-c', '1', '-W', str(timeout), ip]
        
        result = subprocess.run(
            cmd, 
            stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE,
            timeout=timeout + 1
        )
        
        # Check of ping succesvol was (return code 0)
        return result.returncode == 0
        
    except subprocess.TimeoutExpired:
        return False
    except Exception as e:
        print_warning(f"Ping fout: {e}")
        return False

def send_udp(target_ip, port, message):
    """Verstuur een UDP pakket en return True als succesvol"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(1.0)  # 1 seconde timeout voor send
        sock.sendto(message.encode('utf-8'), (target_ip, port))
        sock.close()
        return True
    except Exception as e:
        print_error(f"Fout bij verzenden: {e}")
        return False

def send_ring():
    """Verstuur RING signaal naar ontvanger"""
    print()
    print_melody(">>> RING signaal wordt verzonden naar ontvanger...")
    print_info(f"    Doel: {RECEIVER_IP}:{RECEIVER_PORT}")
    
    # Verstuur 3 pakketten voor redundantie
    success_count = 0
    for i in range(3):
        if send_udp(RECEIVER_IP, RECEIVER_PORT, "RING"):
            print_success(f"    Pakket {i+1}/3: RING verzonden")
            success_count += 1
        time.sleep(0.05)
    
    print()
    if success_count == 3:
        print_success(f"Alle {success_count} pakketten verzonden!")
    else:
        print_warning(f"{success_count}/3 pakketten verzonden")
    
    return success_count > 0

def send_pong():
    """Verstuur PONG signaal naar ontvanger"""
    print()
    print_melody(">>> PONG signaal wordt verzonden naar ontvanger...")
    print_info(f"    Doel: {RECEIVER_IP}:{RECEIVER_PORT}")
    
    success_count = 0
    for i in range(3):
        if send_udp(RECEIVER_IP, RECEIVER_PORT, "PONG"):
            print_success(f"    Pakket {i+1}/3: PONG verzonden")
            success_count += 1
        time.sleep(0.05)
    
    print()
    if success_count == 3:
        print_success(f"Alle {success_count} pakketten verzonden!")
    else:
        print_warning(f"{success_count}/3 pakketten verzonden")
    
    return success_count > 0

def send_qsl():
    """Verstuur QSL signaal naar zender (simuleer ontvanger)"""
    print()
    print_info(">>> QSL bevestiging wordt verzonden naar zender...")
    print_info(f"    Doel: {SENDER_IP}:{RECEIVER_PORT}")
    
    if send_udp(SENDER_IP, RECEIVER_PORT, "QSL"):
        print_success("    QSL pakket verzonden")
        return True
    else:
        print_error("    Kon QSL niet verzenden")
        return False

def listen_for_qsl(timeout_seconds=3):
    """Luister naar QSL bevestigingen"""
    print()
    print_info(f"Luisteren op poort {LISTEN_PORT} voor QSL...")
    print_info(f"Wachten maximaal {timeout_seconds} seconden...")
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(('0.0.0.0', LISTEN_PORT))
        sock.settimeout(timeout_seconds)
        
        print_info("(Druk Ctrl+C om te stoppen met luisteren)")
        
        try:
            data, addr = sock.recvfrom(1024)
            message = data.decode('utf-8')
            print_success(f">>> QSL ONTVANGEN van {addr[0]}:{addr[1]}")
            print_success(f"    Bericht: {message}")
            sock.close()
            return True
        except socket.timeout:
            print_warning(f"Geen QSL ontvangen binnen {timeout_seconds} seconden")
            sock.close()
            return False
            
    except Exception as e:
        print_error(f"Fout bij luisteren: {e}")
        return False

def network_scan():
    """Scan het netwerk met echte ping tests"""
    print()
    print_header("=== NETWERK SCAN (met ping) ===")
    print_info("Dit kan een paar seconden duren...")
    print()
    
    devices = [
        ("Ontvanger", RECEIVER_IP),
        ("Zender", SENDER_IP)
    ]
    
    results = []
    
    for name, ip in devices:
        print(f"Testen {name} ({ip})...", end=" ", flush=True)
        
        # Gebruik echte ping
        is_reachable = ping_host(ip, timeout=2)
        
        if is_reachable:
            print_success("ACTIEF")
            results.append((name, ip, True))
        else:
            print_error("GEEN ANTWOORD")
            results.append((name, ip, False))
    
    print()
    
    # Samenvatting
    active_count = sum(1 for _, _, active in results if active)
    print_header("=== SCAN RESULTATEN ===")
    print(f"Aantal actieve apparaten: {active_count}/{len(results)}")
    print()
    
    for name, ip, active in results:
        status = "ACTIEF" if active else "OFFLINE"
        if active:
            print_success(f"  {name}: {ip} - {status}")
        else:
            print_error(f"  {name}: {ip} - {status}")
    
    print()
    return results

def quick_test():
    """Snelle test - stuur beide signalen"""
    print()
    print_header("=== SNELLE TEST ===")
    print_info("Test beide melodieën...")
    print()
    
    # Eerst netwerk check
    print_info("Stap 1: Netwerk check...")
    network_scan()
    
    print()
    print_info("Stap 2: RING test...")
    send_ring()
    
    print()
    print_info("Wachten 2 seconden...")
    time.sleep(2)
    
    print()
    print_info("Stap 3: PONG test...")
    send_pong()
    
    print()
    print_header("=== TEST VOLTOOID ===")
    print_info("Check of de melodieën zijn afgespeeld op de ontvanger")
    print_info("en of de groene LED op de zender brandde")
    print()
    print_warning("Als de ontvanger niet reageert:")
    print("  1. Check of de ontvanger aan staat")
    print("  2. Check of WiFi verbinding actief is")
    print("  3. Check de seriële monitor van de ontvanger")
    print()

def show_help():
    """Toon help informatie"""
    print()
    print_header("=== HELP INFORMATIE ===")
    print()
    print("Beschikbare opties:")
    print()
    print("  1. RING sturen      - Stuurt RING naar ontvanger (3x)")
    print("  2. PONG sturen      - Stuurt PONG naar ontvanger (3x)")
    print("  3. QSL sturen       - Simuleert ontvanger antwoord")
    print("  4. Luisteren        - Luister naar QSL op achtergrond")
    print("  5. Snelle test      - Test netwerk + beide melodieën")
    print("  6. Netwerk scan      - Test bereikbaarheid apparaten (ping)")
    print("  7. Continu test      - Interactieve test modus")
    print()
    print("  H. Help             - Toon dit bericht")
    print("  Q. Afsluiten        - Sluit het programma af")
    print()
    print("Tips:")
    print("  - Gebruik optie 5 (Snelle test) voor complete diagnose")
    print("  - Optie 6 (Netwerk scan) gebruikt echte ping")
    print("  - Als apparaat offline is, check fysieke status en voeding")
    print()

def main_menu():
    """Hoofdmenu"""
    running = True
    
    while running:
        print_banner()
        
        print("Kies een optie:")
        print()
        print("  1. RING sturen")
        print("  2. PONG sturen")
        print("  3. QSL sturen (naar zender)")
        print("  4. Luister naar QSL")
        print("  5. Snelle test (netwerk + RING + PONG)")
        print("  6. Netwerk scan (ping test)")
        print()
        print("  H. Help")
        print("  Q. Afsluiten")
        print()
        
        choice = input("Keuze: ").strip().upper()
        
        if choice == "1":
            send_ring()
            input("\nDruk Enter om door te gaan...")
            
        elif choice == "2":
            send_pong()
            input("\nDruk Enter om door te gaan...")
            
        elif choice == "3":
            send_qsl()
            input("\nDruk Enter om door te gaan...")
            
        elif choice == "4":
            try:
                listen_for_qsl()
            except KeyboardInterrupt:
                print_warning("\nLuisteren gestopt")
            input("\nDruk Enter om door te gaan...")
            
        elif choice == "5":
            quick_test()
            input("\nDruk Enter om door te gaan...")
            
        elif choice == "6":
            network_scan()
            input("\nDruk Enter om door te gaan...")
            
        elif choice == "7":
            continuous_test()
            
        elif choice == "H":
            show_help()
            input("\nDruk Enter om door te gaan...")
            
        elif choice == "Q":
            print()
            print_success("Tot ziens!")
            print()
            running = False
            
        else:
            print_warning("Ongeldige keuze. Typ H voor help.")
            time.sleep(1)

def continuous_test():
    """Continu test modus"""
    print()
    print_header("=== CONTINU TEST MODUS ===")
    print_info("Druk Ctrl+C om te stoppen")
    print()
    
    try:
        while True:
            print()
            print("1. Stuur RING (met ping check)")
            print("2. Stuur PONG (met ping check)")
            print("3. Netwerk scan")
            print("4. Stop")
            print()
            
            choice = input("Keuze: ").strip()
            
            if choice == "1":
                print_info("Check ontvanger...")
                if ping_host(RECEIVER_IP):
                    send_ring()
                else:
                    print_error("Ontvanger is offline!")
            elif choice == "2":
                print_info("Check ontvanger...")
                if ping_host(RECEIVER_IP):
                    send_pong()
                else:
                    print_error("Ontvanger is offline!")
            elif choice == "3":
                network_scan()
            elif choice == "4":
                break
            else:
                print_warning("Ongeldige keuze")
                
    except KeyboardInterrupt:
        print()
        print_info("Test modus gestopt")

# ============================================
# STARTPUNT
# ============================================

if __name__ == "__main__":
    print_banner()
    
    print()
    print_success("Welkom bij de ESP32 Deurbel Laptop Tester!")
    print()
    print_info("Dit programma stelt u in staat om:")
    print("  - RING en PONG signalen te sturen naar de ontvanger")
    print("  - QSL bevestigingen te ontvangen")
    print("  - Het netwerk te testen met echte ping")
    print()
    print_info("BELANGRIJK: ESP32 apparaten reageren soms niet op ping.")
    print_info("Gebruik de UDP test functies om te controleren of ze werken.")
    print()
    
    input("Druk Enter om door te gaan...")
    
    main_menu()