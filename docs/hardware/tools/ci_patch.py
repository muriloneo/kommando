#!/usr/bin/env python
# Rewrite .github/workflows/ota-release.yml so every branch push publishes its
# own release tag (<branch-slug>-<run_number>-<short-sha>). main keeps
# make_latest; every other branch is published as a prerelease.
import sys, io, os

P = "C:/codes/esp32/kommando/.github/workflows/ota-release.yml"

with open(P, "rb") as f:
    raw = f.read()

crlf = b"\r\n" in raw
s = raw.replace(b"\r\n", b"\n").decode("utf-8")
orig = s

def rep(name, old, new, count=1):
    global s
    n = s.count(old)
    if n != count:
        print("FAIL %s: expected %d occurrence(s), found %d" % (name, count, n))
        sys.exit(1)
    s = s.replace(old, new)
    print("ok   %s" % name)

# --- R1: firmware-ci job outputs + branch-slug step -------------------------
rep("R1 firmware-ci slug step + outputs", """    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Clean build artifacts and managed components
        run: rm -rf firmware/build/ firmware/managed_components/

      - name: Build firmware and OTA image (ESP-IDF)
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v6.0
          target: esp32c6
          path: firmware
          command: idf.py build
""", """    outputs:
      branch_slug: ${{ steps.ref.outputs.slug }}
      branch_name: ${{ steps.ref.outputs.name }}

    steps:
      - name: Resolve branch slug
        id: ref
        run: |
          NAME="${GITHUB_HEAD_REF:-${GITHUB_REF_NAME}}"
          SLUG=$(printf '%s' "$NAME" | tr '/ ' '--' | tr -cd 'A-Za-z0-9._-')
          if [ -z "$SLUG" ]; then SLUG="branch"; fi
          echo "name=$NAME" >> "$GITHUB_OUTPUT"
          echo "slug=$SLUG" >> "$GITHUB_OUTPUT"

      - name: Checkout
        uses: actions/checkout@v4

      - name: Clean build artifacts and managed components
        run: rm -rf firmware/build/ firmware/managed_components/

      - name: Build firmware and OTA image (ESP-IDF)
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v6.0
          target: esp32c6
          path: firmware
          command: idf.py build
""")

# --- R2: artifact upload -> every branch, branch-scoped name ----------------
rep("R2 render/grep of 'Resolve branch slug' collisions", "this-must-not-exist", "x", 0)

rep("R2 artifact upload for any branch", """      - name: Upload latest flashable firmware artifacts
        if: github.event_name == 'push' && github.ref == 'refs/heads/main'
        uses: actions/upload-artifact@v4
        with:
          name: kommando-firmware-main-${{ github.run_number }}
""", """      - name: Upload flashable firmware artifacts
        if: github.event_name == 'push' && !startsWith(github.ref, 'refs/tags/')
        uses: actions/upload-artifact@v4
        with:
          name: kommando-firmware-${{ steps.ref.outputs.slug }}-${{ github.run_number }}
""")

# --- R3: release job -> any branch push -------------------------------------
rep("R3 release job on any branch push", """  auto-release-main:
    if: github.event_name == 'push' && github.ref == 'refs/heads/main'
    needs: firmware-ci
    runs-on: ubuntu-latest

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Download latest flashable firmware artifacts
        uses: actions/download-artifact@v4
        with:
          name: kommando-firmware-main-${{ github.run_number }}
          path: latest-main-artifacts
""", """  auto-release-branch:
    if: github.event_name == 'push' && !startsWith(github.ref, 'refs/tags/')
    needs: firmware-ci
    runs-on: ubuntu-latest

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Download flashable firmware artifacts
        uses: actions/download-artifact@v4
        with:
          name: kommando-firmware-${{ needs.firmware-ci.outputs.branch_slug }}-${{ github.run_number }}
          path: latest-branch-artifacts
""")

# --- R4: rename the scratch artifact dir everywhere -------------------------
rep("R4 rename artifact dir", "latest-main-artifacts", "latest-branch-artifacts", 2)

