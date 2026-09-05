import unittest
from urllib.parse import parse_qs, urlparse

from spotify_provision.pkce import authorization_url, code_challenge, generate_verifier


class PkceTests(unittest.TestCase):
    def test_rfc7636_verifier_produces_expected_challenge(self):
        verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"
        self.assertEqual(
            code_challenge(verifier),
            "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM",
        )

    def test_generated_verifier_meets_pkce_length_and_alphabet(self):
        verifier = generate_verifier()
        self.assertGreaterEqual(len(verifier), 43)
        self.assertLessEqual(len(verifier), 128)
        self.assertRegex(verifier, r"^[A-Za-z0-9._~-]+$")

    def test_authorization_url_contains_exact_redirect_and_scopes(self):
        url = authorization_url("client123", "challenge", "state123")
        parsed = urlparse(url)
        query = parse_qs(parsed.query)

        self.assertEqual(parsed.scheme, "https")
        self.assertEqual(parsed.netloc, "accounts.spotify.com")
        self.assertEqual(query["client_id"], ["client123"])
        self.assertEqual(query["redirect_uri"], ["http://127.0.0.1:8765/callback"])
        self.assertEqual(query["code_challenge_method"], ["S256"])
        self.assertEqual(query["state"], ["state123"])
        self.assertEqual(
            set(query["scope"][0].split()),
            {
                "user-read-playback-state",
                "user-read-currently-playing",
                "user-modify-playback-state",
                "playlist-read-private",
                "playlist-read-collaborative",
                "user-library-read",
            },
        )


if __name__ == "__main__":
    unittest.main()

