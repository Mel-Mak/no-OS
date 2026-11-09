#!/usr/bin/env python3
"""CN0566 ADAR1000 gain/phase validation using a no-OS C command server."""
import json
import os
import pickle
import subprocess
from time import sleep
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

def rms(x):
    """Complex-signal RMS."""
    return np.sqrt(np.mean(np.abs(x) ** 2))


def detect_tone_frequency(x, expected_hz, search_hz=2_000_000):
    """
    Find the strongest FFT tone near expected_hz.

    Returns:
        tone_hz
        peak_dbfs
    """
    dbfs, freqs = spectrum(x, SAMPLE_RATE)

    mask = np.abs(freqs - expected_hz) <= search_hz
    if not np.any(mask):
        raise RuntimeError("Expected tone is outside the capture bandwidth")

    indices = np.flatnonzero(mask)
    peak_idx = indices[np.argmax(dbfs[mask])]

    return freqs[peak_idx], dbfs[peak_idx]


def tone_phasor(x, tone_hz, fs):
    """
    Estimate the complex amplitude of a tone by coherent demodulation.

    The phase of the returned complex number represents the phase of the
    selected tone.
    """
    n = np.arange(len(x))
    mixer = np.exp(-1j * 2.0 * np.pi * tone_hz * n / fs)

    # Apply a window to reduce leakage from neighboring frequencies.
    window = signal.windows.hann(len(x), sym=False)
    coherent_gain = np.mean(window)

    return np.mean(x * mixer * window) / coherent_gain


def relative_phase_deg(ch0, ch1, tone_hz):
    """
    Return the phase of RX1 relative to RX0 in the range -180 to +180 degrees.
    """
    phasor0 = tone_phasor(ch0, tone_hz, SAMPLE_RATE)
    phasor1 = tone_phasor(ch1, tone_hz, SAMPLE_RATE)

    phase_deg = np.degrees(np.angle(phasor1 * np.conj(phasor0)))
    return phase_deg, phasor0, phasor1


def tone_power_db(x, tone_hz):
    """
    Return tone power from the coherently measured tone phasor.

    This is suitable for relative cancellation measurements. It is not
    intended to be an absolute calibrated dBFS measurement.
    """
    phasor = tone_phasor(x, tone_hz, SAMPLE_RATE)
    return 20.0 * np.log10(max(np.abs(phasor), 1e-20))


def tone_dbfs_at_frequency(x, tone_hz, tracking_hz=200_000):
    """
    Measure the FFT peak near a known tone frequency.
    """
    dbfs, freqs = spectrum(x, SAMPLE_RATE)

    mask = np.abs(freqs - tone_hz) <= tracking_hz
    if not np.any(mask):
        raise RuntimeError("Tone tracking region is outside FFT range")

    indices = np.flatnonzero(mask)
    peak_idx = indices[np.argmax(dbfs[mask])]

    return freqs[peak_idx], dbfs[peak_idx]


def normalized_signals(ch0, ch1):
    """
    Scale RX1 to have the same RMS amplitude as RX0.

    This compensates for amplitude mismatch when checking whether the
    relative phase is close to 180 degrees.
    """
    ch0_rms = rms(ch0)
    ch1_rms = rms(ch1)

    if ch1_rms <= 1e-20:
        raise RuntimeError("RX1 RMS is zero; cannot normalize")

    scale = ch0_rms / ch1_rms
    ch1_normalized = ch1 * scale

    return ch1_normalized, scale

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
    sum_data = ch0 + ch1

    # Per-channel and sum peak/RMS
    for name, data in (("voltage0", ch0), ("voltage1", ch1), ("sum", sum_data)):
        peak = np.max(np.abs(data))
        rms = np.sqrt(np.mean(np.abs(data) ** 2))
        print(f" {name}: peak={peak:.3f} RMS={rms:.3f}")

    # Per-channel and sum spectrum peaks
    search_bw = 2_000_000
    for name, data in (("voltage0", ch0), ("voltage1", ch1), ("sum", sum_data)):
        dbfs, freqs = spectrum(data, SAMPLE_RATE)
        mask = np.abs(freqs - expected_hz) <= search_bw
        if np.any(mask):
            indices = np.flatnonzero(mask)
            peak_idx = indices[np.argmax(dbfs[mask])]
        else:
            peak_idx = np.argmax(dbfs)
        print(f" {name}: peak frequency = {freqs[peak_idx] / 1e6:.6f} MHz "
              f"({dbfs[peak_idx]:.1f} dBFS)")

    # Spectra for plotting
    dbfs_ch0, freqs_plot = spectrum(ch0, SAMPLE_RATE)
    dbfs_ch1, _ = spectrum(ch1, SAMPLE_RATE)
    dbfs_sum, _ = spectrum(sum_data, SAMPLE_RATE)
    freqs_mhz = freqs_plot / 1e6
    mask = np.abs(freqs_plot - expected_hz) <= search_bw
    if np.any(mask):
        indices = np.flatnonzero(mask)
        sum_peak_idx = indices[np.argmax(dbfs_sum[mask])]
    else:
        sum_peak_idx = np.argmax(dbfs_sum)
    print(f"Peak frequency found at  {freqs_mhz[sum_peak_idx]:.6f} MHz.")

    # Plot: time domain + per-channel spectrum + sum spectrum
    plt.figure(figsize=(10, 9))
    plt.subplot(3, 1, 1)
    plt.title("Time Domain I/Q Data")
    plt.plot(ch0.real, marker="o", ms=2, color="red", label="RX0")
    plt.plot(ch1.real, marker="o", ms=2, color="blue", label="RX1")
    plt.plot(sum_data.real, marker="o", ms=2, color="green", alpha=0.5, label="SUM")
    plt.xlabel("Sample")
    plt.ylabel("ADC output")
    plt.legend()

    plt.subplot(3, 1, 2)
    plt.title("Per-Channel Spectrum")
    plt.plot(freqs_mhz, dbfs_ch0, ms=2, label="RX0", alpha=0.7)
    plt.plot(freqs_mhz, dbfs_ch1, ms=2, label="RX1", alpha=0.7)
    plt.xlabel("Frequency [MHz]")
    plt.ylabel("dBFS")
    plt.legend()

    plt.subplot(3, 1, 3)
    plt.title(f"Sum Spectrum peak at {freqs_mhz[sum_peak_idx]:.3f} MHz "
              f"({dbfs_sum[sum_peak_idx]:.1f} dBFS)")
    plt.plot(freqs_mhz, dbfs_sum, marker="o", ms=2, color="green")
    plt.axvline(x=expected_hz / 1e6, color="r", linestyle="--", alpha=0.5,
                label=f"expected {expected_hz / 1e6:.1f} MHz")
    plt.xlabel("Frequency [MHz]")
    plt.ylabel("dBFS")
    plt.legend()

    plt.tight_layout()
    plt.show()


