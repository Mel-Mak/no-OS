#ifndef IIO_CAPTURE_H
#define IIO_CAPTURE_H
#include <stddef.h>
#include <stdint.h>

struct iio_capture_config {
    const char *uri;
    int64_t sample_rate_hz, rx_lo_hz, rf_bandwidth_hz;
    double rx0_gain_db, rx1_gain_db;
    size_t samples;
    unsigned int discarded_buffers;
};
struct iio_complex_sample { int16_t i, q; };
struct iio_capture_data {
    struct iio_complex_sample *rx0, *rx1;
    size_t sample_count;
};
struct iio_capture;

int iio_capture_open(struct iio_capture **capture,
                     const struct iio_capture_config *config);
int iio_capture_read(struct iio_capture *capture,
                     struct iio_capture_data *data, size_t samples);
int iio_capture_write_csv(const struct iio_capture_data *data,
                          const char *csv_path);
void iio_capture_data_free(struct iio_capture_data *data);
void iio_capture_close(struct iio_capture *capture);
#endif
