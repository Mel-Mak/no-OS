#include "iio_capture.h"

#include <errno.h>
#include <iio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct iio_capture {
    struct iio_context *ctx;
    struct iio_device *phy;
    struct iio_device *rxdev;
    struct iio_channel *rx0_phy;
    struct iio_channel *rx1_phy;
    struct iio_channel *i0;
    struct iio_channel *q0;
    struct iio_channel *i1;
    struct iio_channel *q1;
    struct iio_buffer *buffer;
    size_t buffer_samples;
    unsigned int discarded_buffers;
};

static int write_ll(struct iio_channel *channel, const char *attribute,
                    long long value)
{
    int ret = (int)iio_channel_attr_write_longlong(channel, attribute, value);
    if (ret < 0)
        fprintf(stderr, "libiio: cannot write %s: %s\n", attribute,
                strerror(-ret));
    return ret < 0 ? ret : 0;
}

static int write_string(struct iio_channel *channel, const char *attribute,
                        const char *value)
{
    int ret = (int)iio_channel_attr_write(channel, attribute, value);
    if (ret < 0)
        fprintf(stderr, "libiio: cannot write %s: %s\n", attribute,
                strerror(-ret));
    return ret < 0 ? ret : 0;
}

static struct iio_channel *find_phy_rx(struct iio_device *phy,
                                       const char *name)
{
    return iio_device_find_channel(phy, name, false);
}

static struct iio_channel *find_scan(struct iio_device *dev,
                                      const char *name)
{
    return iio_device_find_channel(dev, name, false);
}

static int configure_phy_channel(struct iio_channel *ch, int64_t sample_rate,
                                 int64_t bandwidth, double gain_db)
{
    char gain[32];
    int ret;

    if (!ch)
        return -ENODEV;

    ret = write_string(ch, "rf_port_select", "A_BALANCED");
    if (ret)
        return ret;
    ret = write_ll(ch, "sampling_frequency", sample_rate);
    if (ret)
        return ret;
    ret = write_ll(ch, "rf_bandwidth", bandwidth);
    if (ret)
        return ret;
    ret = write_string(ch, "gain_control_mode", "manual");
    if (ret)
        return ret;

    snprintf(gain, sizeof(gain), "%.3f", gain_db);
    return write_string(ch, "hardwaregain", gain);
}

static int create_rx_buffer(struct iio_capture *cap, size_t samples)
{
    if (cap->buffer) {
        iio_buffer_destroy(cap->buffer);
        cap->buffer = NULL;
    }

    cap->buffer = iio_device_create_buffer(cap->rxdev, samples, false);
    if (!cap->buffer) {
        int err = errno ? -errno : -EIO;
        fprintf(stderr, "libiio: RX buffer creation failed: %s\n",
                strerror(-err));
        return err;
    }

    cap->buffer_samples = samples;
    return 0;
}

int iio_capture_open(struct iio_capture **out,
                     const struct iio_capture_config *cfg)
{
    struct iio_capture *cap;
    struct iio_channel *lo;
    int ret;

    if (!out || !cfg || !cfg->uri || !cfg->samples)
        return -EINVAL;

    cap = calloc(1, sizeof(*cap));
    if (!cap)
        return -ENOMEM;

    cap->ctx = iio_create_context_from_uri(cfg->uri);
    if (!cap->ctx) {
        ret = errno ? -errno : -ENODEV;
        fprintf(stderr, "libiio: cannot create context for %s: %s\n",
                cfg->uri, strerror(-ret));
        goto fail;
    }

    cap->phy = iio_context_find_device(cap->ctx, "ad9361-phy");
    cap->rxdev = iio_context_find_device(cap->ctx, "cf-ad9361-lpc");
    if (!cap->phy || !cap->rxdev) {
        fprintf(stderr, "libiio: ad9361-phy or cf-ad9361-lpc not found\n");
        ret = -ENODEV;
        goto fail;
    }

    cap->rx0_phy = find_phy_rx(cap->phy, "voltage0");
    cap->rx1_phy = find_phy_rx(cap->phy, "voltage1");
    lo = iio_device_find_channel(cap->phy, "altvoltage0", true);

    cap->i0 = find_scan(cap->rxdev, "voltage0");
    cap->q0 = find_scan(cap->rxdev, "voltage1");
    cap->i1 = find_scan(cap->rxdev, "voltage2");
    cap->q1 = find_scan(cap->rxdev, "voltage3");

    if (!cap->rx0_phy || !cap->rx1_phy || !lo || !cap->i0 || !cap->q0 ||
        !cap->i1 || !cap->q1) {
        fprintf(stderr, "libiio: required AD9361 RX channels were not found\n");
        ret = -ENODEV;
        goto fail;
    }

    ret = configure_phy_channel(cap->rx0_phy, cfg->sample_rate_hz,
                                cfg->rf_bandwidth_hz, cfg->rx0_gain_db);
    if (ret)
        goto fail;
    ret = configure_phy_channel(cap->rx1_phy, cfg->sample_rate_hz,
                                cfg->rf_bandwidth_hz, cfg->rx1_gain_db);
    if (ret)
        goto fail;
    ret = write_ll(lo, "frequency", cfg->rx_lo_hz);
    if (ret)
        goto fail;

    iio_channel_enable(cap->i0);
    iio_channel_enable(cap->q0);
    iio_channel_enable(cap->i1);
    iio_channel_enable(cap->q1);

    cap->discarded_buffers = cfg->discarded_buffers;
    ret = create_rx_buffer(cap, cfg->samples);
    if (ret)
        goto fail;

    *out = cap;
    return 0;

fail:
    iio_capture_close(cap);
    return ret;
}

