#!/usr/bin/env python3
"""CN0566 ADAR1000 gain/phase validation using a no-OS C command server."""
import os
import pickle
import subprocess
import numpy as np
import matplotlib.pyplot as plt
from adi import ad9361
from scipy import signal
from numpy.fft import fft, fftfreq, fftshift

SAMPLE_RATE = 30_000_000
RX_LO = 2_200_000_000
BUFFER_SIZE = 4096
RX_GAIN = 0
SIGNAL_FREQ = 10.525e9
HB100_CAL_FILE = "hb100_freq_val.pkl"
C_EXECUTABLE = "cn0566_adar1000_test"


def load_signal_freq(require_cal=True):
    paths = [HB100_CAL_FILE,
             os.path.expanduser("~/pyadi-iio/examples/phaser/" + HB100_CAL_FILE),
             "/home/analog/pyadi-iio/examples/phaser/" + HB100_CAL_FILE]
    for path in paths:
        if os.path.isfile(path):
            with open(path, "rb") as f:
                freq = float(pickle.load(f))
            print(f"HB100 calibration: {freq / 1e9:.6f} GHz ({path})")
            return freq
    if require_cal:
        raise RuntimeError("HB100 calibration file not found; measure the source frequency first")
    print(f"WARNING: using nominal HB100 frequency {SIGNAL_FREQ / 1e9:.3f} GHz")
    return SIGNAL_FREQ


def spectrum(x, fs, ref=2 ** 12):
    n = len(x)
    window = signal.windows.kaiser(n, beta=14)
    coherent_gain = np.mean(window)
    amplitude = np.abs(fftshift(fft(x * window))) / (n * coherent_gain)
    dbfs = 20.0 * np.log10(np.maximum(amplitude / ref, 1e-20))
    freqs = fftshift(fftfreq(n, 1.0 / fs))
    return dbfs, freqs


def tone_result(x, expected_hz, search_hz=2_000_000):
    dbfs, freqs = spectrum(x, SAMPLE_RATE)
    mask = np.abs(freqs - expected_hz) <= search_hz
    if not np.any(mask):
        raise RuntimeError("expected tone is outside the capture bandwidth")
    indices = np.flatnonzero(mask)
    peak_index = indices[np.argmax(dbfs[mask])]
    lo = max(0, peak_index - 1)
    hi = min(len(x), peak_index + 2)
    linear = np.sum(10.0 ** (dbfs[lo:hi] / 10.0))
    integrated_dbfs = 10.0 * np.log10(max(linear, 1e-20))
    return freqs[peak_index], dbfs[peak_index], integrated_dbfs


class CProcess:
    def __init__(self, executable):
        self.proc = subprocess.Popen(["sudo", executable], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                     text=True, bufsize=1)
        while True:
            line = self.proc.stdout.readline()
            if not line:
                raise RuntimeError("C process failed during init:\n" + self.proc.stderr.read())
            print("[hw] " + line.rstrip())
            if line.rstrip() == "READY":
                break

    def command(self, text):
        self.proc.stdin.write(text + "\n")
        self.proc.stdin.flush()
        lines = []
        while True:
            line = self.proc.stdout.readline()
            if not line:
                raise RuntimeError("C process terminated")
            line = line.rstrip()
            lines.append(line)
            if line == "OK" or line.startswith("ERR:"):
                return lines

    def close(self):
        if self.proc.poll() is None:
            try:
                self.command("quit")
            except Exception:
                pass
            self.proc.terminate()


def build_executable():
    root = os.path.dirname(os.path.abspath(__file__))
    build = os.path.join(root, "build")
    os.makedirs(build, exist_ok=True)
    if not os.path.isfile(os.path.join(build, "CMakeCache.txt")):
        subprocess.run(["cmake", ".."], cwd=build, check=True)
    # Always build so changed source/header files cannot leave a stale executable.
    subprocess.run(["cmake", "--build", ".", "--", "-j4"], cwd=build, check=True)
    return os.path.join(build, C_EXECUTABLE)


def connect_sdr():
    sdr = None
    for uri in ("ip:localhost", "ip:192.168.2.1"):
        try:
            sdr = ad9361(uri=uri)
            print("Pluto connected at", uri)
            break
        except Exception:
            pass
    if sdr is None:
        raise RuntimeError("Cannot connect to PlutoSDR")

    sdr.rx_enabled_channels = [0, 1]
    sdr._rxadc.set_kernel_buffers_count(1)
    sdr.sample_rate = SAMPLE_RATE
    sdr.rx_buffer_size = BUFFER_SIZE
    sdr.rx_rf_bandwidth = 10_000_000
    sdr.rx_lo = RX_LO
    sdr.gain_control_mode_chan0 = "manual"
    sdr.gain_control_mode_chan1 = "manual"
    sdr.rx_hardwaregain_chan0 = RX_GAIN
    sdr.rx_hardwaregain_chan1 = RX_GAIN
    sdr.tx_hardwaregain_chan0 = -88
    sdr.tx_hardwaregain_chan1 = -88
    for _ in range(3):
        sdr.rx()
    return sdr