NUM_ELEMENTS = 8
CAL_AVERAGES = 10
CAL_SETTLE_S = 1.0
CAL_FILE = "cal_data.json"
PHASE_STEP_SIZE = 2.8125


def element_to_chip_ch(element):
    return element // 4, element % 4


def wrap_phase(angle):
    if angle > 180.0:
        angle -= 360.0
    return angle


# ---------------------------------------------------------------------------
# Channel calibration routine.
# Must run BEFORE gain calibration.
# ---------------------------------------------------------------------------

def measure_channel_gain(hw, sdr, channel, expected_hz, averages=CAL_AVERAGES):
    hw.command("gain_all 0")
    chip = 1 - channel
    for ch in range(4):
        hw.command(f"gain {chip} {ch} 127")
    sleep(CAL_SETTLE_S)

    total = 0.0
    for _ in range(averages):
        sdr.rx()
        ch0, ch1 = sdr.rx()
        _, peak_dbfs, _ = tone_result(ch0 + ch1, expected_hz)
        total += 10.0 ** (peak_dbfs / 20.0)
    return total / averages


def channel_calibration(hw, sdr, expected_hz, verbose=True):
    levels = []
    for channel in range(2):
        if verbose:
            print(f"Measuring SDR channel {channel}...")
        level = measure_channel_gain(hw, sdr, channel, expected_hz)
        if verbose:
            print(f"  level = {level:.6f}")
        levels.append(level)

    ch_mismatch_db = 20.0 * np.log10(levels[0] / levels[1])
    if verbose:
        print(f"Channel mismatch: {ch_mismatch_db:.2f} dB")
    if ch_mismatch_db > 0:
        ccal = [0.0, ch_mismatch_db]
    else:
        ccal = [-ch_mismatch_db, 0.0]
    return ccal


# ---------------------------------------------------------------------------
# Gain calibration routine.
# ---------------------------------------------------------------------------

def measure_element_gain(hw, sdr, element, expected_hz, averages=CAL_AVERAGES):
    chip, ch = element_to_chip_ch(element)
    hw.command("gain_all 0")
    hw.command(f"gain {chip} {ch} 127")
    sleep(CAL_SETTLE_S)

    total = 0.0
    for _ in range(averages):
        sdr.rx()
        ch0, ch1 = sdr.rx()
        _, peak_dbfs, _ = tone_result(ch0 + ch1, expected_hz)
        total += 10.0 ** (peak_dbfs / 20.0)
    return total / averages


def gain_calibration(hw, sdr, expected_hz, verbose=True):
    element_levels = []
    for el in range(NUM_ELEMENTS):
        if verbose:
            chip, ch = element_to_chip_ch(el)
            print(f"Measuring element {el} (chip {chip} ch {ch})...")
        level = measure_element_gain(hw, sdr, el, expected_hz)
        if verbose:
            print(f"  level = {level:.6f}")
        element_levels.append(level)

    gcal = [min(element_levels) / lev for lev in element_levels]
    if verbose:
        print("Gain calibration coefficients:")
        for el, g in enumerate(gcal):
            print(f"  element {el}: {g:.4f}")
    return gcal


# ---------------------------------------------------------------------------
# Phase calibration routine.
# Algorithm: for each adjacent pair, sweep the phase of the second element
# while holding the first at 0. The null (destructive interference) occurs
# when the two elements are 180 deg apart, so the correction is 180 minus
# the null position.
# ---------------------------------------------------------------------------

