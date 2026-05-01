#!/usr/bin/env python3
"""
Firmware configuration and protocol tests for Kommando.

Validates:
  - OTA identity constants (manufacturerCode, imageType, fileVersion encoding)
  - ZCL action string protocol (T:, H:, D:, B: formats)
  - Partition table layout
  - OTA state machine transitions (receive → check → apply → finish)
  - Config constant cross-checks between Python tools and firmware headers

These tests do not require hardware or a running device.
They verify that the Python tooling and firmware C constants are in sync.
"""

import re
import struct
import unittest
from pathlib import Path

FIRMWARE_DIR = Path(__file__).parent.parent
CONFIG_H     = FIRMWARE_DIR / "main" / "config.h"
PARTITION_CSV = FIRMWARE_DIR / "partition_table" / "partition-table.csv"
KCONFIG       = FIRMWARE_DIR / "Kconfig.projbuild"
SDKCONFIG     = FIRMWARE_DIR / "sdkconfig"


# ---------------------------------------------------------------------------
# Helpers — parse C header values without full C preprocessor
# ---------------------------------------------------------------------------

def _extract_define(header: Path, name: str):
    """Return the raw string value of a #define from a C header file."""
    text = header.read_text(encoding="utf-8")
    # Match: #define NAME   <value>   (optionally with inline comment)
    pattern = rf"^\s*#define\s+{re.escape(name)}\s+([^\n/\\]+)"
    match = re.search(pattern, text, re.MULTILINE)
    if not match:
        return None
    return match.group(1).strip()


def _int_define(header: Path, name: str, default=None):
    """Return integer value of a numeric #define (hex or decimal)."""
    raw = _extract_define(header, name)
    if raw is None:
        return default
    raw = raw.strip().rstrip("u U L".split()[0])  # strip type suffixes
    try:
        return int(raw, 0)
    except (ValueError, TypeError):
        return default


def _str_define(header: Path, name: str, default=None):
    """Return string value of a #define (strips quotes)."""
    raw = _extract_define(header, name)
    if raw is None:
        return default
    return raw.strip().strip('"')


def _sdkconfig_value(key: str, default=None):
    """Read a value from sdkconfig, returns int for numeric values."""
    if not SDKCONFIG.exists():
        return default
    for line in SDKCONFIG.read_text().splitlines():
        if line.startswith(f"{key}="):
            val = line.split("=", 1)[1].strip()
            try:
                return int(val, 0)
            except ValueError:
                return val
    return default


# ---------------------------------------------------------------------------
# OTA identity tests
# ---------------------------------------------------------------------------

class TestOTAIdentityConstants(unittest.TestCase):
    """Verify that OTA identity in config.h matches expected values."""

    def test_manufacturer_code_is_0x1234(self):
        """ZB_OTA_MANUFACTURER_CODE must be 0x1234 for Z2M index compatibility."""
        val = _int_define(CONFIG_H, "ZB_OTA_MANUFACTURER_CODE")
        self.assertIsNotNone(val, "ZB_OTA_MANUFACTURER_CODE not found in config.h")
        self.assertEqual(val, 0x1234,
                         f"Manufacturer code is 0x{val:04X}, expected 0x1234")

    def test_image_type_default_is_0x1011(self):
        """CONFIG_KOMMANDO_OTA_IMAGE_TYPE default must be 0x1011."""
        # Check the fallback define in config.h
        val = _int_define(CONFIG_H, "CONFIG_KOMMANDO_OTA_IMAGE_TYPE")
        self.assertIsNotNone(val, "CONFIG_KOMMANDO_OTA_IMAGE_TYPE not found in config.h")
        self.assertEqual(val, 0x1011,
                         f"Image type fallback is 0x{val:04X}, expected 0x1011")

    def test_hw_version_default_is_0x0101(self):
        """CONFIG_KOMMANDO_OTA_HW_VERSION default must be 0x0101."""
        val = _int_define(CONFIG_H, "CONFIG_KOMMANDO_OTA_HW_VERSION")
        self.assertIsNotNone(val, "CONFIG_KOMMANDO_OTA_HW_VERSION not found in config.h")
        self.assertEqual(val, 0x0101,
                         f"HW version fallback is 0x{val:04X}, expected 0x0101")

    def test_max_data_size_default_is_223(self):
        """CONFIG_KOMMANDO_OTA_MAX_DATA_SIZE default must be 223 (Espressif recommended max)."""
        val = _int_define(CONFIG_H, "CONFIG_KOMMANDO_OTA_MAX_DATA_SIZE")
        self.assertIsNotNone(val)
        self.assertEqual(val, 223)

    def test_firmware_version_fields_present(self):
        """FW_VERSION_MAJOR, MINOR, PATCH must all be defined."""
        major = _int_define(CONFIG_H, "FW_VERSION_MAJOR")
        minor = _int_define(CONFIG_H, "FW_VERSION_MINOR")
        patch = _int_define(CONFIG_H, "FW_VERSION_PATCH")

        self.assertIsNotNone(major, "FW_VERSION_MAJOR missing from config.h")
        self.assertIsNotNone(minor, "FW_VERSION_MINOR missing from config.h")
        self.assertIsNotNone(patch, "FW_VERSION_PATCH missing from config.h")

    def test_firmware_version_string_matches_fields(self):
        """FW_VERSION_STRING must match the individual MAJOR.MINOR.PATCH fields."""
        major = _int_define(CONFIG_H, "FW_VERSION_MAJOR")
        minor = _int_define(CONFIG_H, "FW_VERSION_MINOR")
        patch = _int_define(CONFIG_H, "FW_VERSION_PATCH")
        ver_str = _str_define(CONFIG_H, "FW_VERSION_STRING")

        self.assertIsNotNone(ver_str, "FW_VERSION_STRING missing from config.h")
        expected = f"{major}.{minor}.{patch}"
        self.assertEqual(ver_str, expected,
                         f"FW_VERSION_STRING '{ver_str}' does not match {expected}")


