# Public GitHub Release Design

## Objective

Prepare the ESP32 Spotify Touch Controller repository for public use and publish it as `joeycatic/esp32-spotify-touch-controller` with an initial `v1.0.0-rc.1` GitHub prerelease.

The release must accurately communicate that code-level validation is complete while physical validation on the Waveshare ESP32-S3-Touch-LCD-2 remains pending because the available antenna arrived damaged.

## Scope

This pass changes repository presentation, contributor guidance, automation, and release metadata. It does not change firmware behavior, provisioning behavior, supported hardware, or dependency versions.

The public tree will contain:

- A README with a visible prerelease and hardware-validation notice, CI badge, concise feature summary, quick start, documentation links, limitations, and credential-safety guidance.
- `CHANGELOG.md` with an entry for `v1.0.0-rc.1` dated 2026-09-05.
- `CONTRIBUTING.md` with setup, test, build, pull-request, hardware-reporting, and credential-safety instructions.
- `SECURITY.md` directing vulnerability reports to GitHub private vulnerability reporting and warning reporters not to include credentials or tokens.
- GitHub issue forms for reproducible bug reports and feature requests, plus issue-template configuration that disables blank issues.
- A GitHub Actions workflow that runs tests and compiles the firmware on pushes to `main`, pull requests targeting `main`, and manual dispatch.

No `LICENSE` file will be added. The repository will therefore remain unlicensed, as explicitly requested.

## Public-Tree Hygiene

The tracked `docs/superpowers` directory contains internal planning material that is not part of the user documentation. It will be removed from the release tree, and `.superpowers/` plus `docs/superpowers/` will remain ignored so future local planning artifacts are not accidentally committed.

Generated toolchains, Python environments, Arduino package data, build outputs, caches, local environment files, and provisioning data will remain ignored. Existing Git history will not be rewritten because the audit found no tracked credential files or generated build artifacts.

## Continuous Integration

The CI workflow will use Ubuntu and perform these steps:

1. Check out the repository.
2. Install the pinned local toolchain with `make bootstrap`.
3. Run native C++ and Python tests with `make test`.
4. Compile the production firmware with `make build`.

Caching is optional and will be omitted from the initial workflow to keep behavior simple and reproducible. The workflow must use least-privilege read-only repository permissions and must not require repository secrets.

## Validation Gate

Publication is allowed only after all of the following succeed from the release tree:

- `make test`
- `make build`
- A scan of all tracked revisions for credential files, private keys, GitHub tokens, Spotify secrets, Wi-Fi passwords, refresh tokens, and access tokens
- A check that ignored build/toolchain directories are absent from the tracked file list
- A clean Git worktree after committing the release-preparation changes

Expected identifier names and security documentation may contain credential-related words; the scan must distinguish those references from actual secret values. If a test, build, or safety check fails, publication stops until the failure is resolved and reverified.

## GitHub Publication

After validation:

1. Rename the current local branch from `feature/spotify-touch-controller` to `main`.
2. Create the public repository `joeycatic/esp32-spotify-touch-controller`.
3. Set the description to “Standalone Spotify display and touchscreen remote for the Waveshare ESP32-S3-Touch-LCD-2.”
4. Add the topics `arduino`, `embedded`, `esp32`, `esp32-s3`, `lvgl`, `spotify`, and `touchscreen`.
5. Disable the repository wiki because maintained documentation lives in the repository.
6. Push `main` and set it as the default branch.
7. Create and push the annotated tag `v1.0.0-rc.1`.
8. Create a GitHub prerelease from that tag.

The prerelease will contain GitHub-generated source archives only. It will not attach firmware binaries because physical hardware validation is incomplete.

## Release Notes

The release notes will summarize the touchscreen playback controls, Spotify Connect device transfer, library browsing, PKCE USB provisioning, artwork handling, recovery behavior, tests, and pinned local toolchain.

They will also state:

- This is a release candidate.
- Physical hardware acceptance is pending because the test antenna arrived damaged.
- Users should expect hardware-specific issues until the acceptance checklist is completed.
- Spotify Premium, a Spotify developer application, 2.4 GHz Wi-Fi, and the exact supported Waveshare board are required.
- The ESP32 controls another Spotify Connect device and does not output audio.

## Completion Criteria

The work is complete when the polished release commit is present on `main`, local validation passes, the public GitHub repository is available under the approved name, the repository metadata is configured, and GitHub shows `v1.0.0-rc.1` as a prerelease with the hardware-validation limitation clearly disclosed.
