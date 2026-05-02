#!/usr/bin/env python3
"""
Create a standard Zigbee OTA (.ota) image from a compiled ESP firmware binary.

This tool mirrors Espressif's `tools/image_builder_tool/image_builder_tool.py`
approach and uses zigpy OTA serializers.
"""

from __future__ import annotations

import argparse
import functools
from pathlib import Path

import zigpy.ota.image
import zigpy.types


def create_image(
    output: Path,
    version: int,
    manuf_id: int,
    image_type: int,
    stack_version: int,
    header_string: str,
    tag_id: int,
    tag_file: Path,
    security_credentials: int | None = None,
    upgrade_dest: str | None = None,
    min_hw_ver: int | None = None,
    max_hw_ver: int | None = None,
) -> None:
    payload = tag_file.read_bytes()

    header = zigpy.ota.image.OTAImageHeader(
        upgrade_file_id=zigpy.ota.image.OTAImageHeader.MAGIC_VALUE,
        header_version=0x0100,
        header_length=0,
        field_control=zigpy.ota.image.FieldControl(0),
        manufacturer_id=manuf_id,
        image_type=image_type,
        file_version=version,
        stack_version=stack_version,
        header_string=header_string[:32],
        image_size=0,
    )

    if security_credentials is not None:
        header.field_control |= zigpy.ota.image.FieldControl.SECURITY_CREDENTIAL_VERSION_PRESENT
        header.security_credential_version = security_credentials

    if upgrade_dest is not None:
        header.field_control |= zigpy.ota.image.FieldControl.DEVICE_SPECIFIC_FILE_PRESENT
        header.upgrade_file_destination = zigpy.types.EUI64.convert(upgrade_dest)

    if min_hw_ver is not None and max_hw_ver is not None:
        header.field_control |= zigpy.ota.image.FieldControl.HARDWARE_VERSIONS_PRESENT
        header.minimum_hardware_version = min_hw_ver
        header.maximum_hardware_version = max_hw_ver

    image = zigpy.ota.image.OTAImage(
        header=header,
        subelements=[
            zigpy.ota.image.SubElement(
                tag_id=tag_id,
                data=payload,
            )
        ],
    )

    image.header.header_length = len(image.header.serialize())
    image.header.image_size = image.header.header_length + len(image.subelements.serialize())

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(image.serialize())


def main() -> None:
    any_int = functools.wraps(int)(functools.partial(int, base=0))

    parser = argparse.ArgumentParser(description="Create Zigbee OTA image")
    parser.add_argument("-c", "--create", required=True, type=Path, help="Output OTA file path")
    parser.add_argument("-v", "--version", required=True, type=any_int, help="Firmware file version")
    parser.add_argument("-m", "--manuf-id", required=True, type=any_int, help="Manufacturer code")
    parser.add_argument("-i", "--image-type", required=True, type=any_int, help="Image type")
    parser.add_argument("-s", "--stack-version", default=0x0002, type=any_int, help="Stack version (default: 0x0002)")
    parser.add_argument("--header_string", default="", help="OTA header string (max 32 chars)")
    parser.add_argument("--security-credentials", type=any_int, help="Optional security credential version")
    parser.add_argument("--upgrade-dest", help="Optional destination EUI64")
    parser.add_argument("--min-hw-ver", type=any_int, help="Optional min HW version")
    parser.add_argument("--max-hw-ver", type=any_int, help="Optional max HW version")
    parser.add_argument("-t", "--tag-id", default=0, type=any_int, help="Sub-element tag id (default: 0)")
    parser.add_argument("-f", "--tag-file", required=True, type=Path, help="Input firmware binary (.bin)")

    args = parser.parse_args()

    create_image(
        output=args.create,
        version=args.version,
        manuf_id=args.manuf_id,
        image_type=args.image_type,
        stack_version=args.stack_version,
        header_string=args.header_string,
        tag_id=args.tag_id,
        tag_file=args.tag_file,
        security_credentials=args.security_credentials,
        upgrade_dest=args.upgrade_dest,
        min_hw_ver=args.min_hw_ver,
        max_hw_ver=args.max_hw_ver,
    )


if __name__ == "__main__":
    main()
