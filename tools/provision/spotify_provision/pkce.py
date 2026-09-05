"""Spotify Authorization Code with PKCE helpers."""

from __future__ import annotations

import base64
import hashlib
import secrets
import string
from urllib.parse import urlencode


REDIRECT_URI = "http://127.0.0.1:8765/callback"
AUTHORIZE_ENDPOINT = "https://accounts.spotify.com/authorize"
SCOPES = (
    "user-read-playback-state",
    "user-read-currently-playing",
    "user-modify-playback-state",
    "playlist-read-private",
    "playlist-read-collaborative",
    "user-library-read",
)
_VERIFIER_ALPHABET = string.ascii_letters + string.digits + "-._~"


def generate_verifier(length: int = 64) -> str:
    if not 43 <= length <= 128:
        raise ValueError("PKCE verifier length must be between 43 and 128")
    return "".join(secrets.choice(_VERIFIER_ALPHABET) for _ in range(length))


def code_challenge(verifier: str) -> str:
    digest = hashlib.sha256(verifier.encode("ascii")).digest()
    return base64.urlsafe_b64encode(digest).rstrip(b"=").decode("ascii")


def authorization_url(client_id: str, challenge: str, state: str) -> str:
    query = urlencode(
        {
            "client_id": client_id,
            "response_type": "code",
            "redirect_uri": REDIRECT_URI,
            "code_challenge_method": "S256",
            "code_challenge": challenge,
            "state": state,
            "scope": " ".join(SCOPES),
        }
    )
    return f"{AUTHORIZE_ENDPOINT}?{query}"

