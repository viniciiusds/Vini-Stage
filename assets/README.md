# Standalone icon

`ViniStage.svg` is the editable design; `ViniStage.png` is the 1024 px master.
The background matches the editor's #bd252a → #79070e gradient.

After changing the master, regenerate the native icon on macOS:

```sh
python3 assets/make-icon.py
```

The committed `ViniStage.icns` uses legacy RGB RLE plus alpha masks for
16/32/48 px, and PNG for large/Retina representations. Do not replace its
small entries with PNG `icp4`: the JUCE-generated entry decoded as colored
noise in Apple's icon decoder. CMake replaces the generated JUCE resource
with this file before bundle assembly/signing.

`packaging/test_package.py` extracts the packaged app icon using `iconutil`
and compares every standard/Retina representation to the master pixels.
