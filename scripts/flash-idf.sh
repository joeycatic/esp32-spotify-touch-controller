#!/usr/bin/env bash
# POSIX equivalent of scripts/flash-idf.ps1.
#
# Opt-in escape hatches, all default-off:
#   ALLOW_NO_BACKUP=1        flash without a verified recovery backup
#   ALLOW_PARTITION_CHANGE=1 write a partition table that differs from the device
#   WRITE_OTA_DATA=1         also write otadata, resetting the boot slot to app0
set -euo pipefail

source "$(dirname "$0")/common-idf.sh"
require_esptool
serial_port="$(resolve_port)"

backup_dir="${PROJECT_ROOT}/build/device-backup"
manifest="${backup_dir}/backup-manifest.json"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

# 1. Recovery backup. flash-idf.ps1 makes this unconditional; there is no
#    manifest on this host yet, so it is an explicit opt-out instead of a
#    silent skip.
if [[ -f "${manifest}" ]]; then
  backup="${backup_dir}/$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["file"])' "${manifest}")"
  expected_bytes="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["bytes"])' "${manifest}")"
  expected_sha="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["sha256"])' "${manifest}")"
  if [[ "$(stat -c%s "${backup}")" != "${expected_bytes}" ]] ||
     [[ "$(sha256sum "${backup}" | cut -d' ' -f1)" != "${expected_sha,,}" ]]; then
    echo "Recovery backup does not match its manifest; flash cancelled." >&2
    exit 1
  fi
  echo "Recovery backup verified against its manifest."
elif [[ "${ALLOW_NO_BACKUP:-0}" != "1" ]]; then
  echo "No verified recovery backup in ${backup_dir}." >&2
  echo "Take one, or re-run with ALLOW_NO_BACKUP=1 to flash anyway." >&2
  exit 1
else
  echo "WARNING: flashing without a recovery backup (ALLOW_NO_BACKUP=1)."
fi

# 2. Flash layout must be the one this project expects.
read -r app_offset bootloader_offset partition_offset flash_size \
        flash_mode flash_freq app_file bootloader_file partition_file \
  < <(python3 - "${IDF_BUILD_DIR}/flasher_args.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
print(d["app"]["offset"], d["bootloader"]["offset"], d["partition-table"]["offset"],
      d["flash_settings"]["flash_size"], d["flash_settings"]["flash_mode"],
      d["flash_settings"]["flash_freq"], d["app"]["file"],
      d["bootloader"]["file"], d["partition-table"]["file"])
PY
)
if [[ "${app_offset}" != "0x10000" || "${bootloader_offset}" != "0x0" ||
      "${partition_offset}" != "0x8000" || "${flash_size}" != "16MB" ]]; then
  echo "Unexpected flash layout; flash cancelled." >&2
  exit 1
fi

bootloader="${IDF_BUILD_DIR}/${bootloader_file}"
partition="${IDF_BUILD_DIR}/${partition_file}"
app="${IDF_BUILD_DIR}/${app_file}"
for image in "${bootloader}" "${partition}" "${app}"; do
  [[ -f "${image}" ]] || { echo "Missing ${image}. Run: make build-idf" >&2; exit 1; }
done

# 3. The application must fit the existing OTA slot.
if [[ "$(stat -c%s "${app}")" -gt $((0x300000)) ]]; then
  echo "Application exceeds the existing OTA slot; flash cancelled." >&2
  exit 1
fi

# 4. Compare the partition table against the device itself rather than against a
#    backup file, so the check still means something when the backup is stale.
"${ESPTOOL}" --chip esp32s3 --port "${serial_port}" --baud 460800 \
  read-flash 0x8000 "$(stat -c%s "${partition}")" "${work}/device-partitions.bin" >/dev/null
if ! cmp -s "${partition}" "${work}/device-partitions.bin"; then
  if [[ "${ALLOW_PARTITION_CHANGE:-0}" != "1" ]]; then
    echo "Partition table differs from the device; flash cancelled." >&2
    echo "Re-run with ALLOW_PARTITION_CHANGE=1 if that is intended." >&2
    exit 1
  fi
  echo "WARNING: overwriting a differing partition table (ALLOW_PARTITION_CHANGE=1)."
fi

# Deliberately preserve NVS (0x9000), which holds provisioning. otadata (0xe000)
# is preserved too unless asked for: the bootloader's existing slot selection is
# correct when this project was already resident, and resetting it is only right
# when some other firmware was.
images=(0x0 "${bootloader}" 0x8000 "${partition}" 0x10000 "${app}")
if [[ "${WRITE_OTA_DATA:-0}" == "1" ]]; then
  images+=(0xe000 "${IDF_BUILD_DIR}/ota_data_initial.bin")
fi

"${ESPTOOL}" --chip esp32s3 --port "${serial_port}" --baud 921600 write-flash \
  --flash-mode "${flash_mode}" --flash-freq "${flash_freq}" --flash-size "${flash_size}" \
  "${images[@]}"

# An independent read-back pass. otadata is excluded: the application claims its
# boot slot on first start, so it legitimately no longer matches the image.
"${ESPTOOL}" --chip esp32s3 --port "${serial_port}" --baud 921600 verify-flash \
  0x0 "${bootloader}" 0x8000 "${partition}" 0x10000 "${app}"

echo
echo "Source-built firmware written and verified."
echo "Disconnect all power for ten seconds, then reconnect UART1 before judging"
echo "the display: every flash ends in a warm reset, which Waveshare's FAQ lists"
echo "as a black-screen cause on this panel. See docs/hardware-checklist.md."
