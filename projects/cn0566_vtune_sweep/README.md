# CN0566 VTune Sweep (No-OS)

Userspace application that sweeps the ADF4159 PLL frequency on the
[CN0566 (ADALM-PHASER)](https://www.analog.com/en/resources/reference-designs/circuits-from-the-lab/cn0566.html)
phased array board and reads back the VCO tuning voltage (VTune) from the
AD7291 monitor at each step. A smoothly rising VTune confirms PLL lock across
the band.

## Hardware

- Raspberry Pi 4 with the CN0566 (ADALM-PHASER) board stacked on the GPIO
  header.
- ADF4159 fractional-N PLL connected via SPI0 CS2 (GPIO 27).
- AD7291 8-channel voltage/temperature monitor on I2C-1 at address 0x2A.
- HMC431LP4 VCO with a /4 prescaler feeding the ADF4159 RFIN. The LO
  frequency seen by the antenna array is 4x the PLL output.

## Prerequisites

- Raspberry Pi OS (or equivalent) with kernel headers and `dtc` installed.
- CMake >= 3.10 and a C compiler (`gcc`).
- No IIO kernel drivers loaded for the ADF4159 or ADAR1000, the No-OS
  overlay uses `spidev` so the devices stay in userspace.

## Device Tree Overlay Setup

The overlay configures SPI0 with three chip-selects for userspace access:
CS0 (ADAR1000 #0), CS1 (ADAR1000 #1), CS2 (ADF4159).

1. Compile the overlay:

   ```bash
   dtc -@ -I dts -O dtb -o rpi-cn0566-noos.dtbo projects/rpi-cn0566-noos-overlay.dts
   ```

2. Copy to the boot overlays directory:

   ```bash
   sudo cp rpi-cn0566-noos.dtbo /boot/overlays/
   ```

3. Edit `/boot/config.txt` and add the following line (remove or comment out
   any existing `dtoverlay=rpi-cn0566` line that loads the IIO kernel
   drivers):

   ```
   dtoverlay=rpi-cn0566-noos
   ```

4. Reboot:

   ```bash
   sudo reboot
   ```

5. Verify the SPI devices appeared:

   ```bash
   ls /dev/spidev0.*
   ```

   Expected output:

   ```
   /dev/spidev0.0  /dev/spidev0.1  /dev/spidev0.2
   ```

## Building

From the project directory:

```bash
cd projects/cn0566_vtune_sweep
mkdir -p build && cd build
cmake ..
make
```

The binary is produced at `build/cn0566_vtune_sweep`.

## Running

`sudo` is required for GPIO and SPI access.

### Sweep mode (default)

Sweeps signal frequency from 9.5 GHz to 12.5 GHz in 100 MHz steps, reading
VTune at each point:

```bash
sudo ./build/cn0566_vtune_sweep
sudo ./build/cn0566_vtune_sweep --sweep
```

### Fixed frequency mode

Sets a single frequency and reads VTune once:

```bash
sudo ./build/cn0566_vtune_sweep --fixed 10.5
```

### Help

```bash
sudo ./build/cn0566_vtune_sweep --help
```

## Expected Output

The application runs several diagnostic checks before the selected mode:

1. **AD7291 initialization** : confirms I2C communication with the voltage
   monitor.
2. **Board GPIO setup** : configures the CN0566 control signals (LO path
   switches, divider, TR switch).
3. **ADF4159 initialization** : programs all PLL registers via SPI.
4. **Mode execution** : runs the sweep or sets a fixed frequency.

### Sweep mode example (values are approximate)

```
Sweeping signal from 9.5 to 12.5 GHz (RX LO = 2.0 GHz)...
# freq_GHz,vtune_V
9.50,4.891
9.60,5.378
9.70,5.889
...
12.50,13.813

Sweep complete.
```

### Fixed frequency mode example

```
Setting signal frequency to 10.500 GHz (PLL = 3.125 GHz)...
10.500 GHz: VTune = 10.948 V
```

**Signal frequency convention:** The sweep labels match the Python
`phaser_find_hb100.py` convention where
`signal_freq = LO_freq - SDR_RX_LO`. The PLL is programmed at
`pll_freq = (signal_freq + 2 GHz) / 4`.

## Troubleshooting

- **VTune rails at ~13.8 V:** PLL is not locking. Check that the overlay is
  loaded (`ls /dev/spidev0.2`) and that no IIO kernel driver has claimed the
  device (`ls /sys/bus/iio/devices/` should not show an `adf4159`).
- **MUXOUT latch test fails (both read 0 or both read 1):** SPI data is not
  reaching the ADF4159. Verify the overlay is active and GPIO 27 is in ALT0
  mode (`raspi-gpio get 27`).
- **AD7291 init fails:** Check I2C bus with `i2cdetect -y 1`, address 0x2A
  should respond.

## File Structure

```
cn0566_vtune_sweep/
+-- CMakeLists.txt       # Build configuration
+-- README.md            # This file
+-- .gitignore           # Excludes build artifacts
+-- src/
    +-- main.c           # Application entry point and sweep logic
    +-- parameters.h     # Hardware pin/bus definitions for RPi + CN0566
```

## Drivers Used

- **ADF4159** :`drivers/frequency/adf4159/` (No-OS fractional-N PLL driver)
- **AD7291** : `drivers/power/ad7291/` (No-OS I2C voltage monitor driver)
- **Linux platform** : `drivers/platform/linux/` (SPI, I2C, GPIO, delay)
