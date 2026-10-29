/***************************************************************************//**
 * @file   adar1000.c
 * @brief  ADAR1000 RX direct-register driver for the CN0566 Phaser.
 * @author Melissa Makonga
 ******************************************************************************/

#include <stdio.h>
#include "adar1000.h"
#include "no_os_alloc.h"
#include "no_os_delay.h"
#include "no_os_error.h"

static const struct adar1000_phase adar1000_phase_table[] = {

	{0,0,0x3F,0x20},{2,8125,0x3F,0x21},{5,6250,0x3F,0x23},{8,4375,0x3F,0x24},
	{11,2500,0x3F,0x26},{14,625,0x3E,0x27},{16,8750,0x3E,0x28},{19,6875,0x3D,0x2A},
	{22,5000,0x3D,0x2B},{25,3125,0x3C,0x2D},{28,1250,0x3C,0x2E},{30,9375,0x3B,0x2F},
	{33,7500,0x3A,0x30},{36,5625,0x39,0x31},{39,3750,0x38,0x33},{42,1875,0x37,0x34},
	{45,0,0x36,0x35},{47,8125,0x35,0x36},{50,6250,0x34,0x37},{53,4375,0x33,0x38},
	{56,2500,0x32,0x38},{59,625,0x30,0x39},{61,8750,0x2F,0x3A},{64,6875,0x2E,0x3A},
	{67,5000,0x2C,0x3B},{70,3125,0x2B,0x3C},{73,1250,0x2A,0x3C},{75,9375,0x28,0x3C},
	{78,7500,0x27,0x3D},{81,5625,0x25,0x3D},{84,3750,0x24,0x3D},{87,1875,0x22,0x3D},
	{90,0,0x21,0x3D},{92,8125,0x01,0x3D},{95,6250,0x03,0x3D},{98,4375,0x04,0x3D},
	{101,2500,0x06,0x3D},{104,625,0x07,0x3C},{106,8750,0x08,0x3C},{109,6875,0x0A,0x3C},
	{112,5000,0x0B,0x3B},{115,3125,0x0D,0x3A},{118,1250,0x0E,0x3A},{120,9375,0x0F,0x39},
	{123,7500,0x11,0x38},{126,5625,0x12,0x38},{129,3750,0x13,0x37},{132,1875,0x14,0x36},
	{135,0,0x16,0x35},{137,8125,0x17,0x34},{140,6250,0x18,0x33},{143,4375,0x19,0x31},
	{146,2500,0x19,0x30},{149,625,0x1A,0x2F},{151,8750,0x1B,0x2E},{154,6875,0x1C,0x2D},
	{157,5000,0x1C,0x2B},{160,3125,0x1D,0x2A},{163,1250,0x1E,0x28},{165,9375,0x1E,0x27},
	{168,7500,0x1E,0x26},{171,5625,0x1F,0x24},{174,3750,0x1F,0x23},{177,1875,0x1F,0x21},
	{180,0,0x1F,0x20},{182,8125,0x1F,0x01},{185,6250,0x1F,0x03},{188,4375,0x1F,0x04},
	{191,2500,0x1F,0x06},{194,625,0x1E,0x07},{196,8750,0x1E,0x08},{199,6875,0x1D,0x0A},
	{202,5000,0x1D,0x0B},{205,3125,0x1C,0x0D},{208,1250,0x1C,0x0E},{210,9375,0x1B,0x0F},
	{213,7500,0x1A,0x10},{216,5625,0x19,0x11},{219,3750,0x18,0x13},{222,1875,0x17,0x14},
	{225,0,0x16,0x15},{227,8125,0x15,0x16},{230,6250,0x14,0x17},{233,4375,0x13,0x18},
	{236,2500,0x12,0x18},{239,625,0x10,0x19},{241,8750,0x0F,0x1A},{244,6875,0x0E,0x1A},
	{247,5000,0x0C,0x1B},{250,3125,0x0B,0x1C},{253,1250,0x0A,0x1C},{255,9375,0x08,0x1C},
	{258,7500,0x07,0x1D},{261,5625,0x05,0x1D},{264,3750,0x04,0x1D},{267,1875,0x02,0x1D},
	{270,0,0x01,0x1D},{272,8125,0x21,0x1D},{275,6250,0x23,0x1D},{278,4375,0x24,0x1D},
	{281,2500,0x26,0x1D},{284,625,0x27,0x1C},{286,8750,0x28,0x1C},{289,6875,0x2A,0x1C},
	{292,5000,0x2B,0x1B},{295,3125,0x2D,0x1A},{298,1250,0x2E,0x1A},{300,9375,0x2F,0x19},
	{303,7500,0x31,0x18},{306,5625,0x32,0x18},{309,3750,0x33,0x17},{312,1875,0x34,0x16},
	{315,0,0x36,0x15},{317,8125,0x37,0x14},{320,6250,0x38,0x13},{323,4375,0x39,0x11},
	{326,2500,0x39,0x10},{329,625,0x3A,0x0F},{331,8750,0x3B,0x0E},{334,6875,0x3C,0x0D},
	{337,5000,0x3C,0x0B},{340,3125,0x3D,0x0A},{343,1250,0x3E,0x08},{345,9375,0x3E,0x07},
	{348,7500,0x3E,0x06},{351,5625,0x3F,0x04},{354,3750,0x3F,0x03},{357,1875,0x3F,0x01}
};

