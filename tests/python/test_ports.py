import types
import unittest

from spotify_provision.ports import PortResolutionError, resolve_port


def port(device, vid=None, description="", manufacturer="", product="", pid=None):
    return types.SimpleNamespace(
        device=device,
        vid=vid,
        pid=pid,
        description=description,
        manufacturer=manufacturer,
        product=product,
    )


class PortResolverTests(unittest.TestCase):
    def test_explicit_port_always_wins(self):
        self.assertEqual(resolve_port([], "/dev/custom"), "/dev/custom")

    def test_ignores_macos_pseudo_ports(self):
        with self.assertRaises(PortResolutionError):
            resolve_port([port("/dev/cu.Bluetooth-Incoming-Port")])

    def test_selects_espressif_native_usb(self):
        self.assertEqual(
            resolve_port([port("/dev/cu.usbmodem1", 0x303A)]),
            "/dev/cu.usbmodem1",
        )

    def test_prefers_7b_uart_when_both_connectors_exist(self):
        self.assertEqual(
            resolve_port(
                [
                    port("/dev/cu.usbmodem1", 0x303A),
                    port("/dev/cu.usbserial-7b", 0x1A86, "USB Single Serial"),
                ]
            ),
            "/dev/cu.usbserial-7b",
        )

    def test_recognizes_ch343_by_usb_product_id(self):
        self.assertEqual(
            resolve_port([port("/dev/cu.wchusbserial1", 0x1A86, pid=0x55D3)]),
            "/dev/cu.wchusbserial1",
        )

    def test_does_not_accept_unrelated_wch_adapter(self):
        with self.assertRaises(PortResolutionError):
            resolve_port([port("/dev/cu.usbserial-x", 0x1A86, "CH340")])

    def test_multiple_7b_adapters_are_ambiguous(self):
        with self.assertRaisesRegex(PortResolutionError, "Multiple supported"):
            resolve_port(
                [
                    port("/dev/cu.usbserial-a", 0x1A86, "CH343"),
                    port("/dev/cu.usbserial-b", 0x1A86, "CH343"),
                ]
            )

    def test_multiple_espressif_ports_are_ambiguous(self):
        with self.assertRaisesRegex(PortResolutionError, "Multiple supported"):
            resolve_port(
                [
                    port("/dev/cu.usbmodem-a", 0x303A),
                    port("/dev/cu.usbmodem-b", 0x303A),
                ]
            )


if __name__ == "__main__":
    unittest.main()
