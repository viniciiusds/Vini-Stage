# Project workflow

- The user authorizes committing and pushing completed project changes directly to `main`. Keep this workflow unless a later user instruction changes it.
- Build and run relevant verification before committing. Never force-push or rewrite shared history without explicit authorization.
- Primary target: a universal macOS VST3, Intel x86_64 with deployment target 10.13 for a MacBook Pro 2011, plus arm64 for an M4 development Mac. Preserve both architectures.
- Keep two optional sounds A/B in a single Sounds LCD on the red right side. Shared grey volume area on the left, each fader with green setting LEDs on its right and a dB display. Select A/B becomes Clear A/B when a file is selected; clear removes/cancels only that slot and persists an empty path. Empty rows show only A: / B:. No ON/OFF buttons or visual keyboard. Preserve legacy ON parameters and IDs for host automation/state compatibility. Selecting a new file re-enables its legacy ON parameter. Keep 1 dB steps (−60 to 0 dB plus −∞), both paths and controls, and restore legacy one-sound projects into A.
- Keep automatic installation after each successful VST3 build into `~/Library/Audio/Plug-Ins/VST3/Vini Stage.vst3`. Sign and verify the final bundle after generating the VST3 manifest, then copy and verify the installed bundle.
- Preserve the 0.1.1 release fix: note-off must not start an additional release recording. Use the existing voice's fade and sustain behavior.
- Do not commit piano libraries, extracted WAVs, credentials, dependency checkouts or build products. `.npno` test fixtures are supplied separately by the user.
- Clearly distinguish native host tests from real Ableton/High Sierra validation. Velocity thresholds, loops and Nord envelope modelling are not yet exact.

- Preserve the approved red/charcoal hardware layout, no manufacturer text in the header, only four external screws and no screws inside the sound sections. README imagery must be rendered from the actual current editor, not the concept image.

- Display actual file-read/decode progress as Loading... X% in the corresponding A/B row of the shared Sounds LCD. Do not replace it with timer-driven progress. Align the Select/Clear buttons under the shared LCD and the LED column with the fader track; keep the dB LCD as wide as the fader/LED group and no visible Volume heading.
- Keep at least 20 px of clear space between the bottom of the dB LCD and the fader cap at maximum (0 dB); keep the LED column aligned to the shortened fader track.

- `package.sh` produces a legacy-compressed Universal PKG targeting macOS 10.13+, installing globally in `/Library/Audio/Plug-Ins/VST3/`. Keep its stable package identifier, non-relocatable/version-checked atomic bundle upgrade and safe per-user backup migration. Do not claim Developer ID signing, notarization or real High Sierra installation unless actually verified. Package tests must use isolated temporary roots and never run a real system installation without the user's explicit request.
