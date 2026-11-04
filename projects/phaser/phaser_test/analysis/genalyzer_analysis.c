#include "genalyzer_analysis.h"
#include <cgenalyzer.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ADC_BITS 12
#define CFG_KEY "phaser"

static double wrap_phase(double x)
{
    while (x > 180.0) x -= 360.0;
    while (x <= -180.0) x += 360.0;
    return x;
}

static int analyze_channel(const struct iio_complex_sample *x, size_t n,
                           double fs, unsigned int max_harmonics,
                           struct channel_analysis *r)
{
    int16_t *idata = NULL, *qdata = NULL;
    double *fft_out = NULL;
    char **rkeys = NULL;
    double *rvalues = NULL;
    size_t *key_sizes = NULL;
    size_t fft_out_size = 0, nresults = 0;
    size_t i;
    int ret;

    if (!x || !r || n < 8 || fs <= 0.0)
        return -EINVAL;

    idata = malloc(n * sizeof(*idata));
    qdata = malloc(n * sizeof(*qdata));
    if (!idata || !qdata) { ret = -ENOMEM; goto cleanup; }

    for (i = 0; i < n; i++) {
        idata[i] = x[i].i;
        qdata[i] = x[i].q;
    }

    ret = gn_fft_size(&fft_out_size, n, n, 1, n);
    if (ret) goto cleanup;

    fft_out = malloc(fft_out_size * sizeof(*fft_out));
    if (!fft_out) { ret = -ENOMEM; goto cleanup; }

    ret = gn_fft16(fft_out, fft_out_size,
                   idata, n, qdata, n,
                   ADC_BITS, 1, n,
                   GnWindowBlackmanHarris,
                   GnCodeFormatTwosComplement);
    if (ret) goto cleanup;

    gn_mgr_remove(CFG_KEY);
    ret = gn_fa_create(CFG_KEY);
    if (ret) goto cleanup;

    gn_fa_fsample(CFG_KEY, fs);
    gn_fa_max_tone(CFG_KEY, "sig", GnFACompTagSignal, 0);
    gn_fa_hd(CFG_KEY, (int)max_harmonics);

    ret = gn_fft_analysis_results_size(&nresults, CFG_KEY, fft_out_size, n);
    if (ret) goto cleanup;

    key_sizes = malloc(nresults * sizeof(*key_sizes));
    rkeys = calloc(nresults, sizeof(*rkeys));
    rvalues = malloc(nresults * sizeof(*rvalues));
    if (!key_sizes || !rkeys || !rvalues) { ret = -ENOMEM; goto cleanup; }

    ret = gn_fft_analysis_results_key_sizes(key_sizes, nresults,
                                            CFG_KEY, fft_out_size, n);
    if (ret) goto cleanup;

    for (i = 0; i < nresults; i++) {
        rkeys[i] = malloc(key_sizes[i]);
        if (!rkeys[i]) { ret = -ENOMEM; goto cleanup; }
    }

    ret = gn_fft_analysis(rkeys, nresults, rvalues, nresults,
                          CFG_KEY, fft_out, fft_out_size, n,
                          GnFreqAxisTypeDcCenter);
    if (ret) goto cleanup;

    gn_fa_result(&r->fundamental_hz, (const char **)rkeys, nresults,
                 rvalues, nresults, "sig:freq");
    gn_fa_result(&r->magnitude_dbfs, (const char **)rkeys, nresults,
                 rvalues, nresults, "sig:mag_dbfs");
    gn_fa_result(&r->phase_deg, (const char **)rkeys, nresults,
                 rvalues, nresults, "sig:phase");
    gn_fa_result(&r->snr_db, (const char **)rkeys, nresults,
                 rvalues, nresults, "snr");
    gn_fa_result(&r->sfdr_db, (const char **)rkeys, nresults,
                 rvalues, nresults, "sfdr");

    r->phase_deg = r->phase_deg * 180.0 / M_PI;
    ret = 0;

cleanup:
    gn_mgr_remove(CFG_KEY);
    free(idata);
    free(qdata);
    free(fft_out);
    free(key_sizes);
    if (rkeys) {
        for (i = 0; i < nresults; i++)
            free(rkeys[i]);
        free(rkeys);
    }
    free(rvalues);
    return ret;
}

int genalyzer_analyze_iq(const struct iio_capture_data *cap,
                         const struct genalyzer_config *cfg,
                         struct genalyzer_result *r)
{
    int ret;
    if (!cap || !cfg || !r || cfg->fft_size != cap->sample_count) return -EINVAL;
    if ((ret = analyze_channel(cap->rx0, cap->sample_count,
                               cfg->sample_rate_hz, cfg->max_harmonics,
                               &r->rx0))) return ret;
    if ((ret = analyze_channel(cap->rx1, cap->sample_count,
                               cfg->sample_rate_hz, cfg->max_harmonics,
                               &r->rx1))) return ret;
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