static int16_t converted_sample(struct iio_channel *channel, const void *src)
{
    int16_t value = 0;
    iio_channel_convert(channel, &value, src);
    return value;
}

int iio_capture_to_csv(struct iio_capture *cap, const char *csv_path,
                       size_t samples)
{
    FILE *csv = NULL;
    ptrdiff_t step;
    char *p_i0, *p_q0, *p_i1, *p_q1;
    char *end;
    size_t index = 0;
    unsigned int n;
    ssize_t bytes;
    int ret = 0;

    if (!cap || !csv_path || !samples)
        return -EINVAL;

    if (!cap->buffer || cap->buffer_samples != samples) {
        ret = create_rx_buffer(cap, samples);
        if (ret)
            return ret;
    }

    for (n = 0; n < cap->discarded_buffers; n++) {
        bytes = iio_buffer_refill(cap->buffer);
        if (bytes < 0) {
            fprintf(stderr, "libiio: discard refill failed: %s\n",
                    strerror((int)-bytes));
            return (int)bytes;
        }
    }

    bytes = iio_buffer_refill(cap->buffer);
    if (bytes < 0) {
        fprintf(stderr, "libiio: capture refill failed: %s\n",
                strerror((int)-bytes));
        return (int)bytes;
    }

    csv = fopen(csv_path, "w");
    if (!csv) {
        ret = -errno;
        fprintf(stderr, "Cannot open %s: %s\n", csv_path, strerror(errno));
        return ret;
    }

    fprintf(csv, "sample,rx0_i,rx0_q,rx1_i,rx1_q\n");
    step = iio_buffer_step(cap->buffer);
    end = (char *)iio_buffer_end(cap->buffer);
    p_i0 = (char *)iio_buffer_first(cap->buffer, cap->i0);
    p_q0 = (char *)iio_buffer_first(cap->buffer, cap->q0);
    p_i1 = (char *)iio_buffer_first(cap->buffer, cap->i1);
    p_q1 = (char *)iio_buffer_first(cap->buffer, cap->q1);

    while (index < samples && p_i0 < end && p_q0 < end &&
           p_i1 < end && p_q1 < end) {
        int16_t i0 = converted_sample(cap->i0, p_i0);
        int16_t q0 = converted_sample(cap->q0, p_q0);
        int16_t i1 = converted_sample(cap->i1, p_i1);
        int16_t q1 = converted_sample(cap->q1, p_q1);

        if (fprintf(csv, "%zu,%d,%d,%d,%d\n", index, i0, q0, i1, q1) < 0) {
            ret = -EIO;
            break;
        }

        p_i0 += step;
        p_q0 += step;
        p_i1 += step;
        p_q1 += step;
        index++;
    }

    if (fclose(csv) != 0 && !ret)
        ret = -errno;

    if (!ret && index != samples) {
        fprintf(stderr, "Expected %zu samples but wrote %zu\n", samples, index);
        ret = -EIO;
    }

    if (!ret)
        printf("Captured %zu dual-channel IQ samples to %s\n", index, csv_path);
    return ret;
}

void iio_capture_close(struct iio_capture *cap)
{
    if (!cap)
        return;
    if (cap->buffer)
        iio_buffer_destroy(cap->buffer);
    if (cap->i0)
        iio_channel_disable(cap->i0);
    if (cap->q0)
        iio_channel_disable(cap->q0);
    if (cap->i1)
        iio_channel_disable(cap->i1);
    if (cap->q1)
        iio_channel_disable(cap->q1);
    if (cap->ctx)
        iio_context_destroy(cap->ctx);
    free(cap);
}
