# Contributing

Thanks for helping improve the ESP32 Spotify Touch Controller.

## Development setup

Use Linux, macOS, or WSL with Python 3, `curl`, `tar`, `git`, and `g++`. From the repository root:

```bash
make bootstrap
make test
make build
```

The bootstrap command installs pinned tools under ignored repository directories.

## Pull requests

- Keep each change focused and explain its user-visible effect.
- Run `make test` and `make build` before submitting.
- Update documentation when setup, behavior, hardware support, or security assumptions change.
- Include physical test results for hardware-dependent changes and name the exact board revision and peripherals used.
- Do not commit generated build output, downloaded toolchains, local environments, or provisioning data.

## Bug reports

Use the bug report form and include reproducible steps, expected and actual behavior, board revision, build revision, and sanitized serial output. Never include Wi-Fi credentials, Spotify authorization codes, access tokens, refresh tokens, or client secrets.

## Hardware validation

Use `docs/hardware-checklist.md` for physical acceptance testing. Record results separately for the exact Waveshare ESP32-S3-Touch-LCD-2 or ESP32-S3-Touch-LCD-7B revision tested. Hardware-driver changes require regression results for both profiles before release.

## Security reports

Do not open a public issue for a vulnerability. Follow `SECURITY.md` instead.
