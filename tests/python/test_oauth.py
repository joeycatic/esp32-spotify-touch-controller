import unittest

from spotify_provision.oauth import OAuthCallbackError, parse_callback_path


class OAuthCallbackTests(unittest.TestCase):
    def test_valid_callback_returns_authorization_code(self):
        code = parse_callback_path(
            "/callback?code=auth-code-123&state=expected", "expected"
        )
        self.assertEqual(code, "auth-code-123")

    def test_callback_rejects_mismatched_state(self):
        with self.assertRaisesRegex(OAuthCallbackError, "state"):
            parse_callback_path(
                "/callback?code=auth-code-123&state=attacker", "expected"
            )

    def test_callback_reports_spotify_denial(self):
        with self.assertRaisesRegex(OAuthCallbackError, "access_denied"):
            parse_callback_path(
                "/callback?error=access_denied&state=expected", "expected"
            )

    def test_callback_rejects_missing_code(self):
        with self.assertRaisesRegex(OAuthCallbackError, "code"):
            parse_callback_path("/callback?state=expected", "expected")


if __name__ == "__main__":
    unittest.main()