def phase_cal_sweep(hw, sdr, ref_el, cal_el, expected_hz, gcal,
                    averages=CAL_AVERAGES):
    hw.command("gain_all 0")
    ref_chip, ref_ch = element_to_chip_ch(ref_el)
    cal_chip, cal_ch = element_to_chip_ch(cal_el)

    ref_gain = int(round(127 * gcal[ref_el]))
    cal_gain = int(round(127 * gcal[cal_el]))
    hw.command(f"gain {ref_chip} {ref_ch} {ref_gain}")
    hw.command(f"gain {cal_chip} {cal_ch} {cal_gain}")
    sleep(CAL_SETTLE_S)

    hw.command(f"phase {ref_chip} {ref_ch} 0")

    phase_values = np.arange(-180, 180, PHASE_STEP_SIZE)
    gains = []
    for phase_deg in phase_values:
        phase_cmd = int(round(phase_deg)) % 360
        hw.command(f"phase {cal_chip} {cal_ch} {phase_cmd}")

        total = 0.0
        for _ in range(averages):
            sdr.rx()
            ch0, ch1 = sdr.rx()
            _, peak_dbfs, _ = tone_result(ch0 + ch1, expected_hz)
            total += 10.0 ** (peak_dbfs / 20.0)
        gains.append(total / averages)

    return phase_values, gains


def phase_calibration(hw, sdr, expected_hz, gcal, verbose=True):
    pcal = [0.0] * NUM_ELEMENTS
    ph_deltas = [0.0] * (NUM_ELEMENTS - 1)

    for cal_el in range(NUM_ELEMENTS - 1):
        ref_el = cal_el
        if verbose:
            print(f"Phase cal: element {ref_el} vs {cal_el + 1}...")

        phase_values, gains = phase_cal_sweep(
            hw, sdr, ref_el, cal_el + 1, expected_hz, gcal)

        null_idx = int(np.argmin(gains))
        null_phase = phase_values[null_idx]
        ph_delta = wrap_phase((180.0 - null_phase) % 360.0)

        if verbose:
            print(f"  null at {null_phase:.1f} deg, correction = {ph_delta:.1f} deg")

        ph_deltas[cal_el] = ph_delta
        pcal[cal_el + 1] = wrap_phase((pcal[cal_el] - ph_delta) % 360.0)

    if verbose:
        print("Phase calibration offsets:")
        for el, p in enumerate(pcal):
            print(f"  element {el}: {p:.1f} deg")
    return pcal, ph_deltas


# ---------------------------------------------------------------------------
# Apply calibration and save/load
# ---------------------------------------------------------------------------

def apply_gain_cal(hw, gcal, base_gain=127):
    for el in range(NUM_ELEMENTS):
        chip, ch = element_to_chip_ch(el)
        calibrated = int(round(base_gain * gcal[el]))
        calibrated = max(0, min(127, calibrated))
        hw.command(f"gain {chip} {ch} {calibrated}")


def apply_phase_cal(hw, pcal):
    for el in range(NUM_ELEMENTS):
        chip, ch = element_to_chip_ch(el)
        phase_cmd = int(round(pcal[el])) % 360
        hw.command(f"phase {chip} {ch} {phase_cmd}")


def apply_calibration(hw, cal):
    if "gcal" in cal:
        apply_gain_cal(hw, cal["gcal"])
    if "pcal" in cal:
        apply_phase_cal(hw, cal["pcal"])


def save_calibration(gcal, pcal=None, ccal=None, filename=CAL_FILE):
    data = {"gcal": gcal}
    if pcal is not None:
        data["pcal"] = pcal
    if ccal is not None:
        data["ccal"] = ccal
    with open(filename, "w") as f:
        json.dump(data, f, indent=2)
    print(f"Calibration saved to {filename}")


def load_calibration(filename=CAL_FILE):
    with open(filename) as f:
        return json.load(f)


def run_full_calibration(hw, sdr, expected_hz, verbose=True):
    print("=== Channel calibration ===")
    ccal = channel_calibration(hw, sdr, expected_hz, verbose)

    print("\n=== Gain calibration ===")
    gcal = gain_calibration(hw, sdr, expected_hz, verbose)
    apply_gain_cal(hw, gcal)

    print("\n=== Phase calibration ===")
    pcal, _ = phase_calibration(hw, sdr, expected_hz, gcal, verbose)
    apply_phase_cal(hw, pcal)

    save_calibration(gcal, pcal, ccal)
    print("\nCalibration complete. All corrections applied.")


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
  cal                        run full calibration (gain + phase), save results
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

    if os.path.isfile(CAL_FILE):
        cal = load_calibration()
        apply_calibration(hw, cal)
        print(f"Loaded calibration from {CAL_FILE}")
    else:
        print("No calibration file found. Run 'cal' to calibrate.")

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
            elif cmd == "cal":
                run_full_calibration(hw, sdr, expected_hz)
            else:
                for line in hw.command(cmd):
                    print("[hw]", line)
    finally:
        hw.close()
        del sdr


if __name__ == "__main__":
    main()
