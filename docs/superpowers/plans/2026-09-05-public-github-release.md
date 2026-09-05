# Public GitHub Release Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Polish the repository for public use and publish `v1.0.0-rc.1` as a clearly labeled GitHub prerelease at `joeycatic/esp32-spotify-touch-controller`.

**Architecture:** Keep firmware behavior and dependency pins unchanged. Add only public-facing documentation and GitHub configuration, validate the existing code locally, then publish `main`, wait for CI, and create a source-only prerelease.

**Tech Stack:** Markdown, YAML, GitHub Actions, Arduino CLI 1.5.1, Arduino-ESP32 3.3.11, Bash, native C++, Python `unittest`, Git, GitHub CLI.

**Spec:** `docs/superpowers/specs/2026-09-05-public-github-release-design.md`

## Global Constraints

- Do not change firmware or provisioning behavior.
- Do not change the pinned toolchain or library versions.
- Do not add a `LICENSE` file.
- Publish under `joeycatic/esp32-spotify-touch-controller`.
- Use `v1.0.0-rc.1` and mark it as a GitHub prerelease.
- State that physical hardware acceptance is pending because the antenna arrived damaged.
- Do not attach unvalidated firmware binaries.
- Stop before publication if tests, compilation, or safety checks fail.

---

### Task 1: Public-facing project documentation

**Files:**
- Modify: `README.md:1-70`
- Create: `CHANGELOG.md`
- Create: `CONTRIBUTING.md`
- Create: `SECURITY.md`

**Interfaces:**
- Consumes: Existing setup, architecture, hardware checklist, Make targets, and credential-safety policy.
- Produces: Public entry-point documentation and the release notes source used by Task 5.

- [ ] **Step 1: Verify the public-release documents do not exist yet**

Run:

```bash
test ! -e CHANGELOG.md && test ! -e CONTRIBUTING.md && test ! -e SECURITY.md
```

Expected: exit 0.

- [ ] **Step 2: Add release status and CI metadata to the README**

Insert directly below the title:

```markdown
[![CI](https://github.com/joeycatic/esp32-spotify-touch-controller/actions/workflows/ci.yml/badge.svg)](https://github.com/joeycatic/esp32-spotify-touch-controller/actions/workflows/ci.yml)

> [!WARNING]
> `v1.0.0-rc.1` is a release candidate. Native tests and firmware compilation pass, but physical acceptance testing on the Waveshare ESP32-S3-Touch-LCD-2 is pending because the test antenna arrived damaged. Expect hardware-specific issues until the [hardware checklist](docs/hardware-checklist.md) is complete.
```

Add a `## Project Status` section before `## Features`:

```markdown
## Project Status

The software is feature-complete for the first release candidate. Physical-device validation is still pending; see the [hardware acceptance checklist](docs/hardware-checklist.md) for the exact tests that remain.
```

Add a final `## Contributing and Security` section:

```markdown
## Contributing and Security

Contributions are welcome; read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request. Report security issues privately as described in [SECURITY.md](SECURITY.md), and never include credentials or tokens in an issue.
```

- [ ] **Step 3: Create the changelog and prerelease notes source**

Create `CHANGELOG.md` with:

```markdown
# Changelog

All notable changes to this project are documented here.

## [1.0.0-rc.1] - 2026-09-05

Initial public release candidate.

### Added

- Touchscreen playback controls for play/pause, previous, next, seek, volume, shuffle, and repeat.
- Spotify Connect device discovery and playback transfer.
- Playlist, song, and Liked Songs browsing with bounded pagination.
- Cover-first Now Playing interface with in-memory artwork handling.
- Browser-based Spotify PKCE authorization and USB provisioning without a client secret.
- Automatic token refresh, validated HTTPS, rate-limit handling, offline recovery, and factory-reset recovery.
- Native C++ tests, Python provisioning tests, reproducible firmware builds, and public CI.

### Release candidate limitations

- Physical acceptance testing is pending because the test antenna arrived damaged.
- Hardware-specific issues may remain until every item in `docs/hardware-checklist.md` has passed.
- No firmware binary is attached; build from source with the pinned local toolchain.

### Requirements

- Waveshare ESP32-S3-Touch-LCD-2.
- Spotify Premium and a Spotify developer application.
- 2.4 GHz Wi-Fi and an existing Spotify Connect playback device.

The controller controls playback on another Spotify Connect device; it does not output audio.
```

- [ ] **Step 4: Create contributor guidance**

Create `CONTRIBUTING.md` with sections that state these exact requirements:

````markdown
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

Use `docs/hardware-checklist.md` for physical acceptance testing. Check an item only after observing it on the supported Waveshare ESP32-S3-Touch-LCD-2.

## Security reports

Do not open a public issue for a vulnerability. Follow `SECURITY.md` instead.
````

- [ ] **Step 5: Create private security-reporting guidance**

Create `SECURITY.md` with:

```markdown
# Security Policy

## Supported versions

Only the latest tagged release receives security fixes. The current `v1.0.0-rc.1` release is a prerelease and has not completed physical hardware validation.

## Reporting a vulnerability

Use GitHub's private vulnerability reporting for this repository. Do not open a public issue for a suspected vulnerability.

Describe the affected revision, impact, reproduction steps, and any suggested mitigation. Do not include Wi-Fi passwords, Spotify authorization codes, access tokens, refresh tokens, client secrets, or other live credentials. Revoke any credential that may have been exposed before sending the report.

## Device security boundary

This hobby-device build does not enable flash encryption or secure boot. An attacker with physical access and suitable equipment may extract stored Wi-Fi and Spotify credentials. Review `docs/architecture.md` before deploying the device outside a trusted environment.
```

- [ ] **Step 6: Validate links and content, then commit**

Run:

```bash
test -f README.md
test -f CHANGELOG.md
test -f CONTRIBUTING.md
test -f SECURITY.md
test -f docs/setup.md
test -f docs/architecture.md
test -f docs/hardware-checklist.md
rg -n 'v1\.0\.0-rc\.1|hardware.*pending|antenna.*damaged' README.md CHANGELOG.md SECURITY.md
git diff --check
```

Expected: every command exits 0 and the search reports the release status in all three files.

Commit:

```bash
git add README.md CHANGELOG.md CONTRIBUTING.md SECURITY.md
git commit -m "docs: prepare public release candidate"
```

---

### Task 2: GitHub community files and continuous integration

**Files:**
- Create: `.github/workflows/ci.yml`
- Create: `.github/ISSUE_TEMPLATE/bug_report.yml`
- Create: `.github/ISSUE_TEMPLATE/feature_request.yml`
- Create: `.github/ISSUE_TEMPLATE/config.yml`

**Interfaces:**
- Consumes: `make bootstrap`, `make test`, and `make build` from the existing repository.
- Produces: A least-privilege CI workflow and structured public issue intake.

- [ ] **Step 1: Verify GitHub configuration is absent**

Run:

```bash
test ! -e .github/workflows/ci.yml
test ! -e .github/ISSUE_TEMPLATE/bug_report.yml
test ! -e .github/ISSUE_TEMPLATE/feature_request.yml
test ! -e .github/ISSUE_TEMPLATE/config.yml
```

Expected: all commands exit 0.

- [ ] **Step 2: Add the CI workflow**

Create `.github/workflows/ci.yml` with:

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]
  workflow_dispatch:

permissions:
  contents: read