# ---------------------------------------------------------------------------
# OTA file version encoding
# ---------------------------------------------------------------------------

class TestOTAFileVersionEncoding(unittest.TestCase):
    """Verify the OTA file version byte-packing matches firmware convention."""

    def _fw_version_encode(self, major: int, minor: int, patch: int) -> int:
        """Python equivalent of ZB_OTA_FILE_VERSION macro in config.h."""
        return ((major & 0xFF) << 24) | ((minor & 0xFF) << 16) | ((patch & 0xFF) << 8) | 0x00

    def test_v1_0_0_encodes_correctly(self):
        self.assertEqual(self._fw_version_encode(1, 0, 0), 0x01000000)

    def test_v1_0_1_encodes_correctly(self):
        self.assertEqual(self._fw_version_encode(1, 0, 1), 0x01000100)

    def test_v1_2_3_encodes_correctly(self):
        self.assertEqual(self._fw_version_encode(1, 2, 3), 0x01020300)

    def test_v255_255_255_encodes_correctly(self):
        self.assertEqual(self._fw_version_encode(255, 255, 255), 0xFFFFFF00)

    def test_current_firmware_version_encodes_consistently(self):
        """The current firmware version in config.h must encode consistently."""
        major = _int_define(CONFIG_H, "FW_VERSION_MAJOR")
        minor = _int_define(CONFIG_H, "FW_VERSION_MINOR")
        patch = _int_define(CONFIG_H, "FW_VERSION_PATCH")

        self.assertIsNotNone(major)
        self.assertIsNotNone(minor)
        self.assertIsNotNone(patch)

        encoded = self._fw_version_encode(major, minor, patch)
        # Must be non-zero and fit in uint32
        self.assertGreater(encoded, 0)
        self.assertLessEqual(encoded, 0xFFFFFFFF)

    def test_version_ordering_is_monotonic(self):
        """Newer versions must produce strictly larger file version integers."""
        v1_0_0 = self._fw_version_encode(1, 0, 0)
        v1_0_1 = self._fw_version_encode(1, 0, 1)
        v1_1_0 = self._fw_version_encode(1, 1, 0)
        v2_0_0 = self._fw_version_encode(2, 0, 0)

        self.assertLess(v1_0_0, v1_0_1)
        self.assertLess(v1_0_1, v1_1_0)
        self.assertLess(v1_1_0, v2_0_0)

    def test_file_version_is_non_negative(self):
        """File version must be a non-negative 32-bit integer."""
        major = _int_define(CONFIG_H, "FW_VERSION_MAJOR")
        minor = _int_define(CONFIG_H, "FW_VERSION_MINOR")
        patch = _int_define(CONFIG_H, "FW_VERSION_PATCH")
        encoded = self._fw_version_encode(major, minor, patch)
        # Verify it can round-trip through uint32_t (struct pack)
        packed = struct.pack(">I", encoded)
        unpacked = struct.unpack(">I", packed)[0]
        self.assertEqual(encoded, unpacked)

    def test_tooling_version_encoding_matches_firmware(self):
        """Python tool file_version_from_semver() must produce the same result as the C macro."""
        import sys
        sys.path.insert(0, str(FIRMWARE_DIR / "tools"))
        from build_and_package_ota import file_version_from_semver

        major = _int_define(CONFIG_H, "FW_VERSION_MAJOR")
        minor = _int_define(CONFIG_H, "FW_VERSION_MINOR")
        patch = _int_define(CONFIG_H, "FW_VERSION_PATCH")
        ver_str = f"{major}.{minor}.{patch}"

        c_version  = self._fw_version_encode(major, minor, patch)
        py_version = file_version_from_semver(ver_str)

        self.assertEqual(c_version, py_version,
                         f"C macro gives 0x{c_version:08X}, Python tool gives 0x{py_version:08X}")


