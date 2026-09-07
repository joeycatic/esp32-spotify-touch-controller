"""Command-line entry point for one-time controller provisioning."""

from __future__ import annotations

import argparse
import getpass
import os
import secrets
import sys
import webbrowser

from .oauth import exchange_code, verify_access_token, wait_for_callback
from .pkce import authorization_url, code_challenge, generate_verifier
from .protocol import (
    ProvisioningData,
    ProvisioningError,
    check_port_access,
    provision_serial,
)
from .ports import PortResolutionError, detected_port


def detect_port() -> str:
    try:
        return detected_port()
    except PortResolutionError as error:
        raise ProvisioningError(str(error)) from error


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Authorize Spotify and provision the ESP32 controller over USB."
    )
    parser.add_argument("--port", help="Serial port, for example /dev/ttyACM0")
    parser.add_argument("--client-id", help="Spotify application Client ID")
    parser.add_argument("--ssid", help="Wi-Fi network name")
    parser.add_argument(
        "--no-browser",
        action="store_true",
        help="Print the Spotify authorization URL instead of opening it",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        # Fail before prompting for credentials or starting browser OAuth.
        port = args.port or os.environ.get("PORT") or detect_port()
        check_port_access(port)

        client_id = (args.client_id or input("Spotify Client ID: ")).strip()
        ssid = args.ssid or input("Wi-Fi SSID: ")
        wifi_password = getpass.getpass("Wi-Fi password (empty for open network): ")

        verifier = generate_verifier()
        state = secrets.token_urlsafe(32)
        url = authorization_url(client_id, code_challenge(verifier), state)
        if args.no_browser or not webbrowser.open(url):
            print(f"Open this URL in your browser:\n{url}")
        else:
            print("Spotify authorization opened in your browser.")

        code = wait_for_callback(state)
        tokens = exchange_code(client_id, code, verifier)
        account_name = verify_access_token(str(tokens["access_token"]))
        print(f"Authorized as {account_name}.")

        data = ProvisioningData(
            wifi_ssid=ssid,
            wifi_password=wifi_password,
            client_id=client_id,
            refresh_token=str(tokens["refresh_token"]),
        )
        provision_serial(port, data)
        print("Provisioning accepted. The ESP32 is restarting.")
        return 0
    except (ProvisioningError, RuntimeError, KeyboardInterrupt) as error:
        print(f"Setup failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
