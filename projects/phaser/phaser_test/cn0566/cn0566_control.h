#ifndef CN0566_CONTROL_H
#define CN0566_CONTROL_H

#include <stdint.h>
#include "adar1000.h"
#include "adf4159.h"

#define CN0566_NUM_ADAR1000 2U

struct cn0566_dev;
struct cn0566_init_param {
    uint8_t initial_gain;
    uint16_t initial_phase_deg;
};

int32_t cn0566_init(struct cn0566_dev **dev,
                    const struct cn0566_init_param *init);
struct adar1000_dev *cn0566_get_adar1000(struct cn0566_dev *dev,
                                         uint8_t chip);
struct adf4159_dev *cn0566_get_adf4159(struct cn0566_dev *dev);
int32_t cn0566_set_all_rx_gains(struct cn0566_dev *dev, uint8_t gain);
int32_t cn0566_set_all_rx_phases(struct cn0566_dev *dev, uint16_t phase_deg);
int32_t cn0566_get_pll_lock(struct cn0566_dev *dev, uint8_t *locked);
int32_t cn0566_verify_gpios(struct cn0566_dev *dev);
void cn0566_remove(struct cn0566_dev *dev);

#endif