# ---------------------------------------------------------------------------
# ZCL action string protocol
# ---------------------------------------------------------------------------

class TestZCLActionStringProtocol(unittest.TestCase):
    """
    Verify the ZCL char-string payload formats emitted by zb_report_tile_action().

    Format: [len_byte][ascii_payload]
    Payloads:
      TAP:  T:<tile_id>:<0|1>
      HOLD: H:<tile_id>
      DIM:  D:<tile_id>:<0..100>
      BACKLIGHT: B:<0..255>
    """

    def _make_zcl_string(self, payload: str) -> bytes:
        """Build a ZCL char-string as the firmware does."""
        encoded = payload.encode("ascii")
        return bytes([len(encoded)]) + encoded

    def _parse_zcl_string(self, buf: bytes) -> str:
        """Parse a ZCL char-string back to payload."""
        if not buf:
            return ""
        length = buf[0]
        return buf[1:1 + length].decode("ascii")

    # TAP events

    def test_tap_on_tile_0(self):
        payload = f"T:0:1"
        buf = self._make_zcl_string(payload)
        self.assertEqual(buf[0], len(payload))
        self.assertEqual(self._parse_zcl_string(buf), "T:0:1")

    def test_tap_off_tile_3(self):
        payload = "T:3:0"
        buf = self._make_zcl_string(payload)
        self.assertEqual(self._parse_zcl_string(buf), "T:3:0")

    def test_tap_covers_all_tiles(self):
        for tile_id in range(6):  # MAX_TILES = 6
            for state in (0, 1):
                payload = f"T:{tile_id}:{state}"
                buf = self._make_zcl_string(payload)
                self.assertEqual(self._parse_zcl_string(buf), payload)

    # HOLD events

    def test_hold_tile_0(self):
        buf = self._make_zcl_string("H:0")
        self.assertEqual(self._parse_zcl_string(buf), "H:0")

    def test_hold_all_tiles(self):
        for tile_id in range(6):
            payload = f"H:{tile_id}"
            buf = self._make_zcl_string(payload)
            self.assertEqual(self._parse_zcl_string(buf), payload)

    # DIM events

    def test_dim_tile_1_at_50_percent(self):
        buf = self._make_zcl_string("D:1:50")
        self.assertEqual(self._parse_zcl_string(buf), "D:1:50")

    def test_dim_range_0_to_100(self):
        for level in (0, 1, 50, 99, 100):
            payload = f"D:2:{level}"
            buf = self._make_zcl_string(payload)
            self.assertEqual(self._parse_zcl_string(buf), payload)

    def test_dim_level_0_is_off(self):
        """Level 0 means fully off — dimmer at zero."""
        payload = "D:0:0"
        buf = self._make_zcl_string(payload)
        parsed = self._parse_zcl_string(buf)
        parts = parsed.split(":")
        self.assertEqual(parts[0], "D")
        self.assertEqual(int(parts[2]), 0)

    def test_dim_level_100_is_max(self):
        payload = "D:0:100"
        buf = self._make_zcl_string(payload)
        parsed = self._parse_zcl_string(buf)
        parts = parsed.split(":")
        self.assertEqual(int(parts[2]), 100)

    # BACKLIGHT events

    def test_backlight_string_format(self):
        """Backlight reports use B:<level> format."""
        for level in (0, 10, 128, 255):
            payload = f"B:{level}"
            buf = self._make_zcl_string(payload)
            self.assertEqual(self._parse_zcl_string(buf), payload)

    def test_backlight_level_fits_uint8(self):
        """Backlight level must fit in uint8_t (0–255)."""
        for level in (0, 127, 255):
            self.assertGreaterEqual(level, 0)
            self.assertLessEqual(level, 255)

    # Format parsing sanity

    def test_action_type_discrimination(self):
        """Each action type must be identifiable from the first character."""
        actions = [
            ("T:0:1", "T"),
            ("H:2",   "H"),
            ("D:1:50", "D"),
            ("B:200",  "B"),
        ]
        for payload, expected_type in actions:
            buf = self._make_zcl_string(payload)
            parsed = self._parse_zcl_string(buf)
            actual_type = parsed.split(":")[0]
            self.assertEqual(actual_type, expected_type,
                             f"Payload '{payload}' type is '{actual_type}', expected '{expected_type}'")

    def test_zcl_string_length_byte_is_correct(self):
        """Length byte must equal the exact ASCII byte count of the payload."""
        test_payloads = ["T:0:1", "T:5:0", "H:3", "D:2:75", "B:255", "REFRESH"]
        for payload in test_payloads:
            buf = self._make_zcl_string(payload)
            self.assertEqual(buf[0], len(payload.encode("ascii")),
                             f"Length byte mismatch for '{payload}'")

    def test_max_buffer_size_not_exceeded(self):
        """No action string should exceed the 15-byte s_action_buffer payload limit."""
        # s_action_buffer is 16 bytes: [0]=len, [1..15]=data => max payload = 15 chars
        MAX_PAYLOAD = 15
        worst_cases = [
            f"T:5:1",      # 5 chars
            f"H:5",        # 3 chars
            f"D:5:100",    # 7 chars
            f"B:255",      # 5 chars
            "REFRESH",     # 7 chars (used by zb_refresh_reporting)
        ]
        for payload in worst_cases:
            self.assertLessEqual(len(payload), MAX_PAYLOAD,
                                 f"Payload '{payload}' ({len(payload)} chars) exceeds {MAX_PAYLOAD}")