static int32_t adar1000_scratchpad_test(struct adar1000_dev *dev,
					uint8_t pattern)
{
	uint8_t readback;
	int32_t ret;

	ret = adar1000_spi_write(dev, ADAR1000_SCRATCH_PAD, pattern);
	if (ret)
		return ret;

	ret = adar1000_spi_read(dev, ADAR1000_SCRATCH_PAD, &readback);
	if (ret)
		return ret;

	return readback == pattern ? 0 : -EIO;
}

int32_t adar1000_spi_write(struct adar1000_dev *dev, uint16_t reg_addr,
			   uint8_t data)
{
	uint8_t buf[3];
	uint16_t command;

	if (!dev || !dev->spi_desc)
		return -EINVAL;
	if (reg_addr & ~ADAR1000_SPI_REG_MASK)
		return -EINVAL;

	command = ADAR1000_SPI_ADDR(dev->dev_addr) | reg_addr;
	buf[0] = (uint8_t)(command >> 8);
	buf[1] = (uint8_t)command;
	buf[2] = data;

	return no_os_spi_write_and_read(dev->spi_desc, buf, sizeof(buf));
}

int32_t adar1000_spi_read(struct adar1000_dev *dev, uint16_t reg_addr,
			  uint8_t *data)
{
	uint8_t buf[3];
	uint16_t command;
	int32_t ret;
	int32_t restore_ret;

	if (!dev || !dev->spi_desc || !data)
		return -EINVAL;
	if (reg_addr & ~ADAR1000_SPI_REG_MASK)
		return -EINVAL;

	ret = adar1000_spi_write(dev, ADAR1000_INTERFACE_CFG_A,
				 ADAR1000_4WIRE_SPI);
	if (ret)
		return ret;

	command = ADAR1000_SPI_READ_CMD | ADAR1000_SPI_ADDR(dev->dev_addr) |
		  reg_addr;
	buf[0] = (uint8_t)(command >> 8);
	buf[1] = (uint8_t)command;
	buf[2] = 0U;

	ret = no_os_spi_write_and_read(dev->spi_desc, buf, sizeof(buf));
	if (!ret)
		*data = buf[2];

	restore_ret = adar1000_spi_write(dev, ADAR1000_INTERFACE_CFG_A, 0U);
	return ret ? ret : restore_ret;
}

int32_t adar1000_init(struct adar1000_dev **device,
		      const struct adar1000_init_param *param)
{
	struct adar1000_dev *dev;
	int32_t ret;

	if (!device || !param || param->dev_addr > 3U)
		return -EINVAL;

	dev = no_os_calloc(1, sizeof(*dev));
	if (!dev)
		return -ENOMEM;

	dev->dev_addr = param->dev_addr;
	ret = no_os_spi_init(&dev->spi_desc, &param->spi_init);
	if (ret)
		goto error_free;

	ret = adar1000_spi_write(dev, ADAR1000_INTERFACE_CFG_A,
				 ADAR1000_SOFTRESET);
	if (ret)
		goto error_spi;
	no_os_mdelay(1);

	ret = adar1000_scratchpad_test(dev, ADAR1000_SCRATCH_PAD_VAL_1);
	if (ret)
		goto error_spi;
	ret = adar1000_scratchpad_test(dev, ADAR1000_SCRATCH_PAD_VAL_2);
	if (ret)
		goto error_spi;

	ret = adar1000_spi_write(dev, ADAR1000_LDO_TRIM_CTL_1,
				 ADAR1000_LDO_TRIM_SEL_2);
	if (ret)
		goto error_spi;
	ret = adar1000_spi_write(dev, ADAR1000_LDO_TRIM_CTL_0,
				 ADAR1000_LDO_TRIM_VAL);
	if (ret)
		goto error_spi;

	*device = dev;
	return 0;

error_spi:
	no_os_spi_remove(dev->spi_desc);
error_free:
	no_os_free(dev);
	return ret;
}