def capture(sdr, expected_hz):
    sdr.rx()  # discard stale buffer
    ch0, ch1 = sdr.rx()

    # Per-channel peak and RMS
    for name, data in (("voltage0", ch0), ("voltage1", ch1)):
        peak = np.max(np.abs(data))
        rms = np.sqrt(np.mean(np.abs(data) ** 2))
        print(f" {name}: peak={peak:.3f} RMS={rms:.3f}")

    # Per-channel spectrum, find peak near expected tone (xB12 MHz window)
    search_bw = 2_000_000
    for name, data in (("voltage0", ch0), ("voltage1", ch1)):
        dbfs, freqs = spectrum(data, SAMPLE_RATE)
        mask = np.abs(freqs - expected_hz) <= search_bw
        if np.any(mask):
            indices = np.flatnonzero(mask)
            peak_idx = indices[np.argmax(dbfs[mask])]
        else:
            peak_idx = np.argmax(dbfs)
        print(f" {name}: peak frequency = {freqs[peak_idx] / 1e6:.6f} MHz "
              f"({dbfs[peak_idx]:.1f} dBFS)")

    # Full spectrum of ch0 for plot
    dbfs_plot, freqs_plot = spectrum(ch0, SAMPLE_RATE)
    freqs_mhz = freqs_plot / 1e6
    mask = np.abs(freqs_plot - expected_hz) <= search_bw
    if np.any(mask):
        indices = np.flatnonzero(mask)
        plot_peak_idx = indices[np.argmax(dbfs_plot[mask])]
    else:
        plot_peak_idx = np.argmax(dbfs_plot)

    # Plot: time domain + spectrum
    plt.figure(figsize=(10, 6))
    plt.subplot(2, 1, 1)
    plt.title("Time Domain I/Q Data")
    plt.plot(ch0.real, marker="o", ms=2, color="red", label="RX0")
    plt.plot(ch1.real, marker="o", ms=2, color="blue", label="RX1")
    plt.xlabel("Sample")
    plt.ylabel("ADC output")
    plt.legend()

    plt.subplot(2, 1, 2)
    plt.title(f"Spectrum x97 peak at {freqs_mhz[plot_peak_idx]:.3f} MHz")
    plt.plot(freqs_mhz, dbfs_plot, marker="o", ms=2)
    plt.axvline(x=expected_hz / 1e6, color="r", linestyle="--", alpha=0.5,
                label=f"expected {expected_hz / 1e6:.1f} MHz")
    plt.xlabel("Frequency [MHz]")
    plt.ylabel("dBFS")
    plt.legend()

    plt.tight_layout()
    plt.show()


HELP = """Commands:
  gain <chip> <ch> <0-127>   set one ADAR1000 channel gain
  read_gain <chip> <ch>      read raw gain/VGA/attenuator state
  gain_all <0-127>           set all eight gains
  phase <chip> <ch> <0-359> set one phase
  phase_all <0-359>          set all phases
  verify <chip>              dump/verify ADAR1000 RX registers
  pll <hz> | pll_full <hz>   tune ADF4159
  lock | gpios | dump        diagnostics
  capture                    capture and analyze RX0/RX1 separately
  isolate                    print one-channel isolation test commands
  quit
"""


def main():
    hw = CProcess(build_executable())
    sdr = connect_sdr()
    signal_freq = load_signal_freq(require_cal=True)
    offset = 1_000_000
    pll_freq = int((signal_freq + RX_LO - offset) / 4)
    for line in hw.command(f"pll_full {pll_freq}"):
        print("[hw]", line)
    for line in hw.command("lock"):
        print("[hw]", line)
    expected_hz = -offset
    print(HELP)

    try:
        while True:
            cmd = input("phaser> ").strip()
            if not cmd:
                continue
            if cmd == "quit":
                break
            if cmd == "help":
                print(HELP)
            elif cmd == "capture":
                capture(sdr, expected_hz)
            elif cmd == "isolate":
                print("Run gain_all 0, capture, then enable one channel at a time:")
                for chip in range(2):
                    for ch in range(4):
                        print(f"  gain_all 0; gain {chip} {ch} 127; capture")
            else:
                for line in hw.command(cmd):
                    print("[hw]", line)
    finally:
        hw.close()
        del sdr


if __name__ == "__main__":
    main()
