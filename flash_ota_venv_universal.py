#!/usr/bin/env python3
"""
flash_ota_universal.py
Plattform: Windows / macOS / Linux
Voraussetzung: Python 3.x (System), Internetverbindung zum Herunterladen espota.py
Leg die firmware.bin in denselben Ordner wie dieses Skript.
"""

import sys
import os
import platform
import subprocess
import urllib.request
import venv

VENV_DIR = "venv"
ESPOTA_URL = "https://raw.githubusercontent.com/espressif/arduino-esp32/master/tools/espota.py"
ESPOTA_LOCAL = "espota.py"
FIRMWARE_FILE = "firmware.bin"


def ensure_python3():
    if sys.version_info.major < 3:
        print("Fehler: Python 3 wird benötigt. Bitte installiere Python 3 und starte das Skript erneut.")
        sys.exit(1)
    print(f"Nutze Python {sys.version_info.major}.{sys.version_info.minor} ({sys.executable})")


def create_venv_if_missing(venv_path=VENV_DIR):
    if os.path.isdir(venv_path):
        print(f"venv bereits vorhanden: {venv_path}")
    else:
        print(f"Erstelle virtuelle Umgebung in '{venv_path}' ...")
        venv.create(venv_path, with_pip=True)
        print("venv erstellt.")


def get_venv_python(venv_path=VENV_DIR):
    system = platform.system()
    if system == "Windows":
        vpy = os.path.join(venv_path, "Scripts", "python.exe")
    else:
        vpy = os.path.join(venv_path, "bin", "python3")
        if not os.path.exists(vpy):
            vpy = os.path.join(venv_path, "bin", "python")
    if not os.path.exists(vpy):
        # Fallback: use current sys.executable (works but not isolated)
        print("Warnung: venv Python nicht gefunden, verwende das aktuelle Python-Executable.")
        return sys.executable
    return vpy


def upgrade_pip(python_exe):
    print("Aktualisiere pip in der venv...")
    try:
        subprocess.run([python_exe, "-m", "pip", "install", "--upgrade", "pip"], check=True)
    except subprocess.CalledProcessError:
        print("Warnung: pip konnte nicht aktualisiert werden (Fortfahren trotzdem).")


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


def run_espota(venv_python, espota_path, firmware_path, ip, port="3232", auth=None):
    cmd = [venv_python, espota_path, "--ip", ip, "--port", port, "--file", firmware_path]
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

    cwd = os.getcwd()
    firmware_path = os.path.join(cwd, FIRMWARE_FILE)
    if not os.path.exists(firmware_path):
        print(f"Fehler: '{FIRMWARE_FILE}' nicht gefunden im Ordner {cwd}. Lege die Firmware dort ab und starte erneut.")
        sys.exit(1)

    create_venv_if_missing(VENV_DIR)
    venv_python = get_venv_python(VENV_DIR)
    upgrade_pip(venv_python)

    espota_path = os.path.join(cwd, ESPOTA_LOCAL)
    download_espota(espota_path)

    print()
    ip = input("Gib die IP-Adresse des ESP32 ein (z.B. 192.168.1.50): ").strip()
    if not ip:
        print("Keine IP eingegeben. Abbruch.")
        sys.exit(1)
    auth = input("Gib das OTA-Passwort ein (leer lassen, falls keins): ").strip() or None

    run_espota(venv_python, espota_path, firmware_path, ip, auth=auth)


if __name__ == "__main__":
    main()