int32_t adar1000_remove(struct adar1000_dev *dev)
{
	int32_t ret;
	if (!dev)
		return -EINVAL;
	ret = no_os_spi_remove(dev->spi_desc);
	no_os_free(dev);
	return ret;
}

int32_t adar1000_read_product_id(struct adar1000_dev *dev,
				 uint16_t *product_id)
{
	uint8_t high;
	uint8_t low;
	int32_t ret;

	if (!product_id)
		return -EINVAL;
	ret = adar1000_spi_read(dev, ADAR1000_PRODUCT_ID_H, &high);
	if (ret)
		return ret;
	ret = adar1000_spi_read(dev, ADAR1000_PRODUCT_ID_L, &low);
	if (ret)
		return ret;
	*product_id = ((uint16_t)high << 8) | low;
	return 0;
}

int32_t adar1000_setup_rx(struct adar1000_dev *dev)
{
	int32_t ret;
	if (!dev)
		return -EINVAL;

	ret = adar1000_spi_write(dev, ADAR1000_MEM_CTRL,
				 ADAR1000_MEM_CTRL_SPI_MODE);
	if (ret)
		return ret;
	ret = adar1000_spi_write(dev, ADAR1000_SW_CTRL,
				 ADAR1000_SW_CTRL_RX);
	if (ret)
		return ret;
	ret = adar1000_spi_write(dev, ADAR1000_RX_ENABLES,
				 ADAR1000_RX_EN_ALL);
	if (ret)
		return ret;
	ret = adar1000_spi_write(dev, ADAR1000_BIAS_CURRENT_RX_LNA,
				 ADAR1000_RX_BIAS_LNA_MID);
	if (ret)
		return ret;
	ret = adar1000_spi_write(dev, ADAR1000_BIAS_CURRENT_RX,
				 ADAR1000_RX_BIAS_VGA_VM);
	if (ret)
		return ret;
	return adar1000_spi_write(dev, ADAR1000_MISC_ENABLES, 0U);
}

int32_t adar1000_set_rx_gain(struct adar1000_dev *dev, uint8_t channel,
			     uint8_t gain)
{
	uint8_t value;
	if (!dev || !ADAR1000_CH_VALID(channel) || gain > ADAR1000_RX_GAIN_MAX)
		return -EINVAL;

	value = gain & ADAR1000_RX_GAIN_MASK;

	/* Match CN0566 behavior: insert attenuator at zero gain. */
	if (gain != 0U)
		value |= ADAR1000_CH_ATTN_BYPASS;

	return adar1000_spi_write(dev, ADAR1000_CH_RX_GAIN(channel), value);
}

int32_t adar1000_get_rx_gain_raw(struct adar1000_dev *dev, uint8_t channel,
			         uint8_t *raw)
{
	if (!dev || !raw || !ADAR1000_CH_VALID(channel))
		return -EINVAL;

	return adar1000_spi_read(dev, ADAR1000_CH_RX_GAIN(channel), raw);
}

int32_t adar1000_get_rx_gain(struct adar1000_dev *dev, uint8_t channel,
			     uint8_t *gain)
{
	uint8_t raw;
	int32_t ret;

	if (!gain)
		return -EINVAL;

	ret = adar1000_get_rx_gain_raw(dev, channel, &raw);
	if (ret)
		return ret;

	*gain = raw & ADAR1000_RX_GAIN_MASK;
	return 0;
}