# --- R5: compare against the previous release OF THIS BRANCH ----------------
rep("R5 previous release of same branch", """      - name: Resolve previous release for comparison
        id: prev_release
        uses: actions/github-script@v7
        with:
          script: |
            const { owner, repo } = context.repo;
            const releases = await github.paginate(github.rest.repos.listReleases, {
              owner,
              repo,
              per_page: 30,
            });

            const previous = releases.find(r => !r.draft && !r.prerelease);
""", """      - name: Resolve previous release for comparison
        id: prev_release
        uses: actions/github-script@v7
        env:
          BRANCH_SLUG: ${{ needs.firmware-ci.outputs.branch_slug }}
        with:
          script: |
            const { owner, repo } = context.repo;
            const slug = process.env.BRANCH_SLUG;
            const releases = await github.paginate(github.rest.repos.listReleases, {
              owner,
              repo,
              per_page: 100,
            });

            const previous = releases.find(
              r => !r.draft && (r.tag_name || '').startsWith(`${slug}-`)
            );
""")

# --- R6: branch-scoped tag + release name ----------------------------------
rep("R6 branch-scoped tag name", """      - name: Prepare auto-release metadata and bundle
        id: release_meta
        run: |
          SHORT_SHA="${GITHUB_SHA::7}"
          TAG_NAME="main-${GITHUB_RUN_NUMBER}-${SHORT_SHA}"
          RELEASE_NAME="Main Firmware Build #${GITHUB_RUN_NUMBER} (${SHORT_SHA})"
""", """      - name: Prepare auto-release metadata and bundle
        id: release_meta
        env:
          BRANCH_SLUG: ${{ needs.firmware-ci.outputs.branch_slug }}
          BRANCH_NAME: ${{ needs.firmware-ci.outputs.branch_name }}
        run: |
          SHORT_SHA="${GITHUB_SHA::7}"
          TAG_NAME="${BRANCH_SLUG}-${GITHUB_RUN_NUMBER}-${SHORT_SHA}"
          RELEASE_NAME="${BRANCH_NAME} Firmware Build #${GITHUB_RUN_NUMBER} (${SHORT_SHA})"
""")

# --- R7: release body wording is no longer main-only ------------------------
rep("R7 release body wording", "          Automated release for the latest successful `main` build, intended for ESPHome Web Flasher/manual flash use.",
    "          Automated release for the latest successful build of branch __BRANCH__, intended for ESPHome Web Flasher/manual flash use.")

# --- R8: substitute __BRANCH__ ---------------------------------------------
rep("R8 __BRANCH__ substitution", '          sed -i "s#__PREV_TAG__#${PREV_TAG:-none}#g" release-body.md',
    '          sed -i "s#__BRANCH__#${BRANCH_NAME}#g" release-body.md\n          sed -i "s#__PREV_TAG__#${PREV_TAG:-none}#g" release-body.md')

# --- R9: publish as prerelease unless this is main --------------------------
rep("R9 publish prerelease/make_latest", """      - name: Publish automatic main release
        uses: softprops/action-gh-release@v2
        with:
          tag_name: ${{ steps.release_meta.outputs.tag }}
          target_commitish: ${{ github.sha }}
          name: ${{ steps.release_meta.outputs.name }}
          prerelease: false
          make_latest: true
""", """      - name: Publish automatic branch release
        uses: softprops/action-gh-release@v2
        with:
          tag_name: ${{ steps.release_meta.outputs.tag }}
          target_commitish: ${{ github.sha }}
          name: ${{ steps.release_meta.outputs.name }}
          prerelease: ${{ github.ref != 'refs/heads/main' }}
          make_latest: ${{ github.ref == 'refs/heads/main' }}
""")

if s == orig:
    print("FAIL: nothing changed")
    sys.exit(1)

out = s.replace("\n", "\r\n") if crlf else s
with open(P, "wb") as f:
    f.write(out.encode("utf-8"))
print("line endings kept: %s" % ("CRLF" if crlf else "LF"))
print("written: %d bytes (was %d)" % (len(out), len(raw)))
