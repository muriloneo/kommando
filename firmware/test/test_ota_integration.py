#!/usr/bin/env python3
"""
Integration test for the complete OTA pipeline

Tests:
  - GitHub Releases URL mode end-to-end
  - Self-hosted URL mode end-to-end
  - Duplicate version handling
  - Index.json correctness
  - Pre/post-publish state
"""

import json
import shutil
import tempfile
from pathlib import Path

import sys
sys.path.insert(0, str(Path(__file__).parent.parent / "tools"))

# Mock imports to avoid needing actual firmware build
import subprocess
from unittest.mock import patch, MagicMock

def test_github_releases_flow():
    """Test full GitHub Releases OTA publish flow."""
    print("\n[TEST] GitHub Releases OTA Flow")
    
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)
        project_dir = tmpdir / "project"
        project_dir.mkdir()
        
        # Setup minimal project structure
        (project_dir / "CMakeLists.txt").write_text('set( PROJECT_VER "1.0.2" )')
        (project_dir / "sdkconfig").write_text(
            "CONFIG_KOMMANDO_OTA_MANUFACTURER_CODE=0x1234\n"
            "CONFIG_KOMMANDO_OTA_IMAGE_TYPE=0x1011\n"
            "CONFIG_KOMMANDO_OTA_HW_VERSION=0x0101\n"
        )
        
        # Create fake OTA file
        releases_dir = project_dir / "releases"
        releases_dir.mkdir()
        fake_ota = releases_dir / "kommando_1.0.2.ota"
        fake_ota.write_bytes(b"FAKE_OTA_CONTENT" * 1000)  # ~15KB
        
        build_dir = project_dir / "build"
        build_dir.mkdir()
        
        # Import and run OTA packaging
        from build_and_package_ota import main
        
        # Patch subprocess.run to skip actual build
        def mock_run(cmd, **kwargs):
            result = MagicMock()
            result.returncode = 0
            result.stdout = ""
            result.stderr = ""
            return result
        
        # Patch sys.argv to simulate CLI call
        with patch.object(sys, 'argv', [
            'build_and_package_ota.py',
            '--project-dir', str(project_dir),
            '--skip-build',
            '--github-repo', 'muriloneo/kommando',
            '--github-tag', 'v1.0.2',
        ]):
            with patch('subprocess.run', side_effect=mock_run):
                result = main()
        
        assert result == 0, f"OTA packaging failed with code {result}"
        
        # Verify artifacts
        feed_dir = project_dir / "releases" / "ota-feed"
        assert feed_dir.exists(), "OTA feed directory not created"
        
        # Check versioned OTA exists
        versioned_ota = feed_dir / "kommando_1.0.2_0x01000200.ota"
        assert versioned_ota.exists(), f"Versioned OTA not found: {versioned_ota}"
        
        # Check index.json
        index_path = feed_dir / "index.json"
        assert index_path.exists(), "index.json not created"
        
        index = json.loads(index_path.read_text())
        assert index["version"] == "1.0.0"
        assert len(index["entries"]) == 1
        
        entry = index["entries"][0]
        assert entry["modelId"] == "Kommando_Nano"
        assert entry["fileName"] == "kommando_1.0.2_0x01000200.ota"
        assert entry["manufacturerCode"] == 0x1234
        assert entry["imageType"] == 0x1011
        assert entry["fileVersion"] == 0x01000200
        assert entry["hardwareVersion"] == 0x0101
        
        # Verify GitHub Releases URL
        assert "github.com/muriloneo/kommando/releases/download/v1.0.2/" in entry["url"]
        assert "kommando_1.0.2_0x01000200.ota" in entry["url"]
        
        # Check metadata file
        meta_path = feed_dir / "kommando_1.0.2_0x01000200.ota.json"
        assert meta_path.exists(), "Metadata JSON not created"
        
        meta = json.loads(meta_path.read_text())
        assert meta["projectVersion"] == "1.0.2"
        assert meta["fileVersion"] == 0x01000200
        
        print("[PASS] GitHub Releases OTA flow works correctly")
        print(f"  - Versioned OTA: {versioned_ota.name}")
        print(f"  - Index URL: {entry['url']}")
        print(f"  - SHA512: {entry['sha512'][:16]}...")


def test_self_hosted_flow():
    """Test self-hosted URL mode."""
    print("\n[TEST] Self-Hosted URL Flow")
    
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)
        project_dir = tmpdir / "project"
        project_dir.mkdir()
        
        # Setup minimal project structure
        (project_dir / "CMakeLists.txt").write_text('set( PROJECT_VER "2.0.0" )')
        (project_dir / "sdkconfig").write_text(
            "CONFIG_KOMMANDO_OTA_MANUFACTURER_CODE=0x1234\n"
            "CONFIG_KOMMANDO_OTA_IMAGE_TYPE=0x1011\n"
        )
        
        releases_dir = project_dir / "releases"
        releases_dir.mkdir()
        fake_ota = releases_dir / "kommando_2.0.0.ota"
        fake_ota.write_bytes(b"FAKE_OTA_CONTENT" * 1000)
        
        build_dir = project_dir / "build"
        build_dir.mkdir()
        
        from build_and_package_ota import main
        
        def mock_run(cmd, **kwargs):
            result = MagicMock()
            result.returncode = 0
            result.stdout = ""
            result.stderr = ""
            return result
        
        with patch.object(sys, 'argv', [
            'build_and_package_ota.py',
            '--project-dir', str(project_dir),
            '--skip-build',
            '--base-url', 'https://my-server.com/ota',
        ]):
            with patch('subprocess.run', side_effect=mock_run):
                result = main()
        
        assert result == 0
        
        feed_dir = project_dir / "releases" / "ota-feed"
        index = json.loads((feed_dir / "index.json").read_text())
        entry = index["entries"][0]
        
        # Verify self-hosted URL
        assert entry["url"] == "https://my-server.com/ota/kommando_2.0.0_0x02000000.ota"
        
        print("[PASS] Self-hosted URL flow works correctly")
        print(f"  - URL: {entry['url']}")


