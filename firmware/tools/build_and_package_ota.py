#!/usr/bin/env python3
"""
Kommando OTA build/publish helper
Version: 1.0.0

Builds firmware, generates Zigbee OTA image, and publishes into OTA feed folder.
It gracefully handles duplicate target/version entries and errors.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


def run_step(cmd: list[str], cwd: Path) -> tuple[bool, str]:
    try:
        proc = subprocess.run(
            cmd,
            cwd=str(cwd),
            check=False,
            capture_output=True,
            text=True,
        )
    except Exception as exc:
        return False, f"Failed to start command {' '.join(cmd)}: {exc}"

    if proc.returncode != 0:
        stderr = (proc.stderr or "").strip()
        stdout = (proc.stdout or "").strip()
        details = stderr if stderr else stdout
        return False, f"Command failed ({proc.returncode}): {' '.join(cmd)}\n{details}"

    return True, (proc.stdout or "").strip()


def parse_project_ver(cmake_lists: Path) -> str:
    text = cmake_lists.read_text(encoding="utf-8")
    m = re.search(r'set\(\s*PROJECT_VER\s+"([^"]+)"\s*\)', text)
    if not m:
        raise ValueError("PROJECT_VER not found in CMakeLists.txt")
    return m.group(1)


def parse_sdkconfig_value(sdkconfig: Path, key: str, default: int) -> int:
    if not sdkconfig.exists():
        return default
    for line in sdkconfig.read_text(encoding="utf-8", errors="ignore").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith(f"{key}="):
            raw = line.split("=", 1)[1].strip()
            try:
                return int(raw, 0)
            except ValueError:
                return default
    return default


def file_version_from_semver(ver: str) -> int:
    parts = ver.split(".") if ver.strip() else ["1"]
    major = int(parts[0]) if len(parts) > 0 and parts[0] else 1
    minor = int(parts[1]) if len(parts) > 1 and parts[1] else 0
    patch = int(parts[2]) if len(parts) > 2 and parts[2] else 0
    return (major << 24) | (minor << 16) | (patch << 8)


def sha512_of(path: Path) -> str:
    h = hashlib.sha512()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def load_json(path: Path, fallback: Any) -> Any:
    if not path.exists():
        return fallback
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return fallback


def save_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def build_download_url(file_name: str, base_url: str | None, github_repo: str | None, github_tag: str | None) -> str:
    if github_repo and github_tag:
        repo = github_repo.strip().strip("/")
        tag = github_tag.strip().strip("/")
        return f"https://github.com/{repo}/releases/download/{tag}/{file_name}"

    if base_url:
        return f"{base_url.rstrip('/')}/{file_name}"

    raise ValueError("Either --base-url OR both --github-repo and --github-tag must be provided")


def main() -> int:
    parser = argparse.ArgumentParser(description="Build and publish Kommando OTA artifacts")
    parser.add_argument("--project-dir", default=".", help="Firmware project root")
    parser.add_argument("--build-dir", default="build", help="Build directory (relative to project-dir)")
    parser.add_argument("--feed-dir", default="releases/ota-feed", help="OTA feed directory")
    parser.add_argument("--base-url", default=None, help="Base URL used in index entries (fallback if GitHub release args are not provided)")
    parser.add_argument("--github-repo", default=None, help="GitHub repository in owner/repo format, e.g. muriloneo/kommando")
    parser.add_argument("--github-tag", default=None, help="GitHub release tag name used for release asset URLs")
    parser.add_argument("--configure", action="store_true", help="Run CMake configure before build")
    parser.add_argument("--skip-build", action="store_true", help="Skip firmware build and only package/publish")
    args = parser.parse_args()

    project_dir = Path(args.project_dir).resolve()
    build_dir = (project_dir / args.build_dir).resolve()
    releases_dir = (project_dir / "releases").resolve()
    feed_dir = (project_dir / args.feed_dir).resolve()
    sdkconfig = (project_dir / "sdkconfig").resolve()
    cmake_lists = (project_dir / "CMakeLists.txt").resolve()

    try:
        project_ver = parse_project_ver(cmake_lists)
        file_version = file_version_from_semver(project_ver)
    except Exception as exc:
        print(f"[ERROR] Could not parse project version: {exc}")
        return 2

    manufacturer_code = parse_sdkconfig_value(sdkconfig, "CONFIG_KOMMANDO_OTA_MANUFACTURER_CODE", 0x1234)
    image_type = parse_sdkconfig_value(sdkconfig, "CONFIG_KOMMANDO_OTA_IMAGE_TYPE", 0x1011)
    hw_version = parse_sdkconfig_value(sdkconfig, "CONFIG_KOMMANDO_OTA_HW_VERSION", 0x0101)

    if args.configure:
        ok, out = run_step(
            [
                "cmake",
                "-G",
                "Ninja",
                "-DPYTHON_DEPS_CHECKED=1",
                "-DESP_PLATFORM=1",
                "-B",
                str(build_dir),
                "-S",
                str(project_dir),
                f"-DSDKCONFIG={sdkconfig}",
            ],
            cwd=project_dir,
        )
        if not ok:
            print(f"[ERROR] Configure failed.\n{out}")
            return 3

    if not args.skip_build:
        ok, out = run_step(["cmake", "--build", str(build_dir)], cwd=project_dir)
        if not ok:
            print(f"[ERROR] Firmware build failed.\n{out}")
            return 4

    ok, out = run_step(["cmake", "--build", str(build_dir), "--target", "zigbee_ota_image"], cwd=project_dir)
    if not ok:
        print(f"[ERROR] OTA image generation failed.\n{out}")
        return 5

    source_ota = releases_dir / f"kommando_{project_ver}.ota"
    if not source_ota.exists():
        print(f"[ERROR] Expected OTA file not found: {source_ota}")
        return 6

    feed_dir.mkdir(parents=True, exist_ok=True)
    versioned_name = f"kommando_{project_ver}_0x{file_version:08X}.ota"
    target_ota = feed_dir / versioned_name

    index_path = feed_dir / "index.json"
    release_meta_path = feed_dir / f"{versioned_name}.json"

    index = load_json(index_path, {"version": "1.0.0", "entries": []})

    entries = index.get("entries", []) if isinstance(index, dict) else []

    duplicate = next(
        (
            e
            for e in entries
            if int(e.get("manufacturerCode", -1)) == manufacturer_code
            and int(e.get("imageType", -1)) == image_type
            and int(e.get("fileVersion", -1)) == file_version
        ),
        None,
    )

    if duplicate:
        print(
            "[INFO] Same target/version already present in index "
            f"(manuf=0x{manufacturer_code:04X}, image=0x{image_type:04X}, fileVersion=0x{file_version:08X})."
        )
        print("[INFO] Ignoring duplicate OTA publish request (graceful no-op).")
        return 0

    shutil.copy2(source_ota, target_ota)
    digest = sha512_of(target_ota)

    try:
        download_url = build_download_url(
            file_name=versioned_name,
            base_url=args.base_url,
            github_repo=args.github_repo,
            github_tag=args.github_tag,
        )
    except ValueError as exc:
        print(f"[ERROR] {exc}")
        return 7

    now_utc = dt.datetime.now(dt.UTC).isoformat(timespec="seconds").replace("+00:00", "Z")

    entry = {
        "modelId": "Kommando_Nano",
        "fileName": versioned_name,
        "url": download_url,
        "manufacturerCode": manufacturer_code,
        "imageType": image_type,
        "fileVersion": file_version,
        "hardwareVersion": hw_version,
        "sha512": digest,
        "force": False,
        "createdAt": now_utc,
    }

    entries.append(entry)
    entries.sort(key=lambda e: int(e.get("fileVersion", 0)), reverse=True)

    index = {
        "version": "1.0.0",
        "entries": entries,
        "updatedAt": now_utc,
    }
    save_json(index_path, index)

    save_json(
        release_meta_path,
        {
            "version": "1.0.0",
            "projectVersion": project_ver,
            "fileVersion": file_version,
            "manufacturerCode": manufacturer_code,
            "imageType": image_type,
            "hardwareVersion": hw_version,
            "source": str(source_ota),
            "published": str(target_ota),
            "sha512": digest,
            "createdAt": now_utc,
        },
    )

    print("[OK] OTA published successfully")
    print(f"      Source : {source_ota}")
    print(f"      Target : {target_ota}")
    print(f"      Index  : {index_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
