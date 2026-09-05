import io
import json
import os
import tempfile
import unittest

from spotify_provision.protocol import (
    ProvisioningData,
    ProvisioningError,
    check_port_access,
    encode_provisioning_message,
    provision_serial,
    redacted_summary,
)


VALID = ProvisioningData(
    wifi_ssid="Studio WiFi",
    wifi_password="correct horse battery staple",
    client_id="0123456789abcdef0123456789abcdef",
    refresh_token="AQD-secret-refresh-token",
)


class FakeSerial:
    def __init__(self, response: bytes):
        self.response = io.BytesIO(response)
        self.written = b""
        self.write_calls: list[bytes] = []
        self.closed = False

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.closed = True

    def reset_input_buffer(self):
        return None

    def write(self, value: bytes):
        self.write_calls.append(value)
        self.written += value
        return len(value)

    def flush(self):
        return None

    def readline(self):
        return self.response.readline()


class ProtocolTests(unittest.TestCase):
    def test_message_is_versioned_ndjson_with_expected_shape(self):
        encoded = encode_provisioning_message(VALID)
        self.assertTrue(encoded.endswith(b"\n"))
        payload = json.loads(encoded)
        self.assertEqual(payload["v"], 1)
        self.assertEqual(payload["type"], "provision")
        self.assertEqual(payload["wifi"]["ssid"], "Studio WiFi")
        self.assertEqual(payload["spotify"]["client_id"], VALID.client_id)

    def test_validation_rejects_oversized_wifi_ssid(self):
        invalid = ProvisioningData("x" * 33, "password", VALID.client_id, "token")
        with self.assertRaisesRegex(ProvisioningError, "SSID"):
            encode_provisioning_message(invalid)

    def test_validation_rejects_short_secured_wifi_password(self):
        invalid = ProvisioningData("wifi", "short", VALID.client_id, "token")
        with self.assertRaisesRegex(ProvisioningError, "password"):
            encode_provisioning_message(invalid)

    def test_redacted_summary_never_contains_secrets(self):
        summary = redacted_summary(VALID)
        self.assertIn("Studio WiFi", summary)
        self.assertNotIn(VALID.wifi_password, summary)
        self.assertNotIn(VALID.refresh_token, summary)
        self.assertNotIn(VALID.client_id, summary)

    def test_serial_provisioning_accepts_success_acknowledgement(self):
        fake = FakeSerial(b'{"v":1,"type":"provision_result","ok":true}\n')
        result = provision_serial(
            "/dev/fake",
            VALID,
            serial_factory=lambda **_kwargs: fake,
            settle_seconds=0,
        )
        self.assertTrue(result)
        self.assertEqual(fake.written, encode_provisioning_message(VALID))
        self.assertTrue(fake.closed)

    def test_long_frame_is_written_in_bounded_chunks(self):
        long_data = ProvisioningData(
            wifi_ssid=VALID.wifi_ssid,
            wifi_password=VALID.wifi_password,
            client_id=VALID.client_id,
            refresh_token="R" * 1800,
        )
        fake = FakeSerial(b'{"v":1,"type":"provision_result","ok":true}\n')

        self.assertTrue(
            provision_serial(
                "/dev/fake",
                long_data,
                serial_factory=lambda **_kwargs: fake,
                settle_seconds=0,
            )
        )
        self.assertEqual(fake.written, encode_provisioning_message(long_data))
        self.assertLessEqual(max(map(len, fake.write_calls)), 128)

    def test_serial_provisioning_surfaces_device_error_without_secrets(self):
        fake = FakeSerial(
            b'{"v":1,"type":"provision_result","ok":false,'
            b'"code":"invalid_payload","message":"Rejected"}\n'
        )
        with self.assertRaisesRegex(ProvisioningError, "Rejected") as raised:
            provision_serial(
                "/dev/fake",
                VALID,
                serial_factory=lambda **_kwargs: fake,
                settle_seconds=0,
            )
        self.assertNotIn(VALID.refresh_token, str(raised.exception))


class PortAccessTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.port = os.path.join(self.directory.name, "ttyFAKE")
        open(self.port, "w").close()

    def denied(self, *, group="dialout", in_session=False, listed=False):
        with self.assertRaises(ProvisioningError) as raised:
            check_port_access(
                self.port,
                access=lambda _path, _mode: False,
                device_group=lambda _port: group,
                session_groups=lambda: [group] if in_session else [],
                listed_in_group=lambda _group: listed,
            )
        return str(raised.exception)

    def test_missing_port_is_reported_before_anything_else_runs(self):
        with self.assertRaisesRegex(ProvisioningError, "does not exist"):
            check_port_access("/dev/definitely-not-a-port")

    def test_unreadable_port_names_the_owning_group_and_how_to_join_it(self):
        message = self.denied()
        self.assertIn("dialout", message)
        self.assertIn("usermod", message)

    def test_stale_session_membership_is_diagnosed_specifically(self):
        message = self.denied(listed=True)
        self.assertIn("log out", message.lower())
        self.assertIn("sg dialout", message)
        self.assertNotIn("usermod", message)

    def test_session_that_already_has_the_group_gets_a_different_hint(self):
        message = self.denied(in_session=True, listed=True)
        self.assertNotIn("usermod", message)
        self.assertIn("denying access", message)

    def test_accessible_port_passes(self):
        check_port_access(self.port, access=lambda _path, _mode: True)


class SerialOpenTests(unittest.TestCase):
    def test_permission_error_on_open_becomes_a_provisioning_error(self):
        def refuse(**_kwargs):
            raise PermissionError(13, "Permission denied")

        with self.assertRaises(ProvisioningError) as raised:
            provision_serial(
                "/dev/fake", VALID, serial_factory=refuse, settle_seconds=0
            )
        self.assertIn("/dev/fake", str(raised.exception))
        self.assertNotIn(VALID.refresh_token, str(raised.exception))


if __name__ == "__main__":
    unittest.main()
