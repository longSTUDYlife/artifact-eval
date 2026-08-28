# Firmware (optional)

This folder is **not** used to regenerate the paper figures. Docker
and `regenerate_all.py` do not read it. It is the host-side capture
path (radino32 DW1000 modules) for anyone who wants to rebuild the
testbed.

Contents:

| Path | What it is |
|------|------------|
| `1.0.3/` | Modified In-Circuit Arduino core (STM32L1 / radino32), used in the paper |
| `dw1000_ranging_demo.ino` | Sketch flashed to each module (Anchor / Tag / Listener) |
| `MultiPort_CIR_Logger.py` | PC logger: multi-port CIR capture and CSV recording |

The sketch and logger match
[DW1000-CIR](https://github.com/liux4189/DW1000-CIR)
branch `Concurrent_Localization`. Usage below is copied from that
README.

---

## 1. Arduino core (`1.0.3/`)

Follow the official radino32 DW1000 Arduino setup first:

https://wiki.in-circuit.de/index.php5?title=radino32_DW1000#Software

In Arduino IDE, add the Boards Manager URL:

```text
http://library.radino.cc/Arduino_1_8/package_radino_radino32_index.json
```

Install the In-Circuit radino32 package so version **1.0.3** appears
on disk. Then **replace that folder** with `firmware/1.0.3/` from this
repository.

Typical locations:

```text
Windows:  %LOCALAPPDATA%\Arduino15\packages\In-circuit\hardware\stm3l1\1.0.3
macOS:    ~/Library/Arduino15/packages/In-circuit/hardware/stm3l1/1.0.3
Linux:    ~/.arduino15/packages/In-circuit/hardware/stm3l1/1.0.3
```

Keep the folder name `1.0.3`. After replacing, restart the IDE and
select a **Radino32** board.

The core is In-Circuit GmbH’s radino32 package with our DW1000 / DMA
changes. Unrelated vendor examples (CC1101, SX1272, WiFi, BLE) ship
with that package.

---

## 2. `MultiPort_CIR_Logger.py`

A real-time multi-port CIR acquisition and visualization tool with parallel data processing capabilities.

### System Requirements

```bash
pip install numpy scipy matplotlib pyserial
```

### Key Features

- Multi-port parallel acquisition from multiple serial ports
- Controllable CSV data logging
- Tkinter control window with real-time acquisition status

### Quick Start

1. Configure serial ports and antenna count (see parameter configuration below)
2. Run the program:

   ```bash
   python MultiPort_CIR_Logger.py
   ```

3. A control window will appear after startup; click the button to start/stop data recording

### Key configurable parameters

```python
# Antenna/Port Configuration
PORT_COUNT = 8  # Number of antennas/ports
COM_PORTS = ['COM11', 'COM12', 'COM22', 'COM13', 'COM18', 'COM19', 'COM20', 'COM16']
BAUD_RATE = 115200  # Baud rate

# Radio Parameters
FC = 3494.4e6           # Carrier frequency (Hz)
FS = 64e9               # Sampling rate (Hz)
LAMBDA = C / FC         # Wavelength (m)
D = LAMBDA / 2          # Antenna spacing (m)
UPSAMPLE_FACTOR = 64    # Upsampling factor

# CIR Processing Parameters
ORIGIN_START_IDX = 699  # CIR starting index
FRAME_RATE = 83         # Frame rate (Hz)

# Frame Rate Control
BEAMFORMING_SKIP_FRAMES = 1
RD_SKIP_FRAMES = 1

# Computational Complexity Control
THETA_RESOLUTION = 2    # Angular resolution (degrees)
RANGE_BINS_LIMIT = 300  # Range bin limit

# Range-Doppler Processing
RD_WINDOW = 83
WIN_RADIUS = 83
WINDOW_LEFT = 640
WINDOW_RIGHT = 2560
```

CSV save path: edit the `csv_file = ...` line in the `start()` method.

### Data format

- **CSV header**: `Sequence, PayloadLength, PacketType, maxNoise, firstPathAmp1, stdNoise, firstPathAmp2, firstPathAmp3, rxPreamCount, firstPath, CIR_real_0, CIR_imag_0, ...`
- **CIR data**: 100 complex samples (real/imaginary parts stored alternately)
- **Frame**: 6-byte header (`0xAA`), 14-byte diagnostic, 400-byte CIR, 4-byte footer (CRC16 + `0x55`)

Phase compensation (optional): load `phase_comp_vector1.csv` as MATLAB-style complex numbers (e.g. `1.0+0.5i`).

---

## 3. `dw1000_ranging_demo.ino`

Open this sketch in Arduino IDE after the core in §1 is installed.
Configure the device role with two flags at the top of the file:

| WE_ARE_ANCHOR | WE_ARE_LISTENER | Role | Description | Sensing | Localization |
|---------------|-----------------|------|-------------|---------|--------------|
| `1` | `1` | **Listener** | Passively listens to ranging packets and outputs CIR data | yes | yes |
| `1` | `0` | **Anchor** | Initiator that sends ranging requests to Tags | yes | yes |
| `0` | `0` | **Tag** | Responder that replies to Anchor ranging requests | no | yes |

```cpp
#define WE_ARE_ANCHOR    1    // 1 = Anchor/Listener, 0 = Tag
#define WE_ARE_LISTENER  1    // 1 = Listener, 0 = normal ranging
#define MY_SHORT_ADDRESS  0x2345
#define DW_TX_POWER      33.5     // optional; 0 to 33.5 dBm, step 0.5
#define PIN_LED    13
#define PIN_LED_1  18
#define PIN_LED_2  17
```

Listener (CIR collection):

```cpp
#define WE_ARE_ANCHOR    1
#define WE_ARE_LISTENER  1
#define MY_SHORT_ADDRESS 0x2345
```

Each device needs a unique `MY_SHORT_ADDRESS`. One Anchor can handle
up to 6 Tags. Respect local wireless regulations when setting TX power.
