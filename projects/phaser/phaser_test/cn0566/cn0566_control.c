#include "cn0566_control.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "adf4159_cfg.h"
#include "no_os_gpio.h"
#include "parameters.h"

struct board_gpios {
    struct no_os_gpio_desc *vctrl1, *vctrl2, *div_mr, *div_s0, *div_s1;
    struct no_os_gpio_desc *div_s2, *rx_load, *tr, *tx_sw, *muxout, *burst;
};

struct cn0566_dev {
    struct adar1000_dev *adar[CN0566_NUM_ADAR1000];
    struct adf4159_dev *pll;
    struct board_gpios gpios;
};

static int32_t gpio_open(struct no_os_gpio_desc **desc, uint32_t number)
{
    struct no_os_gpio_init_param p = {
        .number = number, .platform_ops = GPIO_OPS, .extra = NULL
    };
    return no_os_gpio_get(desc, &p);
}

static void gpio_remove_all(struct board_gpios *g)
{
    struct no_os_gpio_desc *all[] = {
        g->vctrl1, g->vctrl2, g->div_mr, g->div_s0, g->div_s1,
        g->div_s2, g->rx_load, g->tr, g->tx_sw, g->muxout, g->burst
    };
    size_t i;
    for (i = 0; i < sizeof(all) / sizeof(all[0]); i++)
        if (all[i]) no_os_gpio_remove(all[i]);
}

static int32_t gpio_init_all(struct board_gpios *g)
{
    int32_t ret;
#define GET(m, n) do { ret = gpio_open(&g->m, n); if (ret) goto fail; } while (0)
#define OUT(m, v) do { ret = no_os_gpio_direction_output(g->m, v); if (ret) goto fail; } while (0)
    GET(vctrl1, GPIO_VCTRL_1); GET(vctrl2, GPIO_VCTRL_2);
    GET(div_mr, GPIO_DIV_MR); GET(div_s0, GPIO_DIV_S0);
    GET(div_s1, GPIO_DIV_S1); GET(div_s2, GPIO_DIV_S2);
    GET(rx_load, GPIO_RX_LOAD); GET(tr, GPIO_TR);
    GET(tx_sw, GPIO_TX_SW); GET(muxout, GPIO_MUXOUT); GET(burst, GPIO_BURST);
    OUT(burst, GPIO_LEVEL_BURST_RX); OUT(vctrl1, GPIO_LEVEL_VCTRL_1_RX);
    OUT(vctrl2, GPIO_LEVEL_VCTRL_2_RX); OUT(div_mr, GPIO_LEVEL_DIV_MR_RX);
    OUT(div_s0, GPIO_LEVEL_DIV_S0_RX); OUT(div_s1, GPIO_LEVEL_DIV_S1_RX);
    OUT(div_s2, GPIO_LEVEL_DIV_S2_RX); OUT(rx_load, GPIO_LEVEL_RX_LOAD_IDLE);
    OUT(tr, GPIO_LEVEL_TR_RX); OUT(tx_sw, GPIO_LEVEL_TX_SW_RX);
    ret = no_os_gpio_direction_input(g->muxout);
    if (ret) goto fail;
#undef GET
#undef OUT
    return 0;
fail:
#undef GET
#undef OUT
    gpio_remove_all(g);
    *g = (struct board_gpios){0};
    return ret;
}

static int32_t show_gpio(const char *name, uint32_t pin,
                         struct no_os_gpio_desc *desc, int expected)
{
    uint8_t value = 0;
    int32_t ret = no_os_gpio_get_value(desc, &value);
    if (ret) return ret;
    printf("  %-8s GPIO%-2u = %u", name, pin, value);
    if (expected >= 0)
        printf(" %s", value == (uint8_t)expected ? "OK" : "MISMATCH");
    putchar('\n');
    return 0;
}

int32_t cn0566_verify_gpios(struct cn0566_dev *dev)
{
    struct board_gpios *g;
    int32_t status = 0;
    if (!dev) return -EINVAL;
    g = &dev->gpios;
#define SHOW(n, p, m, e) do { if (show_gpio(n, p, g->m, e)) status = -EIO; } while (0)
    SHOW("BURST", GPIO_BURST, burst, GPIO_LEVEL_BURST_RX);
    SHOW("VCTRL_1", GPIO_VCTRL_1, vctrl1, GPIO_LEVEL_VCTRL_1_RX);
    SHOW("VCTRL_2", GPIO_VCTRL_2, vctrl2, GPIO_LEVEL_VCTRL_2_RX);
    SHOW("DIV_MR", GPIO_DIV_MR, div_mr, GPIO_LEVEL_DIV_MR_RX);
    SHOW("DIV_S0", GPIO_DIV_S0, div_s0, GPIO_LEVEL_DIV_S0_RX);
    SHOW("DIV_S1", GPIO_DIV_S1, div_s1, GPIO_LEVEL_DIV_S1_RX);
    SHOW("DIV_S2", GPIO_DIV_S2, div_s2, GPIO_LEVEL_DIV_S2_RX);
    SHOW("RX_LOAD", GPIO_RX_LOAD, rx_load, GPIO_LEVEL_RX_LOAD_IDLE);
    SHOW("TR", GPIO_TR, tr, GPIO_LEVEL_TR_RX);
    SHOW("TX_SW", GPIO_TX_SW, tx_sw, GPIO_LEVEL_TX_SW_RX);
    SHOW("MUXOUT", GPIO_MUXOUT, muxout, -1);
#undef SHOW
    return status;
}

