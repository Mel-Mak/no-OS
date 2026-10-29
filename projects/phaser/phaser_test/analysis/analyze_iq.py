#!/usr/bin/env python3
import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from numpy.fft import fft, fftfreq, fftshift
from scipy import signal


def spectrum(x, sample_rate, reference=2**11):
    n = len(x)
    window = signal.windows.kaiser(n, beta=14)
    coherent_gain = np.mean(window)
    amplitude = np.abs(fftshift(fft(x * window))) / (n * coherent_gain)
    dbfs = 20.0 * np.log10(np.maximum(amplitude / reference, 1e-20))
    frequencies = fftshift(fftfreq(n, 1.0 / sample_rate))
    return dbfs, frequencies


def rms(x):
    return np.sqrt(np.mean(np.abs(x) ** 2))


def tone_phasor(x, tone_hz, sample_rate):
    n = np.arange(len(x))
    mixer = np.exp(-1j * 2.0 * np.pi * tone_hz * n / sample_rate)
    window = signal.windows.hann(len(x), sym=False)
    return np.mean(x * mixer * window) / np.mean(window)


def detect_tone(x, expected_hz, sample_rate, search_hz):
    dbfs, frequencies = spectrum(x, sample_rate)
    mask = np.abs(frequencies - expected_hz) <= search_hz
    if not np.any(mask):
        raise RuntimeError("Expected tone is outside the capture bandwidth")
    candidates = np.flatnonzero(mask)
    index = candidates[np.argmax(dbfs[mask])]
    return frequencies[index], dbfs[index]


def load_csv(path):
    data = np.genfromtxt(path, delimiter=",", names=True, dtype=np.float64)
    required = {"sample", "rx0_i", "rx0_q", "rx1_i", "rx1_q"}
    if not data.dtype.names or not required.issubset(data.dtype.names):
        raise ValueError(f"CSV must contain: {', '.join(sorted(required))}")
    ch0 = data["rx0_i"] + 1j * data["rx0_q"]
    ch1 = data["rx1_i"] + 1j * data["rx1_q"]
    return data["sample"], ch0, ch1


def main():
    parser = argparse.ArgumentParser(description="Analyze dual-RX Pluto IQ CSV")
    parser.add_argument("csv", type=Path)
    parser.add_argument("--sample-rate", type=float, default=30_000_000)
    parser.add_argument("--expected-hz", type=float, default=-1_000_000)
    parser.add_argument("--search-hz", type=float, default=2_000_000)
    parser.add_argument("--save", type=Path, help="Save the figure instead of only displaying it")
    parser.add_argument("--no-show", action="store_true")
    args = parser.parse_args()

    sample, ch0, ch1 = load_csv(args.csv)
    summed = ch0 + ch1
    tone_hz, tone_dbfs = detect_tone(summed, args.expected_hz,
                                     args.sample_rate, args.search_hz)
    p0 = tone_phasor(ch0, tone_hz, args.sample_rate)
    p1 = tone_phasor(ch1, tone_hz, args.sample_rate)
    relative_phase = np.degrees(np.angle(p1 * np.conj(p0)))

    print(f"File: {args.csv}")
    print(f"Samples: {len(ch0)}")
    for name, channel in (("RX0", ch0), ("RX1", ch1), ("SUM", summed)):
        print(f"{name}: peak={np.max(np.abs(channel)):.3f}, RMS={rms(channel):.3f}")
    print(f"Detected tone: {tone_hz / 1e6:.6f} MHz ({tone_dbfs:.2f} dBFS)")
    print(f"RX1 relative to RX0: {relative_phase:.3f} degrees")
    print(f"RX0 tone magnitude: {20*np.log10(max(abs(p0), 1e-20)):.3f} dB")
    print(f"RX1 tone magnitude: {20*np.log10(max(abs(p1), 1e-20)):.3f} dB")

    db0, frequencies = spectrum(ch0, args.sample_rate)
    db1, _ = spectrum(ch1, args.sample_rate)
    dbsum, _ = spectrum(summed, args.sample_rate)
    mhz = frequencies / 1e6

    fig, axes = plt.subplots(3, 1, figsize=(11, 9))
    axes[0].plot(sample, ch0.real, label="RX0 I", alpha=0.75)
    axes[0].plot(sample, ch1.real, label="RX1 I", alpha=0.75)
    axes[0].set(title="Time-domain IQ capture", xlabel="Sample", ylabel="ADC code")
    axes[0].legend()
    axes[1].plot(mhz, db0, label="RX0")
    axes[1].plot(mhz, db1, label="RX1")
    axes[1].set(title="Per-channel spectrum", xlabel="Frequency [MHz]", ylabel="dBFS")
    axes[1].legend()
    axes[2].plot(mhz, dbsum, color="green", label="RX0 + RX1")
    axes[2].axvline(args.expected_hz / 1e6, color="red", linestyle="--",
                    label="Expected tone")
    axes[2].set(title=f"Sum spectrum, peak {tone_hz/1e6:.3f} MHz",
                xlabel="Frequency [MHz]", ylabel="dBFS")
    axes[2].legend()
    fig.tight_layout()

    if args.save:
        fig.savefig(args.save, dpi=150)
        print(f"Saved plot: {args.save}")
    if not args.no_show:
        plt.show()


if __name__ == "__main__":
    main()
