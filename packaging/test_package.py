#!/usr/bin/env python3
"""Validate a PKG and its migration logic using temporary directories only."""
import plistlib
import subprocess
import sys
import tempfile
from pathlib import Path
import xml.etree.ElementTree as ET


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def fixture(path, version, identifier="audio.vd.npnoplayer"):
    (path / "Contents").mkdir(parents=True)
    with (path / "Contents/Info.plist").open("wb") as stream:
        plistlib.dump({"CFBundleIdentifier": identifier, "CFBundleShortVersionString": version}, stream)


def main(package):
    with tempfile.TemporaryDirectory(prefix="vini-stage-pkg-test-") as folder:
        temp = Path(folder)
        expanded = temp / "expanded"
        run("/usr/sbin/pkgutil", "--expand-full", str(Path(package).resolve()), str(expanded))
        distribution = ET.parse(expanded / "Distribution").getroot()
        assert distribution.find("options").get("hostArchitectures") == "x86_64,arm64"
        assert distribution.find("volume-check/allowed-os-versions/os-version").get("min") == "10.13"
        assert distribution.find("domains").get("enable_localSystem") == "true"
        component = expanded / "ViniStageComponent.pkg"
        metadata = ET.parse(component / "PackageInfo").getroot()
        assert metadata.get("identifier") == "audio.vinizera.vinistage.vst3"
        assert metadata.get("install-location") == "/" and metadata.get("auth") == "root"
        assert metadata.get("relocatable") == "false"
        assert metadata.find("upgrade-bundle/bundle").get("id") == "audio.vd.npnoplayer"
        assert metadata.find("bundle-version/bundle").get("id") == "audio.vd.npnoplayer"
        assert len(metadata.find("relocate")) == 0
        payload = component / "Payload"
        bundle = payload / "Library/Audio/Plug-Ins/VST3/Vini Stage.vst3"
        binary = bundle / "Contents/MacOS/Vini Stage"
        assert set(run("/usr/bin/lipo", "-archs", str(binary)).split()) == {"x86_64", "arm64"}
        commands = run("/usr/bin/otool", "-l", str(binary))
        assert "version 10.13" in commands and "minos 11.0" in commands
        run("/usr/bin/codesign", "--verify", "--deep", "--strict", str(bundle))
        for path in payload.rglob("*"):
            relative = str(path.relative_to(payload))
            if path.is_file():
                assert relative.startswith(("Library/Audio/Plug-Ins/VST3/Vini Stage.vst3/", "Library/Application Support/Vini Stage/"))
                assert path.suffix.lower() not in {".npno", ".wav"}
        with (bundle / "Contents/Info.plist").open("rb") as stream:
            version = plistlib.load(stream)["CFBundleShortVersionString"]
        assert metadata.get("version") == version
        print("PASS Universal payload, OS targets, code signature, fixed destination and upgrade metadata")
        migrate = component / "Scripts/migrate-user-copy.sh"
        for label, old_version, identifier, should_move in [
            ("old", "0.0.0", "audio.vd.npnoplayer", True),
            ("same", version, "audio.vd.npnoplayer", True),
            ("newer", "999.0.0", "audio.vd.npnoplayer", False),
            ("different", "0.0.0", "org.example.other", False),
        ]:
            home = temp / ("home " + label)
            candidate = home / "Library/Audio/Plug-Ins/VST3/Vini Stage.vst3"
            fixture(candidate, old_version, identifier)
            run("/bin/bash", str(migrate), str(home), str(bundle))
            backups = list((home / "Library/Application Support/Vini Stage/Plugin Backups").glob("*/Vini Stage.vst3"))
            assert candidate.exists() != should_move and len(backups) == int(should_move)
            if should_move:
                with (backups[0] / "Contents/Info.plist").open("rb") as stream:
                    assert plistlib.load(stream)["CFBundleShortVersionString"] == old_version
            print(f"PASS user-copy migration: {label}")
        # Offline-volume installs must not touch the currently logged-in user.
        run("/bin/bash", str(component / "Scripts/postinstall"), "test.pkg", "/", str(temp))
        print("PASS offline-volume guard; no real installation performed")


if __name__ == "__main__":
    main(sys.argv[1])
