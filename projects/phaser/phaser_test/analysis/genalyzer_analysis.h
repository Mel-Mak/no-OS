#ifndef GENALYZER_ANALYSIS_H
#define GENALYZER_ANALYSIS_H
#include <stddef.h>
#include "iio_capture.h"

struct genalyzer_config {
    double sample_rate_hz;
    size_t fft_size;
    unsigned int max_harmonics;
};
struct channel_analysis {
    double fundamental_hz, magnitude_dbfs, phase_deg, snr_db, sfdr_db;
};
struct genalyzer_result {
    struct channel_analysis rx0, rx1;
    double relative_phase_deg, relative_gain_db;
};
int genalyzer_analyze_iq(const struct iio_capture_data *capture,
                         const struct genalyzer_config *config,
                         struct genalyzer_result *result);
void genalyzer_print_result(const struct genalyzer_result *result);
int genalyzer_write_json(const struct genalyzer_result *result,
                         const char *path);
#endif