def test_duplicate_version_ignored():
    """Test that duplicate target/version is ignored gracefully."""
    print("\n[TEST] Duplicate Version Handling")
    
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)
        project_dir = tmpdir / "project"
        project_dir.mkdir()
        
        # Setup
        (project_dir / "CMakeLists.txt").write_text('set( PROJECT_VER "1.0.0" )')
        (project_dir / "sdkconfig").write_text(
            "CONFIG_KOMMANDO_OTA_MANUFACTURER_CODE=0x1234\n"
            "CONFIG_KOMMANDO_OTA_IMAGE_TYPE=0x1011\n"
        )
        
        releases_dir = project_dir / "releases"
        releases_dir.mkdir()
        fake_ota = releases_dir / "kommando_1.0.0.ota"
        fake_ota.write_bytes(b"FAKE_OTA_CONTENT" * 1000)
        
        (project_dir / "build").mkdir()
        
        from build_and_package_ota import main
        
        def mock_run(cmd, **kwargs):
            result = MagicMock()
            result.returncode = 0
            result.stdout = ""
            result.stderr = ""
            return result
        
        # First publish
        with patch.object(sys, 'argv', [
            'build_and_package_ota.py',
            '--project-dir', str(project_dir),
            '--skip-build',
            '--github-repo', 'test/repo',
            '--github-tag', 'v1.0.0',
        ]):
            with patch('subprocess.run', side_effect=mock_run):
                result1 = main()
        
        assert result1 == 0
        
        # Capture first publish state
        feed_dir = project_dir / "releases" / "ota-feed"
        index1 = json.loads((feed_dir / "index.json").read_text())
        entry_count_after_first = len(index1["entries"])
        
        # Second publish (same version)
        with patch.object(sys, 'argv', [
            'build_and_package_ota.py',
            '--project-dir', str(project_dir),
            '--skip-build',
            '--github-repo', 'test/repo',
            '--github-tag', 'v1.0.1',  # Different tag, but same target/version
        ]):
            with patch('subprocess.run', side_effect=mock_run):
                result2 = main()
        
        # Should return 0 (graceful no-op)
        assert result2 == 0, "Duplicate publish should exit with 0"
        
        # Entry count should remain the same
        index2 = json.loads((feed_dir / "index.json").read_text())
        entry_count_after_second = len(index2["entries"])
        
        assert entry_count_after_first == entry_count_after_second, \
            "Duplicate version should not create new entry"
        
        print("[PASS] Duplicate version correctly ignored")
        print(f"  - Entry count remained: {entry_count_after_second}")


def test_multiple_versions_in_feed():
    """Test that multiple versions accumulate correctly."""
    print("\n[TEST] Multiple Versions in Feed")
    
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)
        project_dir = tmpdir / "project"
        project_dir.mkdir()
        
        releases_dir = project_dir / "releases"
        releases_dir.mkdir()
        (project_dir / "build").mkdir()
        
        from build_and_package_ota import main
        
        def mock_run(cmd, **kwargs):
            result = MagicMock()
            result.returncode = 0
            result.stdout = ""
            result.stderr = ""
            return result
        
        versions = ["1.0.0", "1.1.0", "2.0.0"]
        
        for ver in versions:
            # Setup for each version
            (project_dir / "CMakeLists.txt").write_text(f'set( PROJECT_VER "{ver}" )')
            (project_dir / "sdkconfig").write_text(
                "CONFIG_KOMMANDO_OTA_MANUFACTURER_CODE=0x1234\n"
                "CONFIG_KOMMANDO_OTA_IMAGE_TYPE=0x1011\n"
            )
            
            # Create versioned OTA file
            fake_ota = releases_dir / f"kommando_{ver}.ota"
            fake_ota.write_bytes(f"VERSION_{ver}".encode() * 1000)
            
            with patch.object(sys, 'argv', [
                'build_and_package_ota.py',
                '--project-dir', str(project_dir),
                '--skip-build',
                '--github-repo', 'test/repo',
                '--github-tag', f'v{ver}',
            ]):
                with patch('subprocess.run', side_effect=mock_run):
                    result = main()
            
            assert result == 0
        
        # Verify all versions in feed
        feed_dir = project_dir / "releases" / "ota-feed"
        index = json.loads((feed_dir / "index.json").read_text())
        
        assert len(index["entries"]) == 3, f"Expected 3 entries, got {len(index['entries'])}"
        
        # Entries should be sorted by fileVersion (descending)
        file_versions = [e["fileVersion"] for e in index["entries"]]
        assert file_versions == sorted(file_versions, reverse=True), "Entries not sorted descending"
        
        print("[PASS] Multiple versions accumulated correctly")
        print(f"  - Versions in feed: {versions}")
        print(f"  - Sorted by fileVersion (descending): {[f'0x{fv:08X}' for fv in file_versions]}")


if __name__ == "__main__":
    print("=" * 70)
    print("KOMMANDO OTA Pipeline - Integration Tests")
    print("=" * 70)
    
    try:
        test_github_releases_flow()
        test_self_hosted_flow()
        test_duplicate_version_ignored()
        test_multiple_versions_in_feed()
        
        print("\n" + "=" * 70)
        print("✓ ALL INTEGRATION TESTS PASSED")
        print("=" * 70)
        
    except AssertionError as e:
        print(f"\n✗ TEST FAILED: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\n✗ ERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
