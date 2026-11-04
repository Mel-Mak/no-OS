#include "iio_capture.h"
#include <errno.h>
#include <iio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct iio_capture {
    struct iio_context *ctx;
    struct iio_device *phy, *rxdev;
    struct iio_channel *i0, *q0, *i1, *q1;
    struct iio_buffer *buffer;
    size_t buffer_samples;
    unsigned int discarded_buffers;
};

static int write_ll(struct iio_channel *ch, const char *attr, long long value)
{
    int ret = (int)iio_channel_attr_write_longlong(ch, attr, value);
    if (ret < 0) fprintf(stderr, "libiio: cannot write %s: %s\n", attr, strerror(-ret));
    return ret < 0 ? ret : 0;
}
static int write_str(struct iio_channel *ch, const char *attr, const char *value)
{
    int ret = (int)iio_channel_attr_write(ch, attr, value);
    if (ret < 0) fprintf(stderr, "libiio: cannot write %s: %s\n", attr, strerror(-ret));
    return ret < 0 ? ret : 0;
}
static int configure_phy(struct iio_channel *ch, int64_t fs, int64_t bw, double gain)
{
    char text[32]; int ret;
    if (!ch) return -ENODEV;
    if ((ret = write_str(ch, "rf_port_select", "A_BALANCED"))) return ret;
    if ((ret = write_ll(ch, "sampling_frequency", fs))) return ret;
    if ((ret = write_ll(ch, "rf_bandwidth", bw))) return ret;
    if ((ret = write_str(ch, "gain_control_mode", "manual"))) return ret;
    snprintf(text, sizeof(text), "%.3f", gain);
    return write_str(ch, "hardwaregain", text);
}
static int create_buffer(struct iio_capture *cap, size_t samples)
{
    if (cap->buffer) { iio_buffer_destroy(cap->buffer); cap->buffer = NULL; }
    cap->buffer = iio_device_create_buffer(cap->rxdev, samples, false);
    if (!cap->buffer) return errno ? -errno : -EIO;
    cap->buffer_samples = samples;
    return 0;
}

int iio_capture_open(struct iio_capture **out,
                     const struct iio_capture_config *cfg)
{
    struct iio_capture *cap;
    struct iio_channel *rx0, *rx1, *lo, *tx0, *tx1;
    int ret;
    if (!out || !cfg || !cfg->uri || !cfg->samples) return -EINVAL;
    cap = calloc(1, sizeof(*cap));
    if (!cap) return -ENOMEM;
    cap->ctx = iio_create_context_from_uri(cfg->uri);
    if (!cap->ctx) { ret = errno ? -errno : -ENODEV; goto fail; }
    cap->phy = iio_context_find_device(cap->ctx, "ad9361-phy");
    cap->rxdev = iio_context_find_device(cap->ctx, "cf-ad9361-lpc");
    if (!cap->phy || !cap->rxdev) { ret = -ENODEV; goto fail; }
    rx0 = iio_device_find_channel(cap->phy, "voltage0", false);
    rx1 = iio_device_find_channel(cap->phy, "voltage1", false);
    lo = iio_device_find_channel(cap->phy, "altvoltage0", true);
    cap->i0 = iio_device_find_channel(cap->rxdev, "voltage0", false);
    cap->q0 = iio_device_find_channel(cap->rxdev, "voltage1", false);
    cap->i1 = iio_device_find_channel(cap->rxdev, "voltage2", false);
    cap->q1 = iio_device_find_channel(cap->rxdev, "voltage3", false);
    if (!rx0 || !rx1 || !lo || !cap->i0 || !cap->q0 || !cap->i1 || !cap->q1) {
        ret = -ENODEV; goto fail;
    }
    if ((ret = configure_phy(rx0, cfg->sample_rate_hz, cfg->rf_bandwidth_hz,
                             cfg->rx0_gain_db))) goto fail;
    if ((ret = configure_phy(rx1, cfg->sample_rate_hz, cfg->rf_bandwidth_hz,
                             cfg->rx1_gain_db))) goto fail;
    if ((ret = write_ll(lo, "frequency", cfg->rx_lo_hz))) goto fail;
    tx0 = iio_device_find_channel(cap->phy, "voltage0", true);
    tx1 = iio_device_find_channel(cap->phy, "voltage1", true);
    if (tx0) write_str(tx0, "hardwaregain", "-88.000");
    if (tx1) write_str(tx1, "hardwaregain", "-88.000");
    iio_channel_enable(cap->i0); iio_channel_enable(cap->q0);
    iio_channel_enable(cap->i1); iio_channel_enable(cap->q1);
    cap->discarded_buffers = cfg->discarded_buffers;
    if ((ret = create_buffer(cap, cfg->samples))) goto fail;
    *out = cap;
    return 0;
fail:
    iio_capture_close(cap);
    return ret;
}

