#include "genalyzer_analysis.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Analysis adapter. Replace analyze_channel() with calls matching the installed
 * Genalyzer C API version; the public interface and capture path stay unchanged. */
static double wrap_phase(double x)
{
    while (x > 180.0) x -= 360.0;
    while (x <= -180.0) x += 360.0;
    return x;
}

static int analyze_channel(const struct iio_complex_sample *x, size_t n,
                           double fs, struct channel_analysis *r)
{
    size_t k, m, peak = 1;
    double peak_power = 0.0, second_power = 0.0, total_power = 0.0;
    double peak_re = 0.0, peak_im = 0.0;
    if (!x || !r || n < 8 || fs <= 0.0) return -EINVAL;
    for (k = 1; k < n; k++) {
        double re = 0.0, im = 0.0;
        for (m = 0; m < n; m++) {
            double window = 0.5 - 0.5 * cos(2.0 * M_PI * m / (n - 1));
            double angle = -2.0 * M_PI * k * m / n;
            double i = x[m].i * window, q = x[m].q * window;
            re += i * cos(angle) - q * sin(angle);
            im += i * sin(angle) + q * cos(angle);
        }
        {
            double power = re * re + im * im;
            total_power += power;
            if (power > peak_power) {
                second_power = peak_power;
                peak_power = power; peak = k; peak_re = re; peak_im = im;
            } else if (power > second_power) {
                second_power = power;
            }
        }
    }
    r->fundamental_hz = peak <= n / 2 ? peak * fs / n : ((double)peak - n) * fs / n;
    r->magnitude_dbfs = 10.0 * log10(peak_power / ((double)n * n * 2048.0 * 2048.0) + 1e-30);
    r->phase_deg = atan2(peak_im, peak_re) * 180.0 / M_PI;
    r->snr_db = 10.0 * log10(peak_power / fmax(total_power - peak_power, 1e-30));
    r->sfdr_db = 10.0 * log10(peak_power / fmax(second_power, 1e-30));
    return 0;
}

int genalyzer_analyze_iq(const struct iio_capture_data *cap,
                         const struct genalyzer_config *cfg,
                         struct genalyzer_result *r)
{
    int ret;
    if (!cap || !cfg || !r || cfg->fft_size != cap->sample_count) return -EINVAL;
    if ((ret = analyze_channel(cap->rx0, cap->sample_count,
                               cfg->sample_rate_hz, &r->rx0))) return ret;
    if ((ret = analyze_channel(cap->rx1, cap->sample_count,
                               cfg->sample_rate_hz, &r->rx1))) return ret;
    r->relative_phase_deg = wrap_phase(r->rx1.phase_deg - r->rx0.phase_deg);
    r->relative_gain_db = r->rx1.magnitude_dbfs - r->rx0.magnitude_dbfs;
    return 0;
}

void genalyzer_print_result(const struct genalyzer_result *r)
{
    if (!r) return;
    printf("RX0: tone=%+.3f Hz mag=%.2f dBFS phase=%.2f deg SNR=%.2f dB SFDR=%.2f dB\n",
           r->rx0.fundamental_hz, r->rx0.magnitude_dbfs, r->rx0.phase_deg,
           r->rx0.snr_db, r->rx0.sfdr_db);
    printf("RX1: tone=%+.3f Hz mag=%.2f dBFS phase=%.2f deg SNR=%.2f dB SFDR=%.2f dB\n",
           r->rx1.fundamental_hz, r->rx1.magnitude_dbfs, r->rx1.phase_deg,
           r->rx1.snr_db, r->rx1.sfdr_db);
    printf("Relative: gain=%.2f dB phase=%.2f deg\n",
           r->relative_gain_db, r->relative_phase_deg);
}

int genalyzer_write_json(const struct genalyzer_result *r, const char *path)
{
    FILE *f;
    if (!r || !path) return -EINVAL;
    f = fopen(path, "w");
    if (!f) return -errno;
    fprintf(f, "{\n  \"rx0\": {\"tone_hz\": %.12g, \"magnitude_dbfs\": %.12g, "
               "\"phase_deg\": %.12g, \"snr_db\": %.12g, \"sfdr_db\": %.12g},\n",
            r->rx0.fundamental_hz, r->rx0.magnitude_dbfs, r->rx0.phase_deg,
            r->rx0.snr_db, r->rx0.sfdr_db);
    fprintf(f, "  \"rx1\": {\"tone_hz\": %.12g, \"magnitude_dbfs\": %.12g, "
               "\"phase_deg\": %.12g, \"snr_db\": %.12g, \"sfdr_db\": %.12g},\n",
            r->rx1.fundamental_hz, r->rx1.magnitude_dbfs, r->rx1.phase_deg,
            r->rx1.snr_db, r->rx1.sfdr_db);
    fprintf(f, "  \"relative_gain_db\": %.12g,\n  \"relative_phase_deg\": %.12g\n}\n",
            r->relative_gain_db, r->relative_phase_deg);
    return fclose(f) == 0 ? 0 : -errno;
}
