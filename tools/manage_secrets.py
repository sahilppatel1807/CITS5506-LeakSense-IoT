#!/usr/bin/env python3
"""
Manage local LeakSense secrets without committing them.

Actions:
  edit   - create or edit the local ignored secrets file
  apply  - write secrets into firmware/dashboard files for local testing
  clean  - replace secrets in firmware/dashboard files with placeholders

The secrets file is stored next to this script and ignored by Git:
  tools/leaksense-local-secrets.json
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SECRETS_PATH = Path(__file__).resolve().with_name("leaksense-local-secrets.json")
FIRMWARE_PATH = REPO_ROOT / "firmware" / "blinking" / "src" / "main.cpp"
FIREBASE_JS_PATH = REPO_ROOT / "dashboard" / "js" / "firebase.js"

FIELDS = [
    ("wifi_ssid", "ESP32 WiFi SSID"),
    ("wifi_password", "ESP32 WiFi password"),
    ("firebase_api_key", "Firebase API key"),
    ("firebase_auth_domain", "Firebase authDomain"),
    ("firebase_database_url", "Firebase databaseURL"),
    ("firebase_project_id", "Firebase projectId"),
    ("firebase_storage_bucket", "Firebase storageBucket"),
    ("firebase_messaging_sender_id", "Firebase messagingSenderId"),
    ("firebase_app_id", "Firebase appId"),
]

PLACEHOLDERS = {
    "wifi_ssid": "YOUR_2_4GHZ_WIFI_SSID",
    "wifi_password": "YOUR_WIFI_PASSWORD",
    "firebase_api_key": "YOUR_FIREBASE_API_KEY",
    "firebase_auth_domain": "YOUR_PROJECT.firebaseapp.com",
    "firebase_database_url": "https://YOUR_PROJECT-default-rtdb.firebaseio.com",
    "firebase_project_id": "YOUR_PROJECT",
    "firebase_storage_bucket": "YOUR_PROJECT.firebasestorage.app",
    "firebase_messaging_sender_id": "YOUR_MESSAGING_SENDER_ID",
    "firebase_app_id": "YOUR_FIREBASE_APP_ID",
}


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8", newline="\n")


def c_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def js_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace("'", "\\'")


def firmware_rest_url(secrets: dict[str, str]) -> str:
    database_url = secrets["firebase_database_url"].rstrip("/")
    return f"{database_url}/leaksense/latest.json"


def load_secrets() -> dict[str, str]:
    if not SECRETS_PATH.exists():
        return {}
    return json.loads(SECRETS_PATH.read_text(encoding="utf-8"))


def save_secrets(secrets: dict[str, str]) -> None:
    SECRETS_PATH.write_text(
        json.dumps(secrets, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


def show_secrets() -> None:
    secrets = load_secrets()
    print(f"Secrets file: {SECRETS_PATH}")
    if not secrets:
        print("No secrets saved yet. Run edit first.")
        return

    for key, label in FIELDS:
        print(f"{label}: {secrets.get(key, '')}")
    if secrets.get("firebase_database_url"):
        print(f"Firmware REST URL: {firmware_rest_url(secrets)}")


def edit_secrets() -> None:
    secrets = load_secrets()
    print(f"Editing secrets file: {SECRETS_PATH}")
    print("Press Enter to keep the current value.")

    for key, label in FIELDS:
        current = secrets.get(key, "")
        suffix = f" [{current}]" if current else ""
        value = input(f"{label}{suffix}: ").strip()
        if value:
            secrets[key] = value

    save_secrets(secrets)
    print("Saved secrets outside the Git repository.")


def require_complete_secrets(secrets: dict[str, str]) -> None:
    missing = [key for key, _ in FIELDS if not secrets.get(key)]
    if missing:
        formatted = ", ".join(missing)
        raise SystemExit(f"Missing secrets: {formatted}. Run edit first.")


def replace_firmware_values(secrets: dict[str, str]) -> None:
    text = read_text(FIRMWARE_PATH)
    text = re.sub(
        r'constexpr const char\* kWifiSsid = ".*?";',
        f'constexpr const char* kWifiSsid = "{c_string(secrets["wifi_ssid"])}";',
        text,
    )
    text = re.sub(
        r'constexpr const char\* kWifiPassword = ".*?";',
        f'constexpr const char* kWifiPassword = "{c_string(secrets["wifi_password"])}";',
        text,
    )
    text = re.sub(
        r'constexpr const char\* kFirebaseLatestUrl =\s*\n\s*".*?";',
        'constexpr const char* kFirebaseLatestUrl =\n'
        f'    "{c_string(firmware_rest_url(secrets))}";',
        text,
        flags=re.S,
    )
    write_text(FIRMWARE_PATH, text)


def replace_dashboard_values(secrets: dict[str, str]) -> None:
    text = read_text(FIREBASE_JS_PATH)
    replacements = {
        "apiKey": secrets["firebase_api_key"],
        "authDomain": secrets["firebase_auth_domain"],
        "databaseURL": secrets["firebase_database_url"],
        "projectId": secrets["firebase_project_id"],
        "storageBucket": secrets["firebase_storage_bucket"],
        "messagingSenderId": secrets["firebase_messaging_sender_id"],
        "appId": secrets["firebase_app_id"],
    }

    for key, value in replacements.items():
        text = re.sub(
            rf"({key}:\s*)'.*?'",
            rf"\g<1>'{js_string(value)}'",
            text,
        )
    write_text(FIREBASE_JS_PATH, text)


def apply_secrets() -> None:
    secrets = load_secrets()
    require_complete_secrets(secrets)
    replace_firmware_values(secrets)
    replace_dashboard_values(secrets)
    print("Applied secrets to:")
    print(f"  {FIRMWARE_PATH.relative_to(REPO_ROOT)}")
    print(f"  {FIREBASE_JS_PATH.relative_to(REPO_ROOT)}")
    print("Remember to run clean before committing.")


def clean_project() -> None:
    clean_values = dict(PLACEHOLDERS)
    replace_firmware_values(clean_values)
    replace_dashboard_values(clean_values)
    print("Replaced project secrets with placeholders.")


def interactive_menu() -> None:
    actions = {
        "1": ("Edit saved secrets", edit_secrets),
        "2": ("Apply secrets to project files", apply_secrets),
        "3": ("Clean project files back to placeholders", clean_project),
        "q": ("Quit", None),
    }

    while True:
        print("\nLeakSense secret manager")
        show_secrets()
        print("\nActions:")
        for key, (label, _) in actions.items():
            print(f"  {key}. {label}")

        choice = input("Choose an action: ").strip().lower()
        if choice == "q":
            return
        action = actions.get(choice)
        if not action:
            print("Unknown choice.")
            continue
        action[1]()


def main() -> None:
    parser = argparse.ArgumentParser(description="Manage LeakSense local secrets.")
    parser.add_argument(
        "action",
        nargs="?",
        choices=["edit", "apply", "clean"],
        help="Action to run. Omit for interactive menu, which shows saved secrets first.",
    )
    args = parser.parse_args()

    if args.action == "edit":
        edit_secrets()
    elif args.action == "apply":
        apply_secrets()
    elif args.action == "clean":
        clean_project()
    else:
        interactive_menu()


if __name__ == "__main__":
    main()