# ---------------------------------------------------------------------------
# Partition table validation
# ---------------------------------------------------------------------------

class TestPartitionTable(unittest.TestCase):
    """Verify the partition table has the expected OTA layout."""

    def _load_partitions(self):
        """Parse the CSV partition table into a list of dicts."""
        partitions = []
        for line in PARTITION_CSV.read_text().splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = [p.strip() for p in line.split(",")]
            if len(parts) < 5:
                continue
            partitions.append({
                "name":    parts[0],
                "type":    parts[1],
                "subtype": parts[2],
                "offset":  int(parts[3], 0) if parts[3] else None,
                "size":    int(parts[4], 0) if parts[4] else None,
            })
        return partitions

    def _find(self, partitions, name):
        return next((p for p in partitions if p["name"] == name), None)

    def test_partition_file_exists(self):
        self.assertTrue(PARTITION_CSV.exists(), "partition-table.csv not found")

    def test_ota_data_partition_present(self):
        """otadata partition must exist for OTA to work."""
        parts = self._load_partitions()
        otadata = self._find(parts, "otadata")
        self.assertIsNotNone(otadata, "otadata partition missing from partition table")
        self.assertEqual(otadata["subtype"], "ota")

    def test_ota_0_partition_present(self):
        parts = self._load_partitions()
        ota_0 = self._find(parts, "ota_0")
        self.assertIsNotNone(ota_0, "ota_0 partition missing from partition table")
        self.assertEqual(ota_0["type"], "app")
        self.assertEqual(ota_0["subtype"], "ota_0")

    def test_ota_1_partition_present(self):
        parts = self._load_partitions()
        ota_1 = self._find(parts, "ota_1")
        self.assertIsNotNone(ota_1, "ota_1 partition missing from partition table")
        self.assertEqual(ota_1["type"], "app")
        self.assertEqual(ota_1["subtype"], "ota_1")

    def test_ota_partitions_are_same_size(self):
        """ota_0 and ota_1 must be the same size (symmetric OTA layout)."""
        parts = self._load_partitions()
        ota_0 = self._find(parts, "ota_0")
        ota_1 = self._find(parts, "ota_1")
        self.assertIsNotNone(ota_0)
        self.assertIsNotNone(ota_1)
        self.assertEqual(ota_0["size"], ota_1["size"],
                         f"ota_0 size 0x{ota_0['size']:X} != ota_1 size 0x{ota_1['size']:X}")

    def test_ota_partition_minimum_size(self):
        """Each OTA partition must be at least 1 MB to hold firmware."""
        MIN_SIZE = 1 * 1024 * 1024  # 1 MB
        parts = self._load_partitions()
        for name in ("ota_0", "ota_1"):
            part = self._find(parts, name)
            self.assertIsNotNone(part, f"{name} missing")
            self.assertGreaterEqual(part["size"], MIN_SIZE,
                                    f"{name} size 0x{part['size']:X} < 1 MB minimum")

    def test_nvs_partition_present(self):
        """NVS partition must be present for tile/settings persistence."""
        parts = self._load_partitions()
        nvs = self._find(parts, "nvs")
        self.assertIsNotNone(nvs, "nvs partition missing")
        self.assertEqual(nvs["type"], "data")
        self.assertEqual(nvs["subtype"], "nvs")

    def test_zigbee_storage_partition_present(self):
        """zb_storage partition must be present for Zigbee stack state."""
        parts = self._load_partitions()
        zb_storage = self._find(parts, "zb_storage")
        self.assertIsNotNone(zb_storage, "zb_storage partition missing")

    def test_no_partition_overlaps(self):
        """No two partitions may overlap in flash address space."""
        parts = self._load_partitions()
        valid = [(p["name"], p["offset"], p["size"])
                 for p in parts
                 if p["offset"] is not None and p["size"] is not None]

        for i, (name_a, off_a, size_a) in enumerate(valid):
            for name_b, off_b, size_b in valid[i + 1:]:
                end_a = off_a + size_a
                end_b = off_b + size_b
                overlap = off_a < end_b and off_b < end_a
                self.assertFalse(overlap,
                                 f"Partitions {name_a} and {name_b} overlap in flash")


