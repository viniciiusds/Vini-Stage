#!/bin/bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
if command -v cmake >/dev/null 2>&1; then
    cmake_bin="$(command -v cmake)"
else
    cmake_bin="$project_dir/.build-tools/bin/cmake"
    if [[ ! -x "$cmake_bin" ]]; then
        python3 -m venv "$project_dir/.build-tools"
        "$project_dir/.build-tools/bin/pip" install cmake
    fi
fi
juce_source="${JUCE_PATH:-$project_dir/deps/JUCE}"
if [[ ! -d "$juce_source/modules" ]]; then
    mkdir -p "$(dirname -- "$juce_source")"
    git clone --depth 1 --branch 8.0.12 https://github.com/juce-framework/JUCE.git "$juce_source"
fi
"$HOME/.cargo/bin/rustup" target add x86_64-apple-darwin aarch64-apple-darwin
"$HOME/.cargo/bin/cargo" fetch --manifest-path "$project_dir/bridge/Cargo.toml" --locked
"$cmake_bin" -S "$project_dir" -B "$project_dir/build" -DJUCE_PATH="$juce_source" \
    -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64' -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
"$cmake_bin" --build "$project_dir/build" --config Release --target NpnoPlayer_VST3 NpnoPlayer_Standalone -j 4
codesign --force --deep --sign - "$project_dir/build/NpnoPlayer_artefacts/Release/Standalone/Vini Stage.app"
