#!/usr/bin/env python3
"""
flash_ota_universal.py
Plattform: Windows / macOS / Linux
Voraussetzung: Python 3.x (System)
Hinweis: Für die Exe wird kein venv benötigt.
Lege firmware.bin in denselben Ordner wie das Skript bzw. bind sie via PyInstaller ein.
"""

import sys
import os
import platform
import subprocess
import urllib.request

ESPOTA_URL = "https://raw.githubusercontent.com/espressif/arduino-esp32/master/tools/espota.py"
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
    print(f"Nutze Python {sys.version_info.major}.{sys.version_info.minor} ({sys.executable})")

def download_espota(local_path=ESPOTA_LOCAL):
    if os.path.exists(local_path):
        print(f"{local_path} bereits vorhanden.")
        return local_path
    print(f"Lade espota.py von {ESPOTA_URL} ...")
    try:
        urllib.request.urlretrieve(ESPOTA_URL, local_path)
        print(f"Gespeichert als {local_path}")
    except Exception as e:
        print("Fehler beim Herunterladen von espota.py:", e)
        sys.exit(1)
    return local_path

def run_espota(python_exe, espota_path, firmware_path, ip, port="3232", auth=None):
    cmd = [python_exe, espota_path, "--ip", ip, "--port", port, "--file", firmware_path]
    if auth:
        cmd += ["--auth", auth]
    print("Starte OTA-Upload mit Befehl:")
    print(" ".join(cmd))
    try:
        subprocess.run(cmd, check=True)
        print("OTA-Upload erfolgreich.")
    except subprocess.CalledProcessError as e:
        print("OTA-Upload fehlgeschlagen:", e)
        sys.exit(1)

def main():
    ensure_python3()

    firmware_path = get_resource_path(FIRMWARE_FILE)
    if not os.path.exists(firmware_path):
        print(f"Fehler: '{FIRMWARE_FILE}' nicht gefunden im Ordner {os.getcwd()}.")
        sys.exit(1)

    espota_path = get_resource_path(ESPOTA_LOCAL)
    if not os.path.exists(espota_path):
        download_espota(espota_path)

    python_exe = sys.executable  # Exe benutzt eingebettetes Python

    print()
    ip = input("Gib die IP-Adresse des ESP32 ein (z.B. 192.168.1.50): ").strip()
    if not ip:
        print("Keine IP eingegeben. Abbruch.")
        sys.exit(1)
    auth = input("Gib das OTA-Passwort ein (leer lassen, falls keins): ").strip() or None

    run_espota(python_exe, espota_path, firmware_path, ip, auth=auth)

if __name__ == "__main__":
    main()
