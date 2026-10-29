#ifndef IIO_CAPTURE_H
#define IIO_CAPTURE_H

#include <stddef.h>
#include <stdint.h>

struct iio_capture_config {
    const char *uri;
    int64_t sample_rate_hz;
    int64_t rx_lo_hz;
    int64_t rf_bandwidth_hz;
    double rx0_gain_db;
    double rx1_gain_db;
    size_t samples;
    unsigned int discarded_buffers;
};

struct iio_capture;

int iio_capture_open(struct iio_capture **capture,
                     const struct iio_capture_config *config);
int iio_capture_to_csv(struct iio_capture *capture,
                       const char *csv_path,
                       size_t samples);
void iio_capture_close(struct iio_capture *capture);

#endif
