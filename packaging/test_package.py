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
        assert distribution.find("options").get("customize") == "always"
        choices = {choice.get("id"): choice for choice in distribution.findall("choice")}
        assert set(choices) == {"vst3", "standalone", "resources"}
        for name, identifier in [("vst3", "audio.vinizera.vinistage.vst3"), ("standalone", "audio.vinizera.vinistage.standalone")]:
            choice = choices[name]
            assert choice.get("visible") == "true" and choice.get("enabled") == "true"
            assert choice.get("start_selected") == "true" and choice.get("selected") is None
            assert [p.get("id") for p in choice.findall("pkg-ref")] == [identifier]
        assert choices["resources"].get("visible") == "false"
        assert choices["resources"].get("selected") == "choices.vst3.selected || choices.standalone.selected"
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
                assert relative.startswith("Library/Audio/Plug-Ins/VST3/Vini Stage.vst3/")
                assert path.suffix.lower() not in {".npno", ".wav"}
        with (bundle / "Contents/Info.plist").open("rb") as stream:
            version = plistlib.load(stream)["CFBundleShortVersionString"]
        assert metadata.get("version") == version
        app_component = expanded / "ViniStageStandalone.pkg"
        app_metadata = ET.parse(app_component / "PackageInfo").getroot()
        assert app_metadata.get("identifier") == "audio.vinizera.vinistage.standalone"
        assert app_metadata.get("install-location") == "/" and app_metadata.get("auth") == "root"
        assert app_metadata.get("version") == version and app_metadata.get("relocatable") == "false"
        assert app_metadata.find("upgrade-bundle/bundle").get("id") == "audio.vd.npnoplayer"
        assert app_metadata.find("bundle-version/bundle").get("id") == "audio.vd.npnoplayer"
        assert len(app_metadata.find("relocate")) == 0
        assert app_metadata.find("scripts") is None  # Standalone-only never migrates a VST3.
        app_payload = app_component / "Payload"
        app = app_payload / "Applications/Vini Stage.app"
        app_binary = app / "Contents/MacOS/Vini Stage"
        assert set(run("/usr/bin/lipo", "-archs", str(app_binary)).split()) == {"x86_64", "arm64"}
        commands = run("/usr/bin/otool", "-l", str(app_binary))
        assert "version 10.13" in commands and "minos 11.0" in commands
        run("/usr/bin/codesign", "--verify", "--deep", "--strict", str(app))
        with (app / "Contents/Info.plist").open("rb") as stream:
            app_info = plistlib.load(stream)
        assert app_info["CFBundleShortVersionString"] == version
        assert app_info["CFBundleIdentifier"] == "audio.vd.npnoplayer"
        for path in app_payload.rglob("*"):
            if path.is_file():
                assert str(path.relative_to(app_payload)).startswith("Applications/Vini Stage.app/")
                assert path.suffix.lower() not in {".npno", ".wav"}
        shared = expanded / "ViniStageResources.pkg"
        shared_metadata = ET.parse(shared / "PackageInfo").getroot()
        assert shared_metadata.get("identifier") == "audio.vinizera.vinistage.resources"
        assert shared_metadata.get("version") == version and shared_metadata.get("install-location") == "/"
        assert shared_metadata.find("scripts") is None
        for path in (shared / "Payload").rglob("*"):
            if path.is_file():
                assert str(path.relative_to(shared / "Payload")).startswith("Library/Application Support/Vini Stage/")
                assert path.suffix.lower() not in {".npno", ".wav"}
        assert (shared / "Payload/Library/Application Support/Vini Stage/Licenses/AGPL-3.0.txt").is_file()
        assert (shared / "Payload/Library/Application Support/Vini Stage/SOURCE.txt").is_file()
        expected_refs = {"audio.vinizera.vinistage.vst3": "ViniStageComponent.pkg",
                         "audio.vinizera.vinistage.standalone": "ViniStageStandalone.pkg",
                         "audio.vinizera.vinistage.resources": "ViniStageResources.pkg"}
        assert {ref.get("id"): ref.text.strip().lstrip("#") for ref in distribution.findall("pkg-ref") if ref.get("version")} == expected_refs
        assert all(ref.get("version") == version for ref in distribution.findall("pkg-ref") if ref.get("version"))
        for vst_selected, app_selected in [(True, True), (True, False), (False, True), (False, False)]:
            # Verify the choice graph and payload isolation. This does not invoke a real installation.
            selected_ids = (["audio.vinizera.vinistage.vst3"] if vst_selected else []) + (["audio.vinizera.vinistage.standalone"] if app_selected else [])
            if vst_selected or app_selected:
                selected_ids.append("audio.vinizera.vinistage.resources")
            files = []
            for identifier in selected_ids:
                root = expanded / expected_refs[identifier] / "Payload"
                files.extend(str(path.relative_to(root)) for path in root.rglob("*") if path.is_file())
            assert any(path.startswith("Applications/") for path in files) == app_selected
            assert any(path.startswith("Library/Audio/") for path in files) == vst_selected
            assert any(path.startswith("Library/Application Support/") for path in files) == (vst_selected or app_selected)
            print(f"PASS isolated selection graph: VST3={vst_selected}, Standalone={app_selected}")
        print("PASS two Universal bundles, independent receipts/destinations/upgrade metadata, signatures and shared licenses")
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
