# Project workflow

- The user authorizes committing and pushing completed project changes directly to `main`. Keep this workflow unless a later user instruction changes it.
- Build and run relevant verification before committing. Never force-push or rewrite shared history without explicit authorization.
- Primary target: a universal macOS VST3, Intel x86_64 with deployment target 10.13 for a MacBook Pro 2011, plus arm64 for an M4 development Mac. Preserve both architectures.
- Keep two optional sound sections, each with Select Sound, ON/OFF and a vertical volume fader with 1 dB steps (−60 to 0 dB plus a −∞ mute position), green setting LEDs and a dB display. Both receive the same MIDI; one may remain empty. Persist both file paths and controls, and restore legacy one-sound projects into section 1.
- Keep automatic installation after each successful VST3 build into `~/Library/Audio/Plug-Ins/VST3/Vini Stage.vst3`. Sign and verify the final bundle after generating the VST3 manifest, then copy and verify the installed bundle.
- Preserve the 0.1.1 release fix: note-off must not start an additional release recording. Use the existing voice's fade and sustain behavior.
- Do not commit piano libraries, extracted WAVs, credentials, dependency checkouts or build products. `.npno` test fixtures are supplied separately by the user.
- Clearly distinguish native host tests from real Ableton/High Sierra validation. Velocity thresholds, loops and Nord envelope modelling are not yet exact.

- Preserve the approved red/charcoal hardware layout, no manufacturer text in the header, only four external screws and no screws inside the sound sections. README imagery must be rendered from the actual current editor, not the concept image.