concurrency:
  group: ci-${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  test-and-build:
    runs-on: ubuntu-latest
    timeout-minutes: 30
    steps:
      - name: Check out repository
        uses: actions/checkout@v7
      - name: Bootstrap pinned toolchain
        run: make bootstrap
      - name: Run tests
        run: make test
      - name: Compile firmware
        run: make build
```

- [ ] **Step 3: Add structured issue forms**

Create `.github/ISSUE_TEMPLATE/bug_report.yml` with:

```yaml
name: Bug report
description: Report a reproducible problem with the controller, provisioning tool, or build.
title: "[Bug]: "
body:
  - type: markdown
    attributes:
      value: |
        Thanks for reporting a problem. Never include Wi-Fi passwords, Spotify authorization codes, access tokens, refresh tokens, or client secrets. Sanitize logs before submitting.
  - type: textarea
    id: description
    attributes:
      label: Description
      description: What happened?
    validations:
      required: true
  - type: textarea
    id: reproduction
    attributes:
      label: Reproduction steps
      description: List the smallest sequence that reproduces the problem.
    validations:
      required: true
  - type: textarea
    id: expected
    attributes:
      label: Expected behavior
      description: What did you expect instead?
    validations:
      required: true
  - type: input
    id: hardware
    attributes:
      label: Hardware
      description: Board revision, antenna, display, touch controller, and power source.
    validations:
      required: true
  - type: input
    id: version
    attributes:
      label: Version
      description: Release tag or Git commit hash.
    validations:
      required: true
  - type: textarea
    id: logs
    attributes:
      label: Sanitized logs
      description: Paste relevant serial or build output after removing all credentials.
      render: text
```

Create `.github/ISSUE_TEMPLATE/feature_request.yml` with:

```yaml
name: Feature request
description: Suggest a focused improvement to the supported controller.
title: "[Feature]: "
body:
  - type: textarea
    id: problem
    attributes:
      label: Problem
      description: Which user problem would this solve?
    validations:
      required: true
  - type: textarea
    id: proposal
    attributes:
      label: Proposed behavior
      description: Describe the smallest useful behavior change.
    validations:
      required: true
  - type: textarea
    id: alternatives
    attributes:
      label: Alternatives considered
      description: Which workarounds or alternatives have you tried?
    validations:
      required: true
  - type: textarea
    id: hardware
    attributes:
      label: Hardware impact
      description: Note any board, memory, storage, power, display, touch, or antenna implications.
    validations:
      required: true
```

Create `config.yml` with:

```yaml
blank_issues_enabled: false
contact_links:
  - name: Private security report
    url: https://github.com/joeycatic/esp32-spotify-touch-controller/security/advisories/new
    about: Report suspected vulnerabilities privately. Do not open a public issue.
```

- [ ] **Step 4: Parse and validate all YAML files**

Run:

```bash
ruby -e 'require "yaml"; Dir[".github/**/*.yml"].each { |path| YAML.safe_load_file(path, permitted_classes: [], permitted_symbols: [], aliases: true); puts path }'
rg -n 'contents: read|make bootstrap|make test|make build|blank_issues_enabled: false' .github
git diff --check
```

Expected: Ruby prints all four YAML paths; the search finds every required CI command and issue-template setting; all commands exit 0.

- [ ] **Step 5: Commit GitHub configuration**

```bash
git add .github
git commit -m "ci: add public project checks and templates"
```

---

### Task 3: Release-tree hygiene

**Files:**
- Modify: `.gitignore:8-12`
- Delete: `docs/superpowers/`

**Interfaces:**
- Consumes: The approved spec and this implementation plan after Tasks 1 and 2 are complete.
- Produces: A public tree without internal planning artifacts and ignore rules that prevent their return.

- [ ] **Step 1: Confirm internal planning files are currently tracked**

Run:

```bash
git ls-files docs/superpowers | grep -q '^docs/superpowers/'
```

Expected: exit 0.

- [ ] **Step 2: Add the internal documentation path to `.gitignore`**

Add this line immediately after `.superpowers/`:

```gitignore
docs/superpowers/
```

- [ ] **Step 3: Remove internal planning material from the public tree**

Delete the tracked `docs/superpowers` directory using the patch tool. Do not rewrite Git history.

- [ ] **Step 4: Verify public-tree hygiene and commit**

Run:

```bash
test ! -e docs/superpowers
test -z "$(git ls-files docs/superpowers)"
git check-ignore -q docs/superpowers/example.md
test -z "$(git ls-files | rg '^(build|\.arduino|\.tools|\.venv)/' || true)"
git diff --check
```

Expected: all commands exit 0.

Commit:

```bash
git add .gitignore
git add -u docs/superpowers
git commit -m "chore: remove internal planning artifacts"
```

---

### Task 4: Local release validation and branch preparation

**Files:**
- No file changes expected.
- Rename branch: `feature/spotify-touch-controller` to `main`.

**Interfaces:**
- Consumes: The polished public tree from Tasks 1-3.
- Produces: A clean, validated `main` branch ready for publication.

- [ ] **Step 1: Run the complete native and Python test suite**

Run:

```bash
make test
```

Expected: native tests print `All native tests passed`; all Python tests report `OK`; exit 0.

- [ ] **Step 2: Compile the production firmware**

Run:

```bash
make build
```

Expected: Arduino CLI compilation exits 0 and reports flash and RAM usage.

- [ ] **Step 3: Scan all tracked revisions for unsafe material**

Run a filename history check:

```bash
for path_name in .env secrets.json provisioning.json; do
  test -z "$(git log --all --format='%H' -- "$path_name")" || exit 1
done
test -z "$(git log --all --format='%H' -- build .arduino .tools .venv)"
```

Run a content-name scan across every revision, reviewing every reported path without printing matched values:

```bash
for commit_id in $(git rev-list --all); do
  git grep -I -l -E '(client[_ -]?secret|wifi[_ -]?(password|pass)|refresh[_ -]?token|access[_ -]?token|BEGIN (RSA|OPENSSH|EC) PRIVATE KEY|gh[pousr]_[A-Za-z0-9_]{20,}|sk-[A-Za-z0-9]{20,})' "$commit_id" -- . 2>/dev/null | sed "s#^${commit_id}:##"
done | sort -u
```

Expected: only source, tests, public security documentation, issue forms, and historical internal release-planning files that intentionally discuss credential handling or contain the scan expressions. No credential value, private-key file, local environment file, generated build artifact, downloaded toolchain, or provisioning record may appear.

- [ ] **Step 4: Verify repository state**

Run:

```bash
git diff --check
git status --short
git tag --list 'v1.0.0-rc.1'
git remote -v
```

Expected: no diff errors, empty status, no matching tag, and no remote.

- [ ] **Step 5: Rename the branch**

Run:

```bash
git branch -m main
git branch --show-current
```

Expected: `main`.

---

### Task 5: Publish and create the GitHub prerelease

**Files:**
- No local file changes expected.
- External state: public GitHub repository, metadata, tag, prerelease.

**Interfaces:**
- Consumes: Clean validated `main`, authenticated GitHub CLI account `joeycatic`, and `CHANGELOG.md` release notes.
- Produces: Public repository and `v1.0.0-rc.1` GitHub prerelease.

- [ ] **Step 1: Reconfirm GitHub authentication and repository availability**

Run:

```bash
gh auth status
test "$(gh api user --jq .login)" = "joeycatic"
if gh repo view joeycatic/esp32-spotify-touch-controller >/dev/null 2>&1; then exit 1; fi
```

Expected: authenticated as `joeycatic`; target repository does not already exist.

- [ ] **Step 2: Create and push the public repository**

Run:

```bash
gh repo create joeycatic/esp32-spotify-touch-controller \
  --public \
  --source=. \
  --remote=origin \
  --push \
  --description "Standalone Spotify display and touchscreen remote for the Waveshare ESP32-S3-Touch-LCD-2."
```

Expected: GitHub returns the new repository URL and pushes `main`.

- [ ] **Step 3: Configure public repository metadata and security intake**

Run:

```bash
gh repo edit joeycatic/esp32-spotify-touch-controller \
  --enable-wiki=false \
  --add-topic arduino \
  --add-topic embedded \
  --add-topic esp32 \
  --add-topic esp32-s3 \
  --add-topic lvgl \
  --add-topic spotify \
  --add-topic touchscreen
gh api --method PUT repos/joeycatic/esp32-spotify-touch-controller/private-vulnerability-reporting
```

Expected: both commands exit 0.

- [ ] **Step 4: Wait for the pushed `main` CI run**

Run:

```bash
run_id="$(gh run list --repo joeycatic/esp32-spotify-touch-controller --workflow CI --branch main --limit 1 --json databaseId --jq '.[0].databaseId')"
test -n "$run_id"
gh run watch "$run_id" --repo joeycatic/esp32-spotify-touch-controller --exit-status
```

Expected: the `test-and-build` job completes successfully. If it fails, inspect with `gh run view "$run_id" --log-failed`, fix locally, revalidate, push, and wait for the replacement run before continuing.

- [ ] **Step 5: Create and push the annotated prerelease tag**

Run:

```bash
git tag -a v1.0.0-rc.1 -m "ESP32 Spotify Touch Controller v1.0.0-rc.1"
git push origin v1.0.0-rc.1
```

Expected: tag push exits 0.

- [ ] **Step 6: Create the source-only GitHub prerelease**

Run:

```bash
gh release create v1.0.0-rc.1 \
  --repo joeycatic/esp32-spotify-touch-controller \
  --verify-tag \
  --prerelease \
  --title "ESP32 Spotify Touch Controller v1.0.0-rc.1" \
  --notes-file CHANGELOG.md
```

Expected: GitHub returns the prerelease URL. Do not upload any binary assets.

- [ ] **Step 7: Verify the final public state**

Run:

```bash
gh repo view joeycatic/esp32-spotify-touch-controller --json nameWithOwner,url,visibility,defaultBranchRef,description,repositoryTopics
gh release view v1.0.0-rc.1 --repo joeycatic/esp32-spotify-touch-controller --json url,isPrerelease,tagName,title,assets
git status --short --branch
```

Expected: visibility `PUBLIC`, default branch `main`, all seven topics present, `isPrerelease` true, tag `v1.0.0-rc.1`, no uploaded assets, and a clean local branch tracking `origin/main`.