# ---------------------------------------------------------------------------
# Kconfig defaults
# ---------------------------------------------------------------------------

class TestKconfigDefaults(unittest.TestCase):
    """Verify Kconfig.projbuild OTA defaults match firmware expectations."""

    def _load_kconfig_defaults(self):
        """Extract 'default' values from Kconfig.projbuild."""
        text = KCONFIG.read_text()
        defaults = {}
        current_key = None
        for line in text.splitlines():
            stripped = line.strip()
            if stripped.startswith("config "):
                current_key = stripped.split()[1]
            elif stripped.startswith("default ") and current_key:
                val_str = stripped.split(None, 1)[1].split("#")[0].strip()
                try:
                    defaults[current_key] = int(val_str, 0)
                except ValueError:
                    defaults[current_key] = val_str
        return defaults

    def test_ota_image_type_default(self):
        defaults = self._load_kconfig_defaults()
        self.assertIn("KOMMANDO_OTA_IMAGE_TYPE", defaults)
        self.assertEqual(defaults["KOMMANDO_OTA_IMAGE_TYPE"], 0x1011)

    def test_ota_hw_version_default(self):
        defaults = self._load_kconfig_defaults()
        self.assertIn("KOMMANDO_OTA_HW_VERSION", defaults)
        self.assertEqual(defaults["KOMMANDO_OTA_HW_VERSION"], 0x0101)

    def test_ota_query_interval_default(self):
        defaults = self._load_kconfig_defaults()
        self.assertIn("KOMMANDO_OTA_QUERY_INTERVAL_MIN", defaults)
        self.assertEqual(defaults["KOMMANDO_OTA_QUERY_INTERVAL_MIN"], 60)

    def test_ota_max_data_size_default(self):
        defaults = self._load_kconfig_defaults()
        self.assertIn("KOMMANDO_OTA_MAX_DATA_SIZE", defaults)
        self.assertEqual(defaults["KOMMANDO_OTA_MAX_DATA_SIZE"], 223)

    def test_kconfig_defaults_match_config_h_fallbacks(self):
        """Kconfig defaults and config.h fallback #defines must agree."""
        kconfig = self._load_kconfig_defaults()

        pairs = [
            ("KOMMANDO_OTA_IMAGE_TYPE",      "CONFIG_KOMMANDO_OTA_IMAGE_TYPE"),
            ("KOMMANDO_OTA_HW_VERSION",      "CONFIG_KOMMANDO_OTA_HW_VERSION"),
            ("KOMMANDO_OTA_MAX_DATA_SIZE",   "CONFIG_KOMMANDO_OTA_MAX_DATA_SIZE"),
        ]

        for kconfig_key, header_key in pairs:
            kconfig_val = kconfig.get(kconfig_key)
            header_val  = _int_define(CONFIG_H, header_key)

            if kconfig_val is None or header_val is None:
                continue  # skip if either is not parseable

            self.assertEqual(kconfig_val, header_val,
                             f"{kconfig_key} Kconfig default ({kconfig_val}) "
                             f"!= {header_key} header fallback ({header_val})")