static int16_t convert_sample(struct iio_channel *ch, const void *src)
{
    int16_t value = 0;
    iio_channel_convert(ch, &value, src);
    return value;
}

int iio_capture_read(struct iio_capture *cap,
                     struct iio_capture_data *data, size_t samples)
{
    char *p0i, *p0q, *p1i, *p1q, *end;
    ptrdiff_t step;
    size_t index = 0;
    unsigned int n;
    ssize_t bytes;
    int ret;
    if (!cap || !data || !samples) return -EINVAL;
    *data = (struct iio_capture_data){0};
    data->rx0 = calloc(samples, sizeof(*data->rx0));
    data->rx1 = calloc(samples, sizeof(*data->rx1));
    if (!data->rx0 || !data->rx1) { iio_capture_data_free(data); return -ENOMEM; }
    if (!cap->buffer || cap->buffer_samples != samples)
        if ((ret = create_buffer(cap, samples))) { iio_capture_data_free(data); return ret; }
    for (n = 0; n < cap->discarded_buffers; n++) {
        bytes = iio_buffer_refill(cap->buffer);
        if (bytes < 0) { iio_capture_data_free(data); return (int)bytes; }
    }
    bytes = iio_buffer_refill(cap->buffer);
    if (bytes < 0) { iio_capture_data_free(data); return (int)bytes; }
    step = iio_buffer_step(cap->buffer);
    end = (char *)iio_buffer_end(cap->buffer);
    p0i = (char *)iio_buffer_first(cap->buffer, cap->i0);
    p0q = (char *)iio_buffer_first(cap->buffer, cap->q0);
    p1i = (char *)iio_buffer_first(cap->buffer, cap->i1);
    p1q = (char *)iio_buffer_first(cap->buffer, cap->q1);
    while (index < samples && p0i < end && p0q < end && p1i < end && p1q < end) {
        data->rx0[index].i = convert_sample(cap->i0, p0i);
        data->rx0[index].q = convert_sample(cap->q0, p0q);
        data->rx1[index].i = convert_sample(cap->i1, p1i);
        data->rx1[index].q = convert_sample(cap->q1, p1q);
        p0i += step; p0q += step; p1i += step; p1q += step; index++;
    }
    data->sample_count = index;
    if (index != samples) { iio_capture_data_free(data); return -EIO; }
    return 0;
}

int iio_capture_write_csv(const struct iio_capture_data *data, const char *path)
{
    FILE *f;
    size_t i;
    if (!data || !data->rx0 || !data->rx1 || !path) return -EINVAL;
    f = fopen(path, "w");
    if (!f) return -errno;
    fprintf(f, "sample,rx0_i,rx0_q,rx1_i,rx1_q\n");
    for (i = 0; i < data->sample_count; i++)
        if (fprintf(f, "%zu,%d,%d,%d,%d\n", i, data->rx0[i].i,
                    data->rx0[i].q, data->rx1[i].i, data->rx1[i].q) < 0) {
            fclose(f); return -EIO;
        }
    return fclose(f) == 0 ? 0 : -errno;
}

void iio_capture_data_free(struct iio_capture_data *data)
{
    if (!data) return;
    free(data->rx0); free(data->rx1);
    *data = (struct iio_capture_data){0};
}

void iio_capture_close(struct iio_capture *cap)
{
    if (!cap) return;
    if (cap->buffer) iio_buffer_destroy(cap->buffer);
    if (cap->i0) iio_channel_disable(cap->i0);
    if (cap->q0) iio_channel_disable(cap->q0);
    if (cap->i1) iio_channel_disable(cap->i1);
    if (cap->q1) iio_channel_disable(cap->q1);
    if (cap->ctx) iio_context_destroy(cap->ctx);
    free(cap);
}
