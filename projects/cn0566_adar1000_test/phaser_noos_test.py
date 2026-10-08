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
import sys
sys.path.insert(0,os.path.expanduser("~/pyadi-iio/examples/phaser"))
from phaser_functions import spec_est

SAMPLE_RATE = 30_000_000
RX_LO = 2_200_000_000
BUFFER_SIZE = 1024
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


def capture(sdr, expected_hz, show_plot=True):
    # Discard stale DMA data.
    sdr.rx()

    ch0, ch1 = sdr.rx()
    ch0 = np.asarray(ch0, dtype=np.complex128)
    ch1 = np.asarray(ch1, dtype=np.complex128)

    # Detect the actual tone using the stronger individual channel.
    reference_channel = ch0 if rms(ch0) >= rms(ch1) else ch1

    tone_hz, reference_peak_dbfs = detect_tone_frequency(
        reference_channel,
        expected_hz,
        search_hz=2_000_000
    )

    # Raw signal combinations.
    sum_raw = ch0 + ch1
    diff_raw = ch0 - ch1

    # Normalize RX1 amplitude to RX0 before summing.
    ch1_normalized, normalization_scale = normalized_signals(ch0, ch1)
    sum_normalized = ch0 + ch1_normalized
    diff_normalized = ch0 - ch1_normalized

    # Measure relative tone phase.
    phase_deg, phasor0, phasor1 = relative_phase_deg(
        ch0,
        ch1,
        tone_hz
    )

    # FFT measurements at the detected tone.
    _, ch0_tone_dbfs = tone_dbfs_at_frequency(ch0, tone_hz)
    _, ch1_tone_dbfs = tone_dbfs_at_frequency(ch1, tone_hz)
    _, sum_raw_dbfs = tone_dbfs_at_frequency(sum_raw, tone_hz)
    _, sum_norm_dbfs = tone_dbfs_at_frequency(sum_normalized, tone_hz)
    _, diff_raw_dbfs = tone_dbfs_at_frequency(diff_raw, tone_hz)
    _, diff_norm_dbfs = tone_dbfs_at_frequency(diff_normalized, tone_hz)

    # Coherent tone amplitudes for cancellation comparison.
    tone_ch0_db = tone_power_db(ch0, tone_hz)
    tone_ch1_db = tone_power_db(ch1, tone_hz)
    tone_sum_raw_db = tone_power_db(sum_raw, tone_hz)
    tone_sum_norm_db = tone_power_db(sum_normalized, tone_hz)
    tone_diff_norm_db = tone_power_db(diff_normalized, tone_hz)

    strongest_channel_db = max(tone_ch0_db, tone_ch1_db)

    raw_cancellation_db = tone_sum_raw_db - strongest_channel_db
    normalized_cancellation_db = tone_sum_norm_db - strongest_channel_db

    print("\nCapture results")
    print("----------------")
    print(f"Configured expected tone : {expected_hz / 1e6:+.6f} MHz")
    print(f"Detected actual tone     : {tone_hz / 1e6:+.6f} MHz")
    print(f"Detection difference     : "
          f"{(tone_hz - expected_hz) / 1e3:+.1f} kHz")
    print(f"Reference FFT peak       : {reference_peak_dbfs:.1f} dBFS")
    print()

    print(f"RX0 RMS                  : {rms(ch0):.3f}")
    print(f"RX1 RMS                  : {rms(ch1):.3f}")
    print(f"RX1 normalization scale  : {normalization_scale:.6f}")
    print()

    print(f"RX1 relative to RX0      : {phase_deg:+.2f} degrees")
    print(f"RX0 tone                 : {ch0_tone_dbfs:.1f} dBFS")
    print(f"RX1 tone                 : {ch1_tone_dbfs:.1f} dBFS")
    print(f"Raw SUM tone             : {sum_raw_dbfs:.1f} dBFS")
    print(f"Normalized SUM tone      : {sum_norm_dbfs:.1f} dBFS")
    print(f"Raw DIFF tone            : {diff_raw_dbfs:.1f} dBFS")
    print(f"Normalized DIFF tone     : {diff_norm_dbfs:.1f} dBFS")
    print()

    print(f"Raw SUM relative level   : {raw_cancellation_db:+.2f} dB")
    print(f"Normalized SUM level     : {normalized_cancellation_db:+.2f} dB")

    result = {
        "ch0": ch0,
        "ch1": ch1,
        "sum_raw": sum_raw,
        "sum_normalized": sum_normalized,
        "diff_raw": diff_raw,
        "diff_normalized": diff_normalized,
        "tone_hz": tone_hz,
        "phase_deg": phase_deg,
        "phasor0": phasor0,
        "phasor1": phasor1,
        "normalization_scale": normalization_scale,
        "sum_raw_dbfs": sum_raw_dbfs,
        "sum_norm_dbfs": sum_norm_dbfs,
        "diff_raw_dbfs": diff_raw_dbfs,
        "diff_norm_dbfs": diff_norm_dbfs,
        "raw_cancellation_db": raw_cancellation_db,
        "normalized_cancellation_db": normalized_cancellation_db
    }

    if not show_plot:
        return result

    # Calculate spectra.
    dbfs_ch0, freqs = spectrum(ch0, SAMPLE_RATE)
    dbfs_ch1, _ = spectrum(ch1, SAMPLE_RATE)
    dbfs_sum_raw, _ = spectrum(sum_raw, SAMPLE_RATE)
    dbfs_sum_norm, _ = spectrum(sum_normalized, SAMPLE_RATE)
    dbfs_diff_raw, _ = spectrum(diff_raw, SAMPLE_RATE)

    freqs_mhz = freqs / 1e6

    # Display only a useful region around the tone.
    plot_span_hz = 3_000_000
    frequency_mask = np.abs(freqs - tone_hz) <= plot_span_hz

    # Use a smaller number of samples for readable time-domain plots.
    time_samples = min(500, len(ch0))
    sample_axis = np.arange(time_samples)

    fig = plt.figure(figsize=(14, 11))
    grid = fig.add_gridspec(3, 2)

    # --------------------------------------------------------------
    # Plot 1: Individual channels in time domain
    # --------------------------------------------------------------
    ax1 = fig.add_subplot(grid[0, 0])
    ax1.set_title("Individual RX Channels: Real Component")
    ax1.plot(
        sample_axis,
        ch0.real[:time_samples],
        color="red",
        label="RX0"
    )
    ax1.plot(
        sample_axis,
        ch1.real[:time_samples],
        color="blue",
        label="RX1"
    )
    ax1.set_xlabel("Sample")
    ax1.set_ylabel("ADC output")
    ax1.grid(True, alpha=0.3)
    ax1.legend()

    # --------------------------------------------------------------
    # Plot 2: Raw and normalized sum in time domain
    # --------------------------------------------------------------
    ax2 = fig.add_subplot(grid[0, 1])
    ax2.set_title("SUM Comparison")
    ax2.plot(
        sample_axis,
        sum_raw.real[:time_samples],
        color="green",
        label="Raw SUM"
    )
    ax2.plot(
        sample_axis,
        sum_normalized.real[:time_samples],
        color="purple",
        label="Normalized SUM"
    )
    ax2.set_xlabel("Sample")
    ax2.set_ylabel("ADC output")
    ax2.grid(True, alpha=0.3)
    ax2.legend()

    # --------------------------------------------------------------
    # Plot 3: Per-channel spectrum
    # --------------------------------------------------------------
    ax3 = fig.add_subplot(grid[1, 0])
    ax3.set_title("Per-Channel Spectrum")
    ax3.plot(
        freqs_mhz[frequency_mask],
        dbfs_ch0[frequency_mask],
        label=f"RX0: {ch0_tone_dbfs:.1f} dBFS"
    )
    ax3.plot(
        freqs_mhz[frequency_mask],
        dbfs_ch1[frequency_mask],
        label=f"RX1: {ch1_tone_dbfs:.1f} dBFS"
    )
    ax3.axvline(
        tone_hz / 1e6,
        color="black",
        linestyle="--",
        alpha=0.5,
        label=f"Tone: {tone_hz / 1e6:+.3f} MHz"
    )
    ax3.set_xlabel("Frequency [MHz]")
    ax3.set_ylabel("dBFS")
    ax3.grid(True, alpha=0.3)
    ax3.legend()

    # --------------------------------------------------------------
    # Plot 4: SUM and DIFF spectrum
    # --------------------------------------------------------------
    ax4 = fig.add_subplot(grid[1, 1])
    ax4.set_title("SUM and DIFF Spectrum")
    ax4.plot(
        freqs_mhz[frequency_mask],
        dbfs_sum_raw[frequency_mask],
        color="green",
        label=f"Raw SUM: {sum_raw_dbfs:.1f} dBFS"
    )
    ax4.plot(
        freqs_mhz[frequency_mask],
        dbfs_sum_norm[frequency_mask],
        color="purple",
        label=f"Normalized SUM: {sum_norm_dbfs:.1f} dBFS"
    )
    ax4.plot(
        freqs_mhz[frequency_mask],
        dbfs_diff_raw[frequency_mask],
        color="orange",
        alpha=0.8,
        label=f"Raw DIFF: {diff_raw_dbfs:.1f} dBFS"
    )
    ax4.axvline(
        tone_hz / 1e6,
        color="black",
        linestyle="--",
        alpha=0.5
    )
    ax4.set_xlabel("Frequency [MHz]")
    ax4.set_ylabel("dBFS")
    ax4.grid(True, alpha=0.3)
    ax4.legend()

    # --------------------------------------------------------------
    # Plot 5: Tone phasor diagram
    # --------------------------------------------------------------
    ax5 = fig.add_subplot(grid[2, 0])

    max_phasor = max(abs(phasor0), abs(phasor1), 1e-20)

    p0 = phasor0 / max_phasor
    p1 = phasor1 / max_phasor

    ax5.quiver(
        0, 0,
        p0.real, p0.imag,
        angles="xy",
        scale_units="xy",
        scale=1,
        color="red",
        width=0.012,
        label="RX0"
    )

    ax5.quiver(
        0, 0,
        p1.real, p1.imag,
        angles="xy",
        scale_units="xy",
        scale=1,
        color="blue",
        width=0.012,
        label="RX1"
    )

    ax5.scatter(
        [p0.real, p1.real],
        [p0.imag, p1.imag],
        color=["red", "blue"]
    )

    ax5.annotate(
        "RX0",
        (p0.real, p0.imag),
        xytext=(5, 5),
        textcoords="offset points"
    )

    ax5.annotate(
        "RX1",
        (p1.real, p1.imag),
        xytext=(5, 5),
        textcoords="offset points"
    )

    ax5.axhline(0, color="gray", linewidth=0.8)
    ax5.axvline(0, color="gray", linewidth=0.8)
    ax5.set_xlim(-1.2, 1.2)
    ax5.set_ylim(-1.2, 1.2)
    ax5.set_aspect("equal", adjustable="box")
    ax5.set_xlabel("In-phase")
    ax5.set_ylabel("Quadrature")
    ax5.set_title(
        f"Tone Phasors: RX1 Relative Phase = {phase_deg:+.1f} degrees"
    )
    ax5.grid(True, alpha=0.3)

    # --------------------------------------------------------------
    # Plot 6: Measurement summary
    # --------------------------------------------------------------
    ax6 = fig.add_subplot(grid[2, 1])
    ax6.axis("off")

    summary = (
        "Cancellation Measurement\n"
        "------------------------\n"
        f"Configured frequency: {expected_hz / 1e6:+.6f} MHz\n"
        f"Detected frequency:   {tone_hz / 1e6:+.6f} MHz\n\n"
        f"RX0 RMS:               {rms(ch0):.3f}\n"
        f"RX1 RMS:               {rms(ch1):.3f}\n"
        f"RX1 scale factor:      {normalization_scale:.4f}\n\n"
        f"Measured phase:        {phase_deg:+.2f} degrees\n"
        f"Raw SUM tone:          {sum_raw_dbfs:.1f} dBFS\n"
        f"Normalized SUM tone:   {sum_norm_dbfs:.1f} dBFS\n"
        f"Normalized DIFF tone:  {diff_norm_dbfs:.1f} dBFS\n\n"
        f"Raw SUM relative:      {raw_cancellation_db:+.2f} dB\n"
        f"Normalized SUM:        {normalized_cancellation_db:+.2f} dB"
    )

    ax6.text(
        0.03,
        0.97,
        summary,
        transform=ax6.transAxes,
        va="top",
        ha="left",
        family="monospace",
        fontsize=11
    )

    fig.suptitle(
        "CN0566 ADAR1000 Phase-Cancellation Validation",
        fontsize=15
    )

    plt.tight_layout()
    plt.show()

    return result

HELP = """Commands:
  gain <chip> <ch> <0-127>   set one ADAR1000 channel gain
  read_gain <chip> <ch>      read raw gain/VGA/attenuator state
  gain_all <0-127>           set all eight gains
  phase <chip> <ch> <0-359> set one phase
  phase_all <0-359>          set all phases
  phase_chip <chip> <0-359>   set all phase for one ADAR1000 
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
    pll_freq = int((signal_freq + RX_LO - offset) // 4)
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
