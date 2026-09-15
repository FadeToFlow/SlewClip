# SlewClip

**SlewClip** is a JUCE-based audio plugin that emulates the behavior of a current-limited output driving a capacitive load in analog circuits. It recreates the dynamic clipping and slew-rate limiting that occurs when an amplifier cannot charge a capacitor quickly enough, resulting in a distinctive, musical distortion.

## Overview

In analog electronics, when an output stage is current-limited and drives a capacitive load, the maximum voltage slew rate is constrained by `dV/dt = I / C`. If the demanded signal tries to change faster than this limit, the output voltage ramps linearly instead of following the input. **SlewClip** models this phenomenon algorithmically:

- The clipping threshold acts as the capacitor voltage.
- The **Slew Rate** parameter defines `I / C` — how fast the threshold can rise.
- When the input exceeds the current threshold, the output is held at that threshold, which then increases at the slew rate.
- When the input falls below the threshold, the threshold resets or tracks the input, simulating capacitor discharge.

This produces a unique form of dynamic waveshaping that is both aggressive and smooth, useful for saturation, limiting, and creative distortion.

## Features

- Dynamic threshold clipping inspired by current-limited capacitive loads.
- Adjustable slew rate (0–1000 per second) to control recovery speed.
- Drive and bias controls for pre-gain and DC offset.
- Up to 16x oversampling to reduce aliasing.
- Reports latency to the DAW for automatic delay compensation.

## Parameters

| Parameter      | ID       | Range                     | Default | Description |
|----------------|----------|---------------------------|---------|-------------|
| Oversampling   | `OS`     | 1x (Off), 2x, 4x, 8x, 16x | 1x      | Internal oversampling factor to reduce aliasing. |
| Drive          | `DRIVE`  | 0.0 – 40.0 dB             | 0.0 dB  | Input gain before clipping. |
| Bias           | `BIAS`   | -2.5 – 2.5                | 0.0     | DC offset added to the signal. |
| Threshold      | `THRESH` | 0.01 – 1.0                | 0.5     | Base clipping threshold (capacitor voltage when discharged). |
| Slew Rate      | `SLEW`   | 0.0 – 1000.0 /s           | 10.0    | Maximum rate of change of the threshold (`I / C`). |

## How It Works

For each sample in each channel:

1. Apply drive and bias:  
   `x = input * driveGain + bias`

2. If `|x| > dynamicThreshold`:  
   - Output is clipped to `dynamicThreshold * sign(x)`.  
   - The threshold increases by `slewRate / sampleRate`.

3. Otherwise:  
   - Output is `x`.  
   - If `|x| < baseThreshold`, the dynamic threshold resets to `baseThreshold`.  
   - Else, the dynamic threshold becomes `|x|`.

This models a capacitor being charged by a current-limited source. The threshold represents the capacitor voltage, and the slew rate sets how quickly it can charge. Oversampling is applied before processing and downsampled afterward, with latency reported to the host.

## Technical Details

- **Framework:** JUCE
- **Plugin formats:** VST3, AU, Standalone (depending on build configuration)
- **Parameter management:** `AudioProcessorValueTreeState`
- **State persistence:** XML via `getStateInformation` / `setStateInformation`
- **Oversampling:** Polyphase IIR half-band filters, factors 2x–16x

## Building

### Requirements

#### Common
- **CMake** ≥ 3.22
- **Git** (used by `FetchContent` to download JUCE)
- A C++17-compatible compiler

#### Windows
- Visual Studio 2022 with the **Desktop development with C++** workload
- Or MinGW-w64 (for cross-compilation from Linux)

#### macOS
- Xcode 14+ and Command Line Tools:
  ```bash
  xcode-select --install
  ```

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y \
    build-essential cmake git pkg-config \
    libasound2-dev libjack-jackd2-dev \
    libfreetype6-dev libx11-dev libxrandr-dev \
    libxinerama-dev libxcursor-dev \
    libcurl4-openssl-dev libwebkit2gtk-4.1-dev
```
### Platform-specific builds

#### Linux

**Build:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

**Install into user plugin directories:**
```bash
mkdir -p ~/.vst3 ~/.lv2
cp -r build/SlewClip_artefacts/Release/VST3/SlewClip.vst3 ~/.vst3/
cp -r build/SlewClip_artefacts/Release/LV2/SlewClip.lv2   ~/.lv2/
```

**Standalone:**
```bash
./build/SlewClip_artefacts/Release/Standalone/SlewClip
```

---

#### macOS

**Build (Xcode project):**
```bash
cmake -B build -G Xcode
cmake --build build --config Release
```

**Or via Makefile/Ninja (faster):**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

**Install:**
```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3
mkdir -p ~/Library/Audio/Plug-Ins/Components
cp -r build/SlewClip_artefacts/Release/VST3/SlewClip.vst3       ~/Library/Audio/Plug-Ins/VST3/
cp -r build/SlewClip_artefacts/Release/AU/SlewClip.component    ~/Library/Audio/Plug-Ins/Components/
```
#### Windows

**Build (PowerShell / cmd):**
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

**Install:**
```powershell
Copy-Item -Recurse build\SlewClip_artefacts\Release\VST3\SlewClip.vst3 `
          "C:\Program Files\Common Files\VST3\"
```

## License

AGPL-3.0-only
This project is licensed under the GNU Affero General Public License v3.0.
See LICENSE for details.
