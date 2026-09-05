"""Local callback and token exchange for Spotify PKCE authorization."""

from __future__ import annotations

import json
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.parse import parse_qs, urlencode, urlparse
from urllib.request import Request, urlopen

from .pkce import REDIRECT_URI


TOKEN_ENDPOINT = "https://accounts.spotify.com/api/token"
PROFILE_ENDPOINT = "https://api.spotify.com/v1/me"


class OAuthCallbackError(RuntimeError):
    pass


def parse_callback_path(path: str, expected_state: str) -> str:
    parsed = urlparse(path)
    if parsed.path != "/callback":
        raise OAuthCallbackError("Unexpected OAuth callback path")
    query = parse_qs(parsed.query)
    received_state = query.get("state", [""])[0]
    if not received_state or received_state != expected_state:
        raise OAuthCallbackError("OAuth state did not match; authorization was rejected")
    if "error" in query:
        raise OAuthCallbackError(f"Spotify authorization failed: {query['error'][0]}")
    code = query.get("code", [""])[0]
    if not code:
        raise OAuthCallbackError("Spotify callback did not include an authorization code")
    return code


def _request_json(request: Request, timeout: float = 15.0) -> dict[str, Any]:
    try:
        with urlopen(request, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8"))
    except HTTPError as error:
        raise OAuthCallbackError(
            f"Spotify returned HTTP {error.code} during authorization"
        ) from error
    except (URLError, TimeoutError, json.JSONDecodeError) as error:
        raise OAuthCallbackError("Spotify authorization request failed") from error


def exchange_code(client_id: str, code: str, verifier: str) -> dict[str, Any]:
    body = urlencode(
        {
            "client_id": client_id,
            "grant_type": "authorization_code",
            "code": code,
            "redirect_uri": REDIRECT_URI,
            "code_verifier": verifier,
        }
    ).encode("ascii")
    request = Request(
        TOKEN_ENDPOINT,
        data=body,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
        method="POST",
    )
    result = _request_json(request)
    if not result.get("access_token") or not result.get("refresh_token"):
        raise OAuthCallbackError("Spotify token response was incomplete")
    return result


def verify_access_token(access_token: str) -> str:
    request = Request(
        PROFILE_ENDPOINT,
        headers={"Authorization": f"Bearer {access_token}"},
        method="GET",
    )
    profile = _request_json(request)
    display_name = profile.get("display_name") or profile.get("account_id")
    return str(display_name or "Spotify user")


def wait_for_callback(expected_state: str, timeout_seconds: float = 180.0) -> str:
    result: dict[str, str | Exception] = {}

    class CallbackHandler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
            try:
                result["code"] = parse_callback_path(self.path, expected_state)
                status = 200
                message = "Spotify setup approved. You can close this tab."
            except OAuthCallbackError as error:
                result["error"] = error
                status = 400
                message = str(error)

            body = (
                "<!doctype html><meta charset='utf-8'><title>Spotify Controller</title>"
                "<style>body{font:18px system-ui;background:#101010;color:#fff;"
                "display:grid;place-items:center;height:100vh;margin:0}main{max-width:34rem;"
                "padding:2rem;background:#181818;border-radius:1rem}</style>"
                f"<main><h1>ESP32 Spotify Controller</h1><p>{message}</p></main>"
            ).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, _format: str, *_args: object) -> None:
            return

    server = HTTPServer(("127.0.0.1", 8765), CallbackHandler)
    server.timeout = timeout_seconds
    server.handle_request()
    server.server_close()

    if "error" in result:
        raise result["error"]  # type: ignore[misc]
    if "code" not in result:
        raise OAuthCallbackError("Timed out waiting for Spotify authorization")
    return str(result["code"])