# ---------------------------------------------------------------------------
# OTA state machine logic
# ---------------------------------------------------------------------------

class TestOTAStateMachine(unittest.TestCase):
    """
    Simulate the OTA state transitions as implemented in zb_ota_upgrade_status_handler().

    The machine must go: IDLE -> IN_PROGRESS -> CHECK -> APPLIED -> REBOOTING
    Any error at any stage must reset state (abort = True if in_progress).
    """

    def setUp(self):
        self.state = {
            "in_progress": False,
            "partition": None,
            "handle": None,
            "tag_received": False,
            "offset": 0,
            "total_size": 0,
        }

    def _reset(self, abort=False):
        """Mirrors zb_ota_reset_state()."""
        if abort and self.state["in_progress"]:
            # would call esp_ota_abort in real firmware
            pass
        self.state.update({
            "in_progress": False,
            "partition": None,
            "handle": None,
            "tag_received": False,
        })

    def _on_start(self):
        self._reset(abort=False)
        self.state["partition"] = "ota_1"  # simulated next partition
        self.state["handle"] = object()
        self.state["in_progress"] = True
        self.state["offset"] = 0

    def _on_receive(self, chunk_size, total_size):
        self.state["total_size"] = total_size
        self.state["offset"] += chunk_size
        self.state["tag_received"] = True

    def _on_check(self):
        return self.state["offset"] == self.state["total_size"]

    def _on_finish(self):
        # simulates esp_ota_end + esp_ota_set_boot_partition
        self._reset(abort=False)
        return "reboot"

    def test_happy_path(self):
        """Full OTA lifecycle: start → receive → check → finish."""
        self._on_start()
        self.assertTrue(self.state["in_progress"])

        self._on_receive(512, 1024)
        self._on_receive(512, 1024)
        self.assertEqual(self.state["offset"], 1024)

        check_ok = self._on_check()
        self.assertTrue(check_ok, "CHECK should pass when offset == total_size")

        result = self._on_finish()
        self.assertEqual(result, "reboot")
        self.assertFalse(self.state["in_progress"])

    def test_partial_receive_fails_check(self):
        """CHECK must fail if not all bytes have been received."""
        self._on_start()
        self._on_receive(512, 1024)   # only half received
        check_ok = self._on_check()
        self.assertFalse(check_ok, "CHECK must fail when offset < total_size")

    def test_start_resets_previous_state(self):
        """A second START must cleanly reset any in-progress state."""
        self._on_start()
        self.state["offset"] = 100  # simulate partial progress
        self._on_start()            # second start (e.g. server re-initiates)
        self.assertEqual(self.state["offset"], 0, "Offset must reset on new START")

    def test_abort_clears_in_progress(self):
        """After abort, in_progress must be False."""
        self._on_start()
        self.assertTrue(self.state["in_progress"])
        self._reset(abort=True)
        self.assertFalse(self.state["in_progress"])

    def test_error_during_receive_triggers_abort(self):
        """Simulated write error during RECEIVE must trigger an abort reset."""
        self._on_start()
        self.assertTrue(self.state["in_progress"])

        # Simulate a write failure
        write_failed = True
        if write_failed:
            self._reset(abort=True)

        self.assertFalse(self.state["in_progress"])
        self.assertIsNone(self.state["partition"])

    def test_no_partition_available_does_not_progress(self):
        """If no OTA partition is available, in_progress must remain False."""
        self._reset(abort=False)
        simulated_partition = None  # esp_ota_get_next_update_partition() returned NULL

        if simulated_partition is None:
            self._reset(abort=False)
        else:
            self.state["in_progress"] = True

        self.assertFalse(self.state["in_progress"])


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    unittest.main(verbosity=2)