int32_t adar1000_set_all_rx_gains(struct adar1000_dev *dev,
				  const uint8_t gains[ADAR1000_NUM_CHANNELS])
{
	uint8_t channel;
	int32_t ret;
	if (!dev || !gains)
		return -EINVAL;

	for (channel = 0; channel < ADAR1000_NUM_CHANNELS; channel++) {
		ret = adar1000_set_rx_gain(dev, channel, gains[channel]);
		if (ret)
			return ret;
	}
	return adar1000_latch_rx(dev);
}

int32_t adar1000_set_rx_phase(struct adar1000_dev *dev, uint8_t channel,
			      uint16_t degrees)
{
	uint16_t index;
	int32_t ret;
	if (!dev || !ADAR1000_CH_VALID(channel) || degrees >= 360U)
		return -EINVAL;

	index = ((uint32_t)degrees * ADAR1000_PHASE_TABLE_SIZE + 180U) / 360U;
	if (index >= ADAR1000_PHASE_TABLE_SIZE)
		index = 0U;

	ret = adar1000_spi_write(dev, ADAR1000_CH_RX_PHASE_I(channel),
				 adar1000_phase_table[index].vm_i);
	if (ret)
		return ret;
	return adar1000_spi_write(dev, ADAR1000_CH_RX_PHASE_Q(channel),
				  adar1000_phase_table[index].vm_q);
}

int32_t adar1000_latch_rx(struct adar1000_dev *dev)
{
	if (!dev)
		return -EINVAL;
	return adar1000_spi_write(dev, ADAR1000_LD_WRK_REGS,
				  ADAR1000_LDRX_OVERRIDE);
}

int32_t adar1000_verify_rx(struct adar1000_dev *dev, const char *label)
{
	static const struct {
		uint16_t address;
		uint8_t expected;
		const char *name;
	} regs[] = {
		{ADAR1000_MEM_CTRL, ADAR1000_MEM_CTRL_SPI_MODE, "MEM_CTRL"},
		{ADAR1000_SW_CTRL, ADAR1000_SW_CTRL_RX, "SW_CTRL"},
		{ADAR1000_RX_ENABLES, ADAR1000_RX_EN_ALL, "RX_ENABLES"},
		{ADAR1000_BIAS_CURRENT_RX_LNA, ADAR1000_RX_BIAS_LNA_MID, "BIAS_RX_LNA"},
		{ADAR1000_BIAS_CURRENT_RX, ADAR1000_RX_BIAS_VGA_VM, "BIAS_RX"},
		{ADAR1000_MISC_ENABLES, 0U, "MISC_ENABLES"}
	};
	uint8_t value = 0U;
	uint32_t i;
	uint8_t ch;
	int32_t ret;
	int32_t status = 0;

	if (!dev)
		return -EINVAL;

	printf("%s RX register verification:\n", label ? label : "ADAR1000");
	for (i = 0; i < NO_OS_ARRAY_SIZE(regs); i++) {
		ret = adar1000_spi_read(dev, regs[i].address, &value);
		if (ret || value != regs[i].expected) {
			printf("  %s [0x%03X]: read=%s0x%02X expected=0x%02X FAIL\n",
			       regs[i].name, regs[i].address, ret ? "ERR/" : "",
			       value, regs[i].expected);
			status = ret ? ret : -EIO;
		} else {
			printf("  %s [0x%03X]: 0x%02X OK\n",
			       regs[i].name, regs[i].address, value);
		}
	}

	for (ch = 0; ch < ADAR1000_NUM_CHANNELS; ch++) {
		uint8_t gain = 0U, phase_i = 0U, phase_q = 0U;
		ret = adar1000_spi_read(dev, ADAR1000_CH_RX_GAIN(ch), &gain);
		if (!ret)
			ret = adar1000_spi_read(dev, ADAR1000_CH_RX_PHASE_I(ch), &phase_i);
		if (!ret)
			ret = adar1000_spi_read(dev, ADAR1000_CH_RX_PHASE_Q(ch), &phase_q);
		if (ret) {
			printf("  CH%u gain/phase read failed: %ld\n", ch, (long)ret);
			status = ret;
		} else {
			printf("  CH%u gain=0x%02X (VGA=%u, ATTN=%s), I=0x%02X Q=0x%02X\n",
			       ch, gain, gain & ADAR1000_RX_GAIN_MASK,
			       (gain & ADAR1000_CH_ATTN_BYPASS) ? "BYPASS" : "INSERT",
			       phase_i, phase_q);
		}
	}

	return status;
}