static int32_t init_adar(struct adar1000_dev **dev, uint8_t cs,
                         const struct cn0566_init_param *init)
{
    struct adar1000_init_param p = {
        .spi_init = {.device_id = ADAR1000_SPI_DEVICE,
            .max_speed_hz = ADAR1000_SPI_SPEED, .chip_select = cs,
            .mode = NO_OS_SPI_MODE_0, .platform_ops = SPI_OPS, .extra = NULL},
        .dev_addr = ADAR1000_DEV_ADDR
    };
    uint16_t id;
    uint8_t ch;
    int32_t ret = adar1000_init(dev, &p);
    if (ret) return ret;
    if ((ret = adar1000_read_product_id(*dev, &id))) goto fail;
    printf("ADAR1000 CS%u product ID: 0x%04X\n", cs, id);
    if ((ret = adar1000_setup_rx(*dev))) goto fail;
    for (ch = 0; ch < ADAR1000_NUM_CHANNELS; ch++) {
        if ((ret = adar1000_set_rx_gain(*dev, ch, init->initial_gain))) goto fail;
        if ((ret = adar1000_set_rx_phase(*dev, ch, init->initial_phase_deg))) goto fail;
    }
    if ((ret = adar1000_latch_rx(*dev))) goto fail;
    return 0;
fail:
    adar1000_remove(*dev); *dev = NULL; return ret;
}

static int32_t init_pll(struct adf4159_dev **pll)
{
    struct adf4159_init_param p = {
        .spi_init = {.device_id = ADF4159_SPI_DEVICE,
            .max_speed_hz = ADF4159_SPI_SPEED, .chip_select = ADF4159_SPI_CS,
            .mode = NO_OS_SPI_MODE_0, .platform_ops = SPI_OPS, .extra = NULL},
        .gpio_le = {.number = ADF4159_GPIO_LE, .platform_ops = GPIO_OPS, .extra = NULL},
        .gpio_ce = {.number = -1, .platform_ops = GPIO_OPS, .extra = NULL},
        .config = adf4159_default_cfg
    };
    int32_t ret = adf4159_init(pll, &p);
    if (!ret) ret = adf4159_set_muxout(*pll, 6U);
    return ret;
}

int32_t cn0566_init(struct cn0566_dev **out,
                    const struct cn0566_init_param *init)
{
    struct cn0566_dev *dev;
    int32_t ret;
    if (!out || !init || init->initial_gain > 127U || init->initial_phase_deg >= 360U)
        return -EINVAL;
    dev = calloc(1, sizeof(*dev));
    if (!dev) return -ENOMEM;
    if ((ret = gpio_init_all(&dev->gpios))) goto fail;
    if ((ret = init_adar(&dev->adar[0], ADAR1000_SPI_CS_0, init))) goto fail;
    if ((ret = init_adar(&dev->adar[1], ADAR1000_SPI_CS_1, init))) goto fail;
    if ((ret = init_pll(&dev->pll))) goto fail;
    *out = dev;
    return 0;
fail:
    cn0566_remove(dev); return ret;
}

struct adar1000_dev *cn0566_get_adar1000(struct cn0566_dev *dev, uint8_t chip)
{ return (!dev || chip >= CN0566_NUM_ADAR1000) ? NULL : dev->adar[chip]; }
struct adf4159_dev *cn0566_get_adf4159(struct cn0566_dev *dev)
{ return dev ? dev->pll : NULL; }

int32_t cn0566_set_all_rx_gains(struct cn0566_dev *dev, uint8_t gain)
{
    uint8_t gains[ADAR1000_NUM_CHANNELS], chip, ch;
    int32_t ret;
    if (!dev || gain > 127U) return -EINVAL;
    for (ch = 0; ch < ADAR1000_NUM_CHANNELS; ch++) gains[ch] = gain;
    for (chip = 0; chip < CN0566_NUM_ADAR1000; chip++) {
        ret = adar1000_set_all_rx_gains(dev->adar[chip], gains);
        if (ret) return ret;
        if ((ret = adar1000_latch_rx(dev->adar[chip]))) return ret;
    }
    return 0;
}

int32_t cn0566_set_all_rx_phases(struct cn0566_dev *dev, uint16_t phase)
{
    uint8_t chip, ch;
    int32_t ret;
    if (!dev || phase >= 360U) return -EINVAL;
    for (chip = 0; chip < CN0566_NUM_ADAR1000; chip++) {
        for (ch = 0; ch < ADAR1000_NUM_CHANNELS; ch++) {
            ret = adar1000_set_rx_phase(dev->adar[chip], ch, phase);
            if (ret) return ret;
        }
        if ((ret = adar1000_latch_rx(dev->adar[chip]))) return ret;
    }
    return 0;
}

int32_t cn0566_get_pll_lock(struct cn0566_dev *dev, uint8_t *locked)
{
    if (!dev || !locked) return -EINVAL;
    return no_os_gpio_get_value(dev->gpios.muxout, locked);
}

void cn0566_remove(struct cn0566_dev *dev)
{
    uint8_t i;
    if (!dev) return;
    if (dev->pll) adf4159_remove(dev->pll);
    for (i = 0; i < CN0566_NUM_ADAR1000; i++)
        if (dev->adar[i]) adar1000_remove(dev->adar[i]);
    gpio_remove_all(&dev->gpios);
    free(dev);
}
