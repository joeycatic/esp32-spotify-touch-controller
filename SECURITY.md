# Security Policy

## Supported versions

Only the latest tagged release receives security fixes. The current `v1.0.0-rc.2` release is a prerelease; unreleased universal-board changes have not completed physical validation.

## Reporting a vulnerability

Use GitHub's private vulnerability reporting for this repository. Do not open a public issue for a suspected vulnerability.

Describe the affected revision, impact, reproduction steps, and any suggested mitigation. Do not include Wi-Fi passwords, Spotify authorization codes, access tokens, refresh tokens, client secrets, or other live credentials. Revoke any credential that may have been exposed before sending the report.

## Device security boundary

This hobby-device build does not enable flash encryption or secure boot. An attacker with physical access and suitable equipment may extract stored Wi-Fi and Spotify credentials. Review `docs/architecture.md` before deploying the device outside a trusted environment.
