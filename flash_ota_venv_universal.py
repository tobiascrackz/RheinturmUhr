#!/usr/bin/env python3
"""
flash_ota_universal.py
Plattform: Windows / macOS / Linux
Hinweis: Fertig für PyInstaller One-File-Exe.
firmware.bin und espota.py werden aus der Exe geladen.
Kein Passwort, kein externes Python nötig.
"""

import sys
import os
import runpy

ESPOTA_LOCAL = "espota.py"
FIRMWARE_FILE = "firmware.bin"

def get_resource_path(filename):
    """Pfad zu eingebetteten Dateien in PyInstaller One-File Exe"""
    if getattr(sys, 'frozen', False):
        return os.path.join(sys._MEIPASS, filename)
    return os.path.abspath(filename)

def ensure_python3():
    if sys.version_info.major < 3:
        print("Fehler: Python 3 wird benötigt.")
        sys.exit(1)
    print(f"Nutze Python {sys.version_info.major}.{sys.version_info.minor} (eingebettet)")

def main():
    ensure_python3()

    firmware_path = get_resource_path(FIRMWARE_FILE)
    if not os.path.exists(firmware_path):
        print(f"Fehler: '{FIRMWARE_FILE}' nicht gefunden im Ordner {os.getcwd()}.")
        sys.exit(1)

    espota_path = get_resource_path(ESPOTA_LOCAL)
    if not os.path.exists(espota_path):
        print(f"Fehler: '{ESPOTA_LOCAL}' nicht gefunden im Ordner {os.getcwd()}.")
        sys.exit(1)

    ip = input("Gib die IP-Adresse des ESP32 ein (z.B. 192.168.1.50): ").strip()
    if not ip:
        print("Keine IP eingegeben. Abbruch.")
        sys.exit(1)

    # Bereite Argumente für espota.py ohne Passwort
    sys.argv = [espota_path, "--ip", ip, "--file", firmware_path]

    print("\nStarte OTA-Upload...")
    try:
        runpy.run_path(espota_path, run_name="__main__")
        print("OTA-Upload abgeschlossen.")
    except Exception as e:
        print("OTA-Upload fehlgeschlagen:", e)
        sys.exit(1)

if __name__ == "__main__":
    main()
