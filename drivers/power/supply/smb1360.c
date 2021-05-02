// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Register definitions taken from drivers/power/smb1360-charger-fg.c
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
 *
 * TODO:
 *  - Investigate using common battery device tree properties
 *    (e.g. voltage-{min,max}-design-microvolt)
 *  - Implement more power supply properties, e.g.
 *    - POWER_SUPPLY_PROP_TEMP_{,ALERT_}{MIN,MAX}
 *    - POWER_SUPPLY_PROP_CAPACITY_ALERT_{MIN,MAX}
 *    - POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT
 */

#ifdef CONFIG_SMB1360_DEBUG
#define DEBUG
#endif

#include <linux/completion.h>
#include <linux/extcon-provider.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

/* Charger Registers */
#define CFG_BATT_CHG_REG		0x00
#define CHG_ITERM_MASK			GENMASK(2, 0)
#define RECHG_MV_MASK			GENMASK(6, 5)
#define RECHG_MV_SHIFT			5
#define OTG_CURRENT_MASK		GENMASK(4, 3)
#define OTG_CURRENT_SHIFT		3

#define CFG_BATT_CHG_ICL_REG		0x05
#define AC_INPUT_ICL_PIN_BIT		BIT(7)
#define AC_INPUT_PIN_HIGH_BIT		BIT(6)
#define RESET_STATE_USB_500		BIT(5)
#define INPUT_CURR_LIM_MASK		GENMASK(3, 0)

#define CFG_GLITCH_FLT_REG		0x06
#define AICL_ENABLED_BIT		BIT(0)
#define INPUT_UV_GLITCH_FLT_20MS_BIT	BIT(7)

#define CFG_CHG_MISC_REG		0x7
#define CHG_EN_BY_PIN_BIT		BIT(7)
#define CHG_EN_ACTIVE_LOW_BIT		BIT(6)
#define PRE_TO_FAST_REQ_CMD_BIT		BIT(5)
#define CFG_BAT_OV_ENDS_CHG_CYC		BIT(4)
#define CHG_CURR_TERM_DIS_BIT		BIT(3)
#define CFG_AUTO_RECHG_DIS_BIT		BIT(2)
#define CFG_CHG_INHIBIT_EN_BIT		BIT(0)

#define CFG_CHG_FUNC_CTRL_REG		0x08
#define CHG_RECHG_THRESH_FG_SRC_BIT	BIT(1)

#define CFG_STAT_CTRL_REG		0x09
#define CHG_STAT_IRQ_ONLY_BIT		BIT(4)
#define CHG_TEMP_CHG_ERR_BLINK_BIT	BIT(3)
#define CHG_STAT_ACTIVE_HIGH_BIT	BIT(1)
#define CHG_STAT_DISABLE_BIT		BIT(0)

#define CFG_SFY_TIMER_CTRL_REG		0x0A
#define SAFETY_TIME_DISABLE_BIT		BIT(5)
#define SAFETY_TIME_MINUTES_SHIFT	2
#define SAFETY_TIME_MINUTES_MASK	GENMASK(3, 2)

#define CFG_BATT_MISSING_REG		0x0D
#define BATT_MISSING_SRC_THERM_BIT	BIT(1)

#define CFG_FG_BATT_CTRL_REG		0x0E
#define CFG_FG_OTP_BACK_UP_ENABLE	BIT(7)
#define BATT_ID_ENABLED_BIT		BIT(5)
#define CHG_BATT_ID_FAIL		BIT(4)
#define BATT_ID_FAIL_SELECT_PROFILE	BIT(3)
#define BATT_PROFILE_SELECT_MASK	GENMASK(3, 0)
#define BATT_PROFILEA_MASK		0x0
#define BATT_PROFILEB_MASK		0xF

#define IRQ_CFG_REG			0x0F
#define IRQ_INTERNAL_TEMPERATURE_BIT	BIT(0)
#define IRQ_AICL_DONE_BIT		BIT(1)
#define IRQ_DCIN_UV_BIT			BIT(2)
#define IRQ_BAT_HOT_COLD_SOFT_BIT	BIT(6)
#define IRQ_HOT_COLD_HARD_BIT		BIT(7)

#define IRQ2_CFG_REG			0x10
#define IRQ2_VBAT_LOW_BIT		BIT(0)
#define IRQ2_BATT_MISSING_BIT		BIT(1)
#define IRQ2_POWER_OK_BIT		BIT(2)
#define IRQ2_CHG_PHASE_CHANGE_BIT	BIT(4)
#define IRQ2_CHG_ERR_BIT		BIT(6)
#define IRQ2_SAFETY_TIMER_BIT		BIT(7)

#define IRQ3_CFG_REG			0x11
#define IRQ3_SOC_FULL_BIT		BIT(0)
#define IRQ3_SOC_EMPTY_BIT		BIT(1)
#define IRQ3_SOC_MAX_BIT		BIT(2)
#define IRQ3_SOC_MIN_BIT		BIT(3)
#define IRQ3_SOC_CHANGE_BIT		BIT(4)
#define IRQ3_FG_ACCESS_OK_BIT		BIT(6)

#define CHG_CURRENT_REG			0x13
#define FASTCHG_CURR_MASK		GENMASK(4, 2)
#define FASTCHG_CURR_SHIFT		2

#define CHG_CMP_CFG			0x14
#define JEITA_COMP_CURR_MASK		GENMASK(3, 0)
#define JEITA_COMP_EN_MASK		GENMASK(7, 4)
#define JEITA_COMP_EN_SHIFT		4
#define JEITA_COMP_EN_BIT		GENMASK(7, 4)

#define BATT_CHG_FLT_VTG_REG		0x15
#define VFLOAT_MASK			GENMASK(6, 0)

#define CFG_FVC_REG			0x16
#define FLT_VTG_COMP_MASK		GENMASK(6, 0)

#define SHDN_CTRL_REG			0x1A
#define SHDN_CMD_USE_BIT		BIT(1)
#define SHDN_CMD_POLARITY_BIT		BIT(2)

/* Command Registers */
#define CMD_I2C_REG			0x40
#define ALLOW_VOLATILE_BIT		BIT(6)
#define FG_ACCESS_ENABLED_BIT		BIT(5)
#define FG_RESET_BIT			BIT(4)
#define CYCLE_STRETCH_CLEAR_BIT		BIT(3)

#define CMD_IL_REG			0x41
#define USB_CTRL_MASK			GENMASK(1, 0)
#define USB_100_BIT			0x01
#define USB_500_BIT			0x00
#define USB_AC_BIT			0x02
#define SHDN_CMD_BIT			BIT(7)

#define CMD_CHG_REG			0x42
#define CMD_CHG_EN			BIT(1)
#define CMD_OTG_EN_BIT			BIT(0)

/* Status Registers */
#define STATUS_1_REG			0x48
#define AICL_CURRENT_STATUS_MASK	GENMASK(6, 0)
#define AICL_LIMIT_1500MA		0xF

#define STATUS_3_REG			0x4B
#define CHG_HOLD_OFF_BIT		BIT(3)
#define CHG_TYPE_MASK			GENMASK(2, 1)
#define CHG_TYPE_SHIFT			1
#define BATT_NOT_CHG_VAL		0x0
#define BATT_PRE_CHG_VAL		0x1
#define BATT_FAST_CHG_VAL		0x2
#define BATT_TAPER_CHG_VAL		0x3

#define STATUS_4_REG			0x4C
#define CYCLE_STRETCH_ACTIVE_BIT	BIT(5)

#define REVISION_CTRL_REG		0x4F
#define DEVICE_REV_MASK			GENMASK(3, 0)

/* IRQ Status Registers */
#define IRQ_REG				0x50

#define IRQ_A_REG			0x50
#define IRQ_A_HOT_HARD_BIT		BIT(6)
#define IRQ_A_COLD_HARD_BIT		BIT(4)
#define IRQ_A_HOT_SOFT_BIT		BIT(2)
#define IRQ_A_COLD_SOFT_BIT		BIT(0)

#define IRQ_B_REG			0x51
#define IRQ_B_BATT_TERMINAL_BIT		BIT(6)
#define IRQ_B_BATT_MISSING_BIT		BIT(4)
#define IRQ_B_VBAT_LOW_BIT		BIT(2)
#define IRQ_B_CHG_HOT_BIT		BIT(0)

#define IRQ_C_REG			0x52
#define IRQ_C_FAST_CHG_BIT		BIT(6)
#define IRQ_C_RECHARGE_BIT		BIT(4)
#define IRQ_C_TAPER_BIT			BIT(2)
#define IRQ_C_CHG_TERM_BIT		BIT(0)

#define IRQ_D_REG			0x53
#define IRQ_D_BATTERY_OV_BIT		BIT(6)
#define IRQ_D_AICL_DONE_BIT		BIT(4)
#define IRQ_D_SAFETY_TIMEOUT_BIT	BIT(2)
#define IRQ_D_PRECHG_TIMEOUT_BIT	BIT(0)

#define IRQ_E_REG			0x54
#define IRQ_E_INHIBIT_BIT		BIT(6)
#define IRQ_E_USBIN_OV_BIT		BIT(2)
#define IRQ_E_USBIN_UV_BIT		BIT(0)

#define IRQ_F_REG			0x54
#define IRQ_F_OTG_OC_BIT		BIT(6)
#define IRQ_F_OTG_FAIL_BIT		BIT(4)
#define IRQ_F_POWER_OK_BIT		BIT(0)

#define IRQ_G_REG			0x56
#define IRQ_G_WD_TIMEOUT_BIT		BIT(4)
#define IRQ_G_CHG_ERROR_BIT		BIT(2)
#define IRQ_G_SOC_CHANGE_BIT		BIT(0)

#define IRQ_H_REG			0x57
#define IRQ_H_FULL_SOC_BIT		BIT(6)
#define IRQ_H_EMPTY_SOC_BIT		BIT(4)
#define IRQ_H_MAX_SOC_BIT		BIT(2)
#define IRQ_H_MIN_SOC_BIT		BIT(0)

#define IRQ_I_REG			0x58
#define IRQ_I_BATT_ID_RESULT_BIT	GENMASK(6, 4)
#define IRQ_I_BATT_ID_SHIFT		4
#define IRQ_I_BATT_ID_COMPLETE_BIT	BIT(4)
#define IRQ_I_FG_DATA_RECOVERY_BIT	BIT(2)
#define IRQ_I_FG_ACCESS_ALLOWED_BIT	BIT(0)

/* FG registers - IRQ config register */
#define SOC_MAX_REG			0x24
#define SOC_MIN_REG			0x25
#define VTG_EMPTY_REG			0x26
#define SOC_DELTA_REG			0x28
#define JEITA_SOFT_COLD_REG		0x29
#define JEITA_SOFT_HOT_REG		0x2A
#define VTG_MIN_REG			0x2B

#define SOC_DELTA_VAL			1
#define SOC_MIN_VAL			15

/* FG SHADOW registers */
#define SHDW_FG_ESR_ACTUAL		0x20
#define SHDW_FG_BATT_STATUS		0x60
#define BATTERY_PROFILE_BIT		BIT(0)
#define SHDW_FG_MSYS_SOC		0x61
#define SHDW_FG_CAPACITY		0x62
#define SHDW_FG_VTG_NOW			0x69
#define SHDW_FG_CURR_NOW		0x6B
#define SHDW_FG_BATT_TEMP		0x6D

/* FG scratchpad registers */
#define VOLTAGE_PREDICTED_REG		0x80
#define CC_TO_SOC_COEFF			0xBA
#define NOMINAL_CAPACITY_REG		0xBC
#define ACTUAL_CAPACITY_REG		0xBE
#define FG_IBATT_STANDBY_REG		0xCF
#define FG_AUTO_RECHARGE_SOC		0xD2
#define FG_SYS_CUTOFF_V_REG		0xD3
#define FG_CC_TO_CV_V_REG		0xD5
#define FG_ITERM_REG			0xD9
#define FG_THERM_C1_COEFF_REG		0xDB

/* Constants */
#define SMB1360_REV_1			0x01

#define FG_RESET_THRESHOLD_MV		15

#define MIN_FLOAT_MV			3460
#define MAX_FLOAT_MV			4730
#define VFLOAT_STEP_MV			10

#define MIN_RECHG_MV			50
#define MAX_RECHG_MV			300

#define SMB1360_FG_ACCESS_TIMEOUT_MS	15000
#define SMB1360_POWERON_DELAY_MS	2000
#define SMB1360_FG_RESET_DELAY_MS	1500

/* FG registers (on different I2C address) */
#define FG_I2C_CFG_MASK			GENMASK(1, 0)
#define FG_CFG_I2C_ADDR			0x1
#define FG_PROFILE_A_ADDR		0x2
#define FG_PROFILE_B_ADDR		0x3

#define CURRENT_GAIN_LSB_REG		0x1D
#define CURRENT_GAIN_MSB_REG		0x1E

#define OTP_WRITABLE_REG_1		0xE0
#define OTP_WRITABLE_REG_2		0xE1
#define OTP_WRITABLE_REG_3		0xE2
#define OTP_WRITABLE_REG_4		0xE3
#define OTP_WRITABLE_REG_5		0xE4
#define OTP_WRITABLE_REG_6		0xE5
#define OTP_WRITABLE_REG_7		0xE6
#define OTP_WRITABLE_REG_8		0xE7
#define OTP_BACKUP_MAP_REG		0xF0
#define CURRENT_GAIN_BITMAP		0x5000
#define HARD_JEITA_BITMAP		0x0500

#define OTP_HARD_COLD_REG_ADDR		0x12
#define OTP_HARD_HOT_REG_ADDR		0x13
#define OTP_GAIN_FIRST_HALF_REG_ADDR	0x1D
#define OTP_GAIN_SECOND_HALF_REG_ADDR	0x1E

#define TEMP_THRE_SET(x) (((x) + 300) / 10)

enum {
	BATTERY_PROFILE_A,
	BATTERY_PROFILE_B,
	BATTERY_PROFILE_MAX,
};

enum {
	IRQ_A, IRQ_B, IRQ_C, IRQ_D, IRQ_E, IRQ_F, IRQ_G, IRQ_H, IRQ_I,
	IRQ_COUNT
};

static const unsigned int smb1360_usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_NONE,
};

struct smb1360 {
	struct device		*dev;
	struct regmap		*regmap;
	struct regmap		*fg_regmap;
	struct power_supply	*psy;
	struct extcon_dev	*edev;
	struct regulator_dev	*otg_vreg;
	struct completion	fg_mem_access_granted;
	struct delayed_work	delayed_init_work;

	unsigned int revision;
	u8 irqstat[IRQ_COUNT];

	bool shdn_after_pwroff;
	bool rsense_10mohm;
	bool initialized;

	int float_voltage;
};

static enum power_supply_property smb1360_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP
};

#define EXPONENT_MASK		0xF800
#define MANTISSA_MASK		0x3FF
#define SIGN_MASK		0x400
#define EXPONENT_SHIFT		11
#define SIGN_SHIFT		10
#define MICRO_UNIT		1000000ULL
static s64 float_decode(u16 reg)
{
	s64 final_val, exponent_val, mantissa_val;
	int exponent, mantissa, n;
	bool sign;

	exponent = (reg & EXPONENT_MASK) >> EXPONENT_SHIFT;
	mantissa = (reg & MANTISSA_MASK);
	sign = !!(reg & SIGN_MASK);

	mantissa_val = mantissa * MICRO_UNIT;

	n = exponent - 15;
	if (n < 0)
		exponent_val = MICRO_UNIT >> -n;
	else
		exponent_val = MICRO_UNIT << n;

	n = n - 10;
	if (n < 0)
		mantissa_val >>= -n;
	else
		mantissa_val <<= n;

	final_val = exponent_val + mantissa_val;

	if (sign)
		final_val *= -1;

	return final_val;
}

#define MAX_MANTISSA (1023 * 1000000ULL)
unsigned int float_encode(s64 float_val)
{
	int exponent = 0, sign = 0;
	unsigned int final_val = 0;

	if (float_val == 0)
		return 0;

	if (float_val < 0) {
		sign = 1;
		float_val = -float_val;
	}

	/* Reduce large mantissa until it fits into 10 bit */
	while (float_val >= MAX_MANTISSA) {
		exponent++;
		float_val >>= 1;
	}

	/* Increase small mantissa to improve precision */
	while (float_val < MAX_MANTISSA && exponent > -25) {
		exponent--;
		float_val <<= 1;
	}

	exponent = exponent + 25;

	/* Convert mantissa from micro-units to units */
	float_val = div_s64((float_val + MICRO_UNIT), (int)MICRO_UNIT);

	if (float_val == 1024) {
		exponent--;
		float_val <<= 1;
	}

	float_val -= 1024;

	/* Ensure that resulting number is within range */
	if (float_val > MANTISSA_MASK)
		float_val = MANTISSA_MASK;

	/* Convert to 5 bit exponent, 11 bit mantissa */
	final_val = (float_val & MANTISSA_MASK) | (sign << SIGN_SHIFT) |
		((exponent << EXPONENT_SHIFT) & EXPONENT_MASK);

	return final_val;
}

static int smb1360_update_le16(struct smb1360 *smb, u8 reg, const char *prop, s16 scale)
{
	int ret;
	u32 temp;
	__le16 val;

	if (device_property_read_u32(smb->dev, prop, &temp))
		return 0;

	if (scale > 0)
		temp = div_u64(temp * S16_MAX, scale);
	else if (scale < 0)
		temp = div_s64(temp * S16_MAX, scale);

	val = cpu_to_le16(temp);
	ret = regmap_raw_write(smb->regmap, reg, &val, sizeof(val));
	if (ret)
		dev_err(smb->dev, "writing %s failed: %d\n", prop, ret);
	return ret;
}

static int smb1360_read_voltage(struct smb1360 *smb, u8 reg,
				int *voltage)
{
	__le16 val;
	int ret;

	ret = regmap_raw_read(smb->regmap, reg, &val, sizeof(val));
	if (ret)
		return ret;

	*voltage = div_u64(le16_to_cpu(val) * 5000, S16_MAX);
	return 0;
}

static int smb1360_get_prop_batt_status(struct smb1360 *smb,
					union power_supply_propval *val)
{
	unsigned int reg, chg_type;
	int ret;

	if (smb->irqstat[IRQ_C] & IRQ_C_CHG_TERM_BIT) {
		val->intval = POWER_SUPPLY_STATUS_FULL;
		return 0;
	}

	ret = regmap_read(smb->regmap, STATUS_3_REG, &reg);
	if (ret) {
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		return ret;
	}

	if (reg & CHG_HOLD_OFF_BIT) {
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		return 0;
	}

	chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;
	if (chg_type == BATT_NOT_CHG_VAL) {
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		return 0;
	}

	val->intval = POWER_SUPPLY_STATUS_CHARGING;
	return 0;
}

static int smb1360_get_prop_charge_type(struct smb1360 *smb,
					union power_supply_propval *val)
{
	int ret;
	unsigned int reg;

	ret = regmap_read(smb->regmap, STATUS_3_REG, &reg);
	if (ret) {
		val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		return ret;
	}

	switch ((reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT) {
	case BATT_NOT_CHG_VAL:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	case BATT_FAST_CHG_VAL:
	case BATT_TAPER_CHG_VAL:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case BATT_PRE_CHG_VAL:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	default:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	}

	return 0;
}

static int smb1360_get_prop_batt_health(struct smb1360 *smb,
					union power_supply_propval *val)
{
	if (smb->irqstat[IRQ_A] & IRQ_A_HOT_HARD_BIT)
		val->intval = POWER_SUPPLY_HEALTH_HOT;
	else if (smb->irqstat[IRQ_A] & IRQ_A_HOT_SOFT_BIT)
		val->intval = POWER_SUPPLY_HEALTH_WARM;
	else if (smb->irqstat[IRQ_A] & IRQ_A_COLD_HARD_BIT)
		val->intval = POWER_SUPPLY_HEALTH_COLD;
	else if (smb->irqstat[IRQ_A] & IRQ_A_COLD_SOFT_BIT)
		val->intval = POWER_SUPPLY_HEALTH_COOL;
	else
		val->intval = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int smb1360_get_prop_current_now(struct smb1360 *smb,
					union power_supply_propval *val)
{
	__le16 temp;
	int ret;

	ret = regmap_raw_read(smb->regmap, SHDW_FG_CURR_NOW, &temp, sizeof(temp));
	if (ret)
		return ret;

	val->intval = -(div_s64(((s16)le16_to_cpu(temp)) * 2500, S16_MAX) * 1000);

	return 0;
}

static int smb1360_get_prop_chg_full_design(struct smb1360 *smb,
					    union power_supply_propval *val)
{
	__le16 fcc_mah;
	int ret;

	ret = regmap_raw_read(smb->regmap, SHDW_FG_CAPACITY, &fcc_mah, sizeof(fcc_mah));
	if (ret)
		return ret;

	val->intval = le16_to_cpu(fcc_mah) * 1000;
	return 0;
}

static int smb1360_get_prop_batt_capacity(struct smb1360 *smb,
					  union power_supply_propval *val)
{
	int ret, soc = 0;
	unsigned int reg;

	if (smb->irqstat[IRQ_H] & IRQ_H_EMPTY_SOC_BIT) {
		val->intval = 0;
		return 0;
	}

	ret = regmap_read(smb->regmap, SHDW_FG_MSYS_SOC, &reg);
	if (ret)
		return ret;

	soc = DIV_ROUND_CLOSEST(reg * 100, U8_MAX);
	val->intval = clamp(soc, 0, 100);

	return 0;
}

static int smb1360_get_prop_batt_temp(struct smb1360 *smb,
				      union power_supply_propval *val)
{
	__le16 temp;
	int ret;

	ret = regmap_raw_read(smb->regmap, SHDW_FG_BATT_TEMP, &temp, sizeof(temp));
	if (ret)
		return ret;

	temp = div_u64(le16_to_cpu(temp) * 625, 10000UL); /* temperature in K */
	val->intval = (temp - 273) * 10; /* temperature in decideg */

	return 0;
}

static int smb1360_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct smb1360 *smb = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		return smb1360_get_prop_batt_status(smb, val);
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		return smb1360_get_prop_charge_type(smb, val);
	case POWER_SUPPLY_PROP_HEALTH:
		return smb1360_get_prop_batt_health(smb, val);
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = !(smb->irqstat[IRQ_E] & IRQ_E_USBIN_UV_BIT);
		return 0;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		return smb1360_read_voltage(smb, SHDW_FG_VTG_NOW, &val->intval);
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		return smb1360_get_prop_current_now(smb, val);
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		return smb1360_get_prop_chg_full_design(smb, val);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = smb->float_voltage * 1000;
		return 0;
	case POWER_SUPPLY_PROP_CAPACITY:
		return smb1360_get_prop_batt_capacity(smb, val);
	case POWER_SUPPLY_PROP_TEMP:
		return smb1360_get_prop_batt_temp(smb, val);
	default:
		return -EINVAL;
	}
}

static irqreturn_t smb1360_irq(int irq, void *data)
{
	struct smb1360 *smb = data;
	int ret;

	ret = regmap_raw_read(smb->regmap, IRQ_REG, smb->irqstat, sizeof(smb->irqstat));
	if (ret < 0)
		return IRQ_NONE;

	extcon_set_state_sync(smb->edev, EXTCON_USB,
			      !(smb->irqstat[IRQ_E] & IRQ_E_USBIN_UV_BIT));

	if (smb->irqstat[IRQ_F] & (IRQ_F_OTG_FAIL_BIT | IRQ_F_OTG_OC_BIT)) {
		dev_warn(smb->dev, "otg error: %d\n", smb->irqstat[IRQ_F]);
		regulator_disable_regmap(smb->otg_vreg);
	}

	if (smb->irqstat[IRQ_I] & IRQ_I_FG_ACCESS_ALLOWED_BIT)
		complete_all(&smb->fg_mem_access_granted);

	if (smb->initialized)
		power_supply_changed(smb->psy);

	return IRQ_HANDLED;
}

static const struct regulator_ops smb1360_regulator_ops = {
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.get_current_limit	= regulator_get_current_limit_regmap,
	.set_current_limit	= regulator_set_current_limit_regmap,
};

static const unsigned int smb1360_otg_current_limits[] = {
	350000, 550000, 950000, 1500000
};

static const struct regulator_desc smb1360_regulator_desc = {
	.name			= "usb_otg_vbus",
	.of_match		= "usb-otg-vbus",
	.ops			= &smb1360_regulator_ops,
	.type			= REGULATOR_VOLTAGE,
	.owner			= THIS_MODULE,
	.enable_reg		= CMD_CHG_REG,
	.enable_mask		= CMD_OTG_EN_BIT,
	.enable_val		= CMD_OTG_EN_BIT,
	.fixed_uV		= 5000000,
	.n_voltages		= 1,
	.curr_table		= smb1360_otg_current_limits,
	.n_current_limits	= ARRAY_SIZE(smb1360_otg_current_limits),
	.csel_reg		= CFG_BATT_CHG_REG,
	.csel_mask		= OTG_CURRENT_MASK,
};

static int smb1360_register_vbus_regulator(struct smb1360 *smb)
{
	struct regulator_config cfg = {
		.dev = smb->dev,
	};

	smb->otg_vreg = devm_regulator_register(smb->dev,
						&smb1360_regulator_desc, &cfg);
	if (IS_ERR(smb->otg_vreg)) {
		dev_err(smb->dev, "can't register regulator: %pe\n", smb->otg_vreg);
		return PTR_ERR(smb->otg_vreg);
	}

	return 0;
}

static int smb1360_enable_fg_access(struct smb1360 *smb)
{
	unsigned int reg;
	int ret;

	ret = regmap_read(smb->regmap, IRQ_I_REG, &reg);
	if (ret || reg & IRQ_I_FG_ACCESS_ALLOWED_BIT)
		goto err;

	/* request FG access */
	ret = regmap_set_bits(smb->regmap, CMD_I2C_REG, FG_ACCESS_ENABLED_BIT);
	if (ret)
		goto err;

	ret = wait_for_completion_timeout(&smb->fg_mem_access_granted,
					  msecs_to_jiffies(SMB1360_FG_ACCESS_TIMEOUT_MS));

	if (ret == 0) {
		/* Clear the FG access bit if request failed */
		dev_err(smb->dev, "enable FG access timed out\n");
		regmap_clear_bits(smb->regmap, CMD_I2C_REG, FG_ACCESS_ENABLED_BIT);
		return -ETIMEDOUT;
	}

	return 0;

err:
	dev_err(smb->dev, "failed to enable fg access: %d\n", ret);
	return ret;
}

static int smb1360_disable_fg_access(struct smb1360 *smb)
{
	int ret;

	ret = regmap_clear_bits(smb->regmap, CMD_I2C_REG, FG_ACCESS_ENABLED_BIT);
	if (ret)
		dev_err(smb->dev, "couldn't disable FG access: %d\n", ret);

	reinit_completion(&smb->fg_mem_access_granted);

	return ret;
}

static int smb1360_force_fg_reset(struct smb1360 *smb)
{
	int ret;

	dev_dbg(smb->dev, "forcing FG reset!\n");

	ret = regmap_set_bits(smb->regmap, CMD_I2C_REG, FG_RESET_BIT);
	if (ret) {
		dev_err(smb->dev, "couldn't reset FG: %d\n", ret);
		return ret;
	}

	msleep(SMB1360_FG_RESET_DELAY_MS);

	ret = regmap_clear_bits(smb->regmap, CMD_I2C_REG, FG_RESET_BIT);
	if (ret)
		dev_err(smb->dev, "couldn't un-reset FG: %d\n", ret);

	return ret;
}

static int smb1360_fg_reset(struct smb1360 *smb)
{
	int ret, temp, v_predicted, v_now;
	u32 val;

	if (!device_property_read_bool(smb->dev, "qcom,fg-reset-at-pon"))
		return 0;

	ret = smb1360_enable_fg_access(smb);
	if (ret)
		return ret;

	ret = smb1360_read_voltage(smb, VOLTAGE_PREDICTED_REG, &v_predicted);
	if (ret)
		goto disable_fg_access;
	ret = smb1360_read_voltage(smb, SHDW_FG_VTG_NOW, &v_now);
	if (ret)
		goto disable_fg_access;

	if (device_property_read_u32(smb->dev, "qcom,fg-reset-threshold-mv", &val))
		val = FG_RESET_THRESHOLD_MV;

	temp = abs(v_predicted - v_now);
	dev_dbg(smb->dev, "FG reset: predicted: %d, now: %d, delta: %d, threshold: %d\n",
		v_predicted, v_now, temp, val);
	if (temp >= val) {
		/* delay for the FG access to settle */
		msleep(1500);

		ret = smb1360_force_fg_reset(smb);
		if (ret)
			goto disable_fg_access;
	}

disable_fg_access:
	smb1360_disable_fg_access(smb);
	return ret;
}

static int smb1360_check_batt_profile(struct smb1360 *smb)
{
	u32 profile, loaded_profile;
	unsigned int val;
	int ret, timeout;

	if (device_property_read_u32(smb->dev, "qcom,battery-profile", &profile))
		return 0;

	if (profile > 1) {
		dev_err(smb->dev, "invalid battery profile: %d\n", profile);
		return -EINVAL;
	}

	ret = regmap_read(smb->regmap, SHDW_FG_BATT_STATUS, &val);
	if (ret)
		return ret;

	loaded_profile = !!(val & BATTERY_PROFILE_BIT);
	dev_dbg(smb->dev, "profile: %d, loaded_profile: %d\n", profile, loaded_profile);

	if (loaded_profile == profile)
		return 0;

	ret = regmap_update_bits(smb->regmap, CFG_FG_BATT_CTRL_REG,
				 BATT_PROFILE_SELECT_MASK,
				 profile ? BATT_PROFILEB_MASK : BATT_PROFILEA_MASK);
	if (ret)
		return ret;

	ret = smb1360_enable_fg_access(smb);
	if (ret)
		return ret;

	/* delay after handshaking for profile-switch to continue */
	msleep(1500);

	ret = smb1360_force_fg_reset(smb);
	if (ret) {
		smb1360_disable_fg_access(smb);
		return ret;
	}
	ret = smb1360_disable_fg_access(smb);
	if (ret)
		return ret;

	for (timeout = 0; timeout < 10; ++timeout) {
		/* delay for profile to change */
		msleep(500);
		ret = regmap_read(smb->regmap, SHDW_FG_BATT_STATUS, &val);
		if (ret)
			return ret;

		loaded_profile = !!(val & BATTERY_PROFILE_BIT);
		if (loaded_profile == profile)
			return 0;
	}

	return -ETIMEDOUT;
}

static int smb1360_adjust_otp_current_gain(struct smb1360 *smb)
{
	int ret;
	u8 val[4];
	__le16 current_gain;
	u16 current_gain_encoded;

	ret = regmap_raw_read(smb->fg_regmap, CURRENT_GAIN_LSB_REG,
			      &current_gain, sizeof(current_gain));
	if (ret)
		return ret;

	current_gain_encoded = le16_to_cpu(current_gain);
	current_gain_encoded = float_encode(MICRO_UNIT + (2 * float_decode(current_gain_encoded)));

	val[0] = OTP_GAIN_FIRST_HALF_REG_ADDR;
	val[1] = current_gain_encoded & 0xFF;
	val[2] = OTP_GAIN_SECOND_HALF_REG_ADDR;
	val[3] = (current_gain_encoded & 0xFF00) >> 8;

	return regmap_raw_write(smb->fg_regmap, OTP_WRITABLE_REG_1, val, ARRAY_SIZE(val));
}

static int smb1360_set_otp_hard_jeita_threshold(struct smb1360 *smb)
{
	u8 val[4];
	s32 hot, cold;

	if (device_property_read_u32(smb->dev, "qcom,otp-hot-bat-decidegc", &hot))
		return -EINVAL;
	if (device_property_read_u32(smb->dev, "qcom,otp-cold-bat-decidegc", &cold))
		return -EINVAL;

	val[0] = OTP_HARD_HOT_REG_ADDR;
	val[1] = TEMP_THRE_SET(hot);
	val[2] = OTP_HARD_COLD_REG_ADDR;
	val[3] = TEMP_THRE_SET(cold);

	if (val[1] < 0 || val[3] < 0)
		return -EINVAL;

	return regmap_raw_write(smb->fg_regmap, OTP_WRITABLE_REG_5, val, ARRAY_SIZE(val));
}

static int smb1360_reconf_otp(struct smb1360 *smb)
{
	bool hard_jeita = device_property_read_bool(smb->dev, "qcom,otp-hard-jeita-config");
	u16 backup_map = 0;
	__be16 val;
	int ret;

	if (!smb->rsense_10mohm && !hard_jeita)
		return 0;

	ret = smb1360_enable_fg_access(smb);
	if (ret)
		return ret;

	if (smb->rsense_10mohm) {
		ret = smb1360_adjust_otp_current_gain(smb);
		if (ret)
			dev_err(smb->dev,
				"couldn't reconfigure gain for lower resistance: %d\n", ret);
		else
			backup_map |= CURRENT_GAIN_BITMAP;
	}

	if (hard_jeita) {
		ret = smb1360_set_otp_hard_jeita_threshold(smb);
		if (ret)
			dev_err(smb->dev, "unable to modify otp hard jeita: %d\n", ret);
		else
			backup_map |= HARD_JEITA_BITMAP;
	}

	val = cpu_to_be16(backup_map);
	ret = regmap_raw_write(smb->fg_regmap, OTP_BACKUP_MAP_REG, &val, sizeof(val));
	if (ret)
		goto out;

	ret = regmap_set_bits(smb->regmap, CFG_FG_BATT_CTRL_REG, CFG_FG_OTP_BACK_UP_ENABLE);
	if (ret)
		dev_err(smb->dev, "failed to enable OTP back-up: %d\n", ret);

out:
	return smb1360_disable_fg_access(smb);
}

static int smb1360_update_bounds(struct smb1360 *smb)
{
	unsigned int val;
	int ret;

	/* REV_1 does not allow access to FG config registers */
	if (smb->revision == SMB1360_REV_1)
		return 0;

	val = abs(((SOC_DELTA_VAL * U8_MAX) / 100) - 1);
	ret = regmap_write(smb->regmap, SOC_DELTA_REG, val);
	if (ret)
		return ret;

	val = DIV_ROUND_CLOSEST(SOC_MIN_VAL * U8_MAX, 100);
	ret = regmap_write(smb->regmap, SOC_MIN_REG, val);
	if (ret)
		return ret;

	if (device_property_read_u32(smb->dev, "qcom,fg-voltage-min-mv", &val) == 0) {
		val = (val - 2500) * U8_MAX;
		val = DIV_ROUND_UP(val, 2500);
		ret = regmap_write(smb->regmap, VTG_MIN_REG, val);
		if (ret)
			return ret;
	}

	if (device_property_read_u32(smb->dev, "qcom,fg-voltage-empty-mv", &val) == 0) {
		val = (val - 2500) * U8_MAX;
		val = DIV_ROUND_UP(val, 2500);
		ret = regmap_write(smb->regmap, VTG_EMPTY_REG, val);
		if (ret)
			return ret;
	}

	return 0;
}

static int smb1360_update_autorecharge_soc_threshold(struct smb1360 *smb)
{
	int ret;
	u32 val;

	if (device_property_read_u32(smb->dev, "qcom,fg-auto-recharge-soc", &val))
		return 0;

	ret = regmap_set_bits(smb->regmap, CFG_CHG_FUNC_CTRL_REG,
			      CHG_RECHG_THRESH_FG_SRC_BIT);
	if (ret)
		return ret;

	val = DIV_ROUND_UP(val * U8_MAX, 100);
	return regmap_write(smb->regmap, FG_AUTO_RECHARGE_SOC, val);
}

static int smb1360_fg_config(struct smb1360 *smb)
{
	int ret;

	ret = smb1360_enable_fg_access(smb);
	if (ret)
		return ret;

	ret = smb1360_update_le16(smb, ACTUAL_CAPACITY_REG, "qcom,fg-batt-capacity-mah", 0);
	if (ret)
		goto disable_fg_access;

	ret = smb1360_update_le16(smb, NOMINAL_CAPACITY_REG, "qcom,fg-batt-capacity-mah", 0);
	if (ret)
		goto disable_fg_access;

	ret = smb1360_update_le16(smb, CC_TO_SOC_COEFF, "qcom,fg-cc-soc-coeff", 0);
	if (ret)
		goto disable_fg_access;

	ret = smb1360_update_le16(smb, FG_SYS_CUTOFF_V_REG, "qcom,fg-cutoff-voltage-mv", 5000);
	if (ret)
		goto disable_fg_access;

	ret = smb1360_update_le16(smb, FG_ITERM_REG, "qcom,fg-iterm-ma", -2500);
	if (ret)
		goto disable_fg_access;

	ret = smb1360_update_le16(smb, FG_IBATT_STANDBY_REG, "qcom,fg-ibatt-standby-ma", 2500);
	if (ret)
		goto disable_fg_access;

	ret = smb1360_update_le16(smb, FG_CC_TO_CV_V_REG, "qcom,fg-cc-to-cv-mv", 5000);
	if (ret)
		goto disable_fg_access;

	ret = smb1360_update_le16(smb, FG_THERM_C1_COEFF_REG, "qcom,thermistor-c1-coeff", 0);
	if (ret)
		goto disable_fg_access;

	ret = smb1360_update_autorecharge_soc_threshold(smb);
	if (ret) {
		dev_err(smb->dev, "smb1360_update_autorecharge_soc_threshold failed\n");
		goto disable_fg_access;
	}

disable_fg_access:
	smb1360_disable_fg_access(smb);
	return ret;
}

static int smb1360_check_cycle_stretch(struct smb1360 *smb)
{
	unsigned int val;
	int ret;

	ret = regmap_read(smb->regmap, STATUS_4_REG, &val);
	if (ret)
		return ret;

	if (!(val & CYCLE_STRETCH_ACTIVE_BIT))
		return 0;

	ret = regmap_set_bits(smb->regmap, CMD_I2C_REG, CYCLE_STRETCH_CLEAR_BIT);
	if (ret)
		dev_err(smb->dev, "unable to clear cycle stretch: %d\n", ret);

	return ret;
}

#ifdef CONFIG_SMB1360_DEBUG
extern void smb1360_dump(struct i2c_client *client);
extern void smb1360_dump_fg_scratch(struct i2c_client *fg_client);
extern void smb1360_dump_fg(struct i2c_client *client);

static void smb1360_dump_fg_access(struct smb1360 *smb)
{
	struct i2c_client *client = to_i2c_client(smb->dev);
	struct i2c_client *fg_client = to_i2c_client(regmap_get_device(smb->fg_regmap));
	int ret;

	ret = smb1360_enable_fg_access(smb);
	if (ret)
		return;

	smb1360_dump_fg_scratch(client);
	smb1360_dump_fg(fg_client);

	smb1360_disable_fg_access(smb);
	smb1360_check_cycle_stretch(smb);
}
#else
static inline void smb1360_dump(struct i2c_client *client) {}
static inline void smb1360_dump_fg_access(struct smb1360 *smb) {}
#endif

static int smb1360_delayed_hw_init(struct smb1360 *smb)
{
	int ret;

	/* Dump initial FG registers */
	smb1360_dump_fg_access(smb);

	ret = smb1360_check_batt_profile(smb);
	if (ret) {
		dev_err(smb->dev, "unable to modify battery profile: %d\n", ret);
		return ret;
	}

	ret = smb1360_reconf_otp(smb);
	if (ret) {
		dev_err(smb->dev, "couldn't reconfigure OTP: %d\n", ret);
		return ret;
	}

	ret = smb1360_fg_reset(smb);
	if (ret)
		dev_err(smb->dev, "smb1360_fg_reset failed: %d\n", ret);

	ret = smb1360_update_bounds(smb);
	if (ret) {
		dev_err(smb->dev, "couldn't configure SOC/voltage bounds: %d\n", ret);
		return ret;
	}

	ret = smb1360_fg_config(smb);
	if (ret) {
		dev_err(smb->dev, "couldn't configure FG: %d\n", ret);
		return ret;
	}

	ret = smb1360_check_cycle_stretch(smb);
	if (ret) {
		dev_err(smb->dev, "Unable to check cycle-stretch\n");
		return ret;
	}

	ret = regmap_set_bits(smb->regmap, CMD_CHG_REG, CMD_CHG_EN);
	if (ret) {
		dev_err(smb->dev, "couldn't enable battery charging: %d\n", ret);
		return ret;
	}

	/* Dump final registers */
	smb1360_dump(to_i2c_client(smb->dev));
	smb1360_dump_fg_access(smb);

	return 0;
}

static void smb1360_delayed_init_work_fn(struct work_struct *work)
{
	int ret = 0;
	struct smb1360 *smb = container_of(work, struct smb1360,
					   delayed_init_work.work);

	ret = smb1360_delayed_hw_init(smb);
	if (!ret) {
		power_supply_changed(smb->psy);
		smb->initialized = true;
	} else if (ret == -ETIMEDOUT) {
		ret = smb1360_force_fg_reset(smb);
		if (ret)
			return;
		schedule_delayed_work(&smb->delayed_init_work, 0);
	}
}

static int smb1360_set_shutdown(struct smb1360 *smb, bool shutdown)
{
	int ret = 0;
	unsigned int val = 0;
	bool polarity;

	ret = regmap_read(smb->regmap, SHDN_CTRL_REG, &val);
	if (ret < 0) {
		dev_err(smb->dev, "couldn't read SHDN_CTRL_REG: %d\n", ret);
		return ret;
	}

	if (!(val & SHDN_CMD_USE_BIT))
		return 0;

	polarity = !!(val & SHDN_CMD_POLARITY_BIT);
	val = (polarity == shutdown) ? SHDN_CMD_BIT : 0;

	ret = regmap_update_bits(smb->regmap, CMD_IL_REG, SHDN_CMD_BIT, val);
	if (ret < 0)
		dev_err(smb->dev, "couldn't update shutdown: %d\n", ret);

	return ret;
}

static inline int smb1360_poweroff(struct smb1360 *smb)
{
	return smb1360_set_shutdown(smb, true);
}

static inline int smb1360_poweron(struct smb1360 *smb)
{
	return smb1360_set_shutdown(smb, false);
}

static int smb1360_float_voltage_set(struct smb1360 *smb)
{
	u32 val;
	int ret;

	if (device_property_read_u32(smb->dev, "qcom,float-voltage-mv", &val)) {
		/* Read float voltage from registers */
		ret = regmap_read(smb->regmap, BATT_CHG_FLT_VTG_REG, &val);
		if (ret)
			return ret;

		val &= VFLOAT_MASK;
		smb->float_voltage = (val * VFLOAT_STEP_MV) + MIN_FLOAT_MV;
		return 0;
	}

	if (val < MIN_FLOAT_MV || val > MAX_FLOAT_MV)
		return -EINVAL;

	smb->float_voltage = val;
	val = (val - MIN_FLOAT_MV) / VFLOAT_STEP_MV;

	return regmap_update_bits(smb->regmap,
				  BATT_CHG_FLT_VTG_REG, VFLOAT_MASK, val);
}

static int smb1360_iterm_set(struct smb1360 *smb)
{
	int ret, iterm_ma;
	u8 val;

	if (device_property_read_bool(smb->dev, "qcom,iterm-disabled"))
		return regmap_set_bits(smb->regmap, CFG_CHG_MISC_REG,
				       CHG_CURR_TERM_DIS_BIT);

	if (device_property_read_u32(smb->dev, "qcom,iterm-ma", &iterm_ma))
		return 0;

	if (smb->rsense_10mohm)
		iterm_ma = iterm_ma / 2;

	val = clamp(iterm_ma, 25, 200);
	val = DIV_ROUND_UP(val, 25) - 1;

	ret = regmap_update_bits(smb->regmap, CFG_BATT_CHG_REG,
				 CHG_ITERM_MASK, val);
	if (ret)
		return ret;

	ret = regmap_clear_bits(smb->regmap, CFG_CHG_MISC_REG,
				CHG_CURR_TERM_DIS_BIT);
	if (ret)
		return ret;

	return 0;
}

static int smb1360_safety_time_set(struct smb1360 *smb)
{
	static const int chg_time[] = { 192, 384, 768, 1536, };
	int ret, i;

	u32 val;
	u8 mask, data;

	if (device_property_read_u32(smb->dev, "qcom,charging-timeout", &val))
		return 0;

	if (val > chg_time[ARRAY_SIZE(chg_time) - 1])
		return -EINVAL;

	mask = SAFETY_TIME_DISABLE_BIT;
	data = SAFETY_TIME_DISABLE_BIT;

	if (val != 0) {
		mask = SAFETY_TIME_DISABLE_BIT | SAFETY_TIME_MINUTES_MASK;

		for (i = 0; i < ARRAY_SIZE(chg_time); i++) {
			if (val <= chg_time[i]) {
				data = i << SAFETY_TIME_MINUTES_SHIFT;
				break;
			}
		}
	}

	ret = regmap_update_bits(smb->regmap,
				 CFG_SFY_TIMER_CTRL_REG, mask, data);
	if (ret < 0) {
		dev_err(smb->dev, "couldn't update safety timer: %d\n", ret);
		return ret;
	}

	return 0;
}

static int smb1360_recharge_threshold_set(struct smb1360 *smb)
{
	u32 val;

	if (device_property_read_u32(smb->dev, "qcom,recharge-thresh-mv", &val))
		return 0;

	if (device_property_read_bool(smb->dev, "qcom,recharge-disabled") &&
	    device_property_read_bool(smb->dev, "qcom,chg-inhibit-disabled")) {
		dev_err(smb->dev, "recharge: both disabled and mv set\n");
		return -EINVAL;
	}

	if (val < MIN_RECHG_MV || val > MAX_RECHG_MV)
		return -EINVAL;

	val = (val / 100) << RECHG_MV_SHIFT;

	return regmap_update_bits(smb->regmap,
				  CFG_BATT_CHG_REG, RECHG_MV_MASK, val);
}

static int smb1360_update_temp_tresh(struct smb1360 *smb, u8 reg, const char *prop)
{
	int ret;
	s32 temp;

	if (device_property_read_u32(smb->dev, prop, &temp))
		return 0;

	ret = regmap_write(smb->regmap, reg, TEMP_THRE_SET(temp));
	if (ret)
		dev_err(smb->dev, "writing %s failed: %d\n", prop, ret);
	return ret;
}

static int smb1360_find_fastchg_current(struct smb1360 *smb, int current_ma)
{
	static const int fastchg_current[] = {
		450, 600, 750, 900, 1050, 1200, 1350, 1500,
	};
	int i;

	for (i = ARRAY_SIZE(fastchg_current) - 1; i >= 0; i--) {
		if (fastchg_current[i] <= current_ma)
			return i;
	}

	dev_err(smb->dev, "cannot find fastchg current %d\n", current_ma);
	return -EINVAL;
}

static int smb1360_jeita_init(struct smb1360 *smb)
{
	int ret;
	unsigned int tmp;
	u32 comp_volt, comp_curr;

	ret = smb1360_update_temp_tresh(smb, JEITA_SOFT_COLD_REG, "qcom,cool-bat-decidegc");
	if (ret)
		return ret;
	ret = smb1360_update_temp_tresh(smb, JEITA_SOFT_HOT_REG, "qcom,warm-bat-decidegc");
	if (ret)
		return ret;

	if (!device_property_read_bool(smb->dev, "qcom,soft-jeita-config"))
		return 0;

	if (device_property_read_u32(smb->dev, "qcom,soft-jeita-comp-voltage-mv", &comp_volt) ||
	    device_property_read_u32(smb->dev, "qcom,soft-jeita-comp-current-ma", &comp_curr)) {
		dev_err(smb->dev, "qcom,soft-jeita-comp-{voltage,current} required for soft JEITA\n");
		return -EINVAL;
	}

	if (comp_volt >= smb->float_voltage) {
		dev_err(smb->dev, "JEITA compensation voltage larger than float voltage\n");
		return -EINVAL;
	}

	tmp = (smb->float_voltage - comp_volt) / 10;
	ret = regmap_update_bits(smb->regmap, CFG_FVC_REG, FLT_VTG_COMP_MASK, tmp);
	if (ret)
		return ret;

	ret = smb1360_find_fastchg_current(smb, comp_curr);
	if (ret < 0)
		return ret;

	/* Write compensation current and enable JEITA compensation */
	return regmap_write(smb->regmap, CHG_CMP_CFG, ret | JEITA_COMP_EN_BIT);
}

static int smb1360_configure_irq(struct smb1360 *smb)
{
	int ret;

	/* enabling only interesting interrupts */
	ret = regmap_write(smb->regmap, IRQ_CFG_REG,
			   IRQ_INTERNAL_TEMPERATURE_BIT
			   | IRQ_DCIN_UV_BIT
			   | IRQ_BAT_HOT_COLD_SOFT_BIT
			   | IRQ_HOT_COLD_HARD_BIT);
	if (ret) {
		dev_err(smb->dev, "couldn't set irq1: %d\n", ret);
		return ret;
	}

	ret = regmap_write(smb->regmap, IRQ2_CFG_REG,
			   IRQ2_VBAT_LOW_BIT
			   | IRQ2_BATT_MISSING_BIT
			   | IRQ2_POWER_OK_BIT
			   | IRQ2_CHG_PHASE_CHANGE_BIT
			   | IRQ2_CHG_ERR_BIT
			   | IRQ2_SAFETY_TIMER_BIT);
	if (ret) {
		dev_err(smb->dev, "couldn't set irq2: %d\n", ret);
		return ret;
	}

	ret = regmap_write(smb->regmap, IRQ3_CFG_REG,
			   IRQ3_SOC_FULL_BIT
			   | IRQ3_SOC_EMPTY_BIT
			   | IRQ3_SOC_MAX_BIT
			   | IRQ3_SOC_MIN_BIT
			   | IRQ3_SOC_CHANGE_BIT
			   | IRQ3_FG_ACCESS_OK_BIT);
	if (ret)
		dev_err(smb->dev, "couldn't set irq3: %d\n", ret);

	return ret;
}

static int smb1360_hw_init(struct i2c_client *client)
{
	struct smb1360 *smb = i2c_get_clientdata(client);
	int ret;
	u8 val;

	ret = regmap_set_bits(smb->regmap, CMD_I2C_REG, ALLOW_VOLATILE_BIT);
	if (ret < 0) {
		dev_err(smb->dev, "couldn't configure volatile: %d\n", ret);
		return ret;
	}

	/* Bring SMB1360 out of shutdown, if it was enabled by default */
	ret = smb1360_poweron(smb);
	if (ret < 0) {
		dev_err(smb->dev, "smb1360 power on failed\n");
		return ret;
	}

	/* en chg by cmd reg, en chg by writing bit 1, en auto pre to fast */
	ret = regmap_clear_bits(smb->regmap, CFG_CHG_MISC_REG,
				CHG_EN_BY_PIN_BIT | CHG_EN_ACTIVE_LOW_BIT
				| PRE_TO_FAST_REQ_CMD_BIT);
	if (ret < 0)
		return ret;

	/* USB/AC pin settings */
	ret = regmap_update_bits(smb->regmap, CFG_BATT_CHG_ICL_REG,
				 AC_INPUT_ICL_PIN_BIT | AC_INPUT_PIN_HIGH_BIT,
				 AC_INPUT_PIN_HIGH_BIT);
	if (ret < 0)
		return ret;

	/* AICL enable and set input-uv glitch flt to 20ms */
	ret = regmap_set_bits(smb->regmap, CFG_GLITCH_FLT_REG,
			      AICL_ENABLED_BIT | INPUT_UV_GLITCH_FLT_20MS_BIT);
	if (ret < 0)
		return ret;

	ret = smb1360_float_voltage_set(smb);
	if (ret < 0)
		return ret;

	ret = smb1360_iterm_set(smb);
	if (ret < 0)
		return ret;

	ret = smb1360_safety_time_set(smb);
	if (ret < 0)
		return ret;

	ret = smb1360_recharge_threshold_set(smb);
	if (ret)
		return ret;

	/* Always stop charging on over-voltage condition */
	val = CFG_BAT_OV_ENDS_CHG_CYC;
	if (device_property_read_bool(smb->dev, "qcom,recharge-disabled"))
		val |= CFG_AUTO_RECHG_DIS_BIT;
	if (!device_property_read_bool(smb->dev, "qcom,chg-inhibit-disabled"))
		val |= CFG_CHG_INHIBIT_EN_BIT;

	ret = regmap_update_bits(smb->regmap, CFG_CHG_MISC_REG,
				 CFG_BAT_OV_ENDS_CHG_CYC
				 | CFG_AUTO_RECHG_DIS_BIT
				 | CFG_CHG_INHIBIT_EN_BIT, val);
	if (ret) {
		dev_err(smb->dev, "couldn't set bat_ov_ends_charge/rechg/chg_inhibit: %d\n", ret);
		return ret;
	}

	ret = smb1360_jeita_init(smb);
	if (ret) {
		dev_err(smb->dev, "couldn't init jeita: %d\n", ret);
		return ret;
	}

	/* interrupt enabling - active low */
	if (client->irq) {
		ret = regmap_update_bits(smb->regmap, CFG_STAT_CTRL_REG,
					 CHG_STAT_IRQ_ONLY_BIT
					 | CHG_STAT_ACTIVE_HIGH_BIT
					 | CHG_STAT_DISABLE_BIT
					 | CHG_TEMP_CHG_ERR_BLINK_BIT,
					 CHG_STAT_IRQ_ONLY_BIT);
		if (ret < 0) {
			dev_err(smb->dev, "couldn't set irq: %d\n", ret);
			return ret;
		}

		ret = smb1360_configure_irq(smb);
		if (ret < 0) {
			dev_err(smb->dev, "couldn't configure irq: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int smb1360_parse_properties(struct smb1360 *smb)
{
	struct device *dev = smb->dev;

	smb->shdn_after_pwroff = device_property_read_bool(dev, "qcom,shdn-after-pwroff");
	smb->rsense_10mohm = device_property_read_bool(dev, "qcom,rsense-10mohm");

	return 0;
}

static const struct regmap_config smb1360_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
};

static const struct power_supply_desc smb1360_battery_desc = {
	.name			= "smb1360-battery",
	.type			= POWER_SUPPLY_TYPE_BATTERY,
	.get_property		= smb1360_get_property,
	.properties		= smb1360_props,
	.num_properties		= ARRAY_SIZE(smb1360_props),
};

static int smb1360_probe(struct i2c_client *client)
{
	int ret;
	struct power_supply_config psy_cfg = {};
	struct device *dev = &client->dev;
	struct i2c_client *fg_client;
	struct smb1360 *smb;
	u16 fg_address;

	if (client->addr & FG_I2C_CFG_MASK) {
		dev_err(&client->dev, "invalid i2c address: %#x\n", client->addr);
		return -EINVAL;
	}

	smb = devm_kzalloc(&client->dev, sizeof(*smb), GFP_KERNEL);
	if (!smb)
		return -EINVAL;

	smb->dev = dev;

	smb->regmap = devm_regmap_init_i2c(client, &smb1360_regmap_config);
	if (IS_ERR(smb->regmap)) {
		dev_err(&client->dev, "failed to init regmap\n");
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&smb->delayed_init_work,
			  smb1360_delayed_init_work_fn);
	init_completion(&smb->fg_mem_access_granted);

	ret = regmap_read(smb->regmap, REVISION_CTRL_REG, &smb->revision);
	if (ret) {
		dev_err(smb->dev, "couldn't read revision: %d\n", ret);
		return ret;
	}
	smb->revision &= DEVICE_REV_MASK;
	dev_dbg(smb->dev, "device revision: %d\n", smb->revision);

	fg_address = client->addr | FG_CFG_I2C_ADDR;
	fg_client = devm_i2c_new_dummy_device(dev, client->adapter, fg_address);
	if (IS_ERR(fg_client)) {
		dev_err(&client->dev, "failed to init fg i2c client\n");
		return -EINVAL;
	}

	smb->fg_regmap = devm_regmap_init_i2c(fg_client, &smb1360_regmap_config);
	if (IS_ERR(smb->fg_regmap)) {
		dev_err(&client->dev, "failed to init fg regmap\n");
		return -EINVAL;
	}

	ret = smb1360_parse_properties(smb);
	if (ret) {
		dev_err(&client->dev, "error parsing device tree: %d\n", ret);
		return ret;
	}

	device_init_wakeup(smb->dev, 1);
	i2c_set_clientdata(client, smb);

	/* Dump initial registers */
	smb1360_dump(client);

	ret = smb1360_hw_init(client);
	if (ret < 0) {
		dev_err(&client->dev, "unable to initialize hw: %d\n", ret);
		return ret;
	}

	ret = regmap_raw_read(smb->regmap, IRQ_REG, smb->irqstat, sizeof(smb->irqstat));
	if (ret < 0) {
		dev_err(&client->dev, "unable to determine init status: %d\n", ret);
		return ret;
	}

	smb->edev = devm_extcon_dev_allocate(dev, smb1360_usb_extcon_cable);
	if (IS_ERR(smb->edev))
		return PTR_ERR(smb->edev);

	ret = devm_extcon_dev_register(dev, smb->edev);
	if (ret < 0)
		return ret;

	extcon_set_state_sync(smb->edev, EXTCON_USB,
			      !(smb->irqstat[IRQ_E] & IRQ_E_USBIN_UV_BIT));

	ret = smb1360_register_vbus_regulator(smb);
	if (ret < 0)
		return ret;

	psy_cfg.drv_data = smb;
	smb->psy = devm_power_supply_register(&client->dev, &smb1360_battery_desc,
					      &psy_cfg);
	if (IS_ERR(smb->psy)) {
		dev_err(&client->dev, "failed to register power supply\n");
		ret = PTR_ERR(smb->psy);
		return ret;
	}

	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq, NULL,
						smb1360_irq, IRQF_ONESHOT,
						NULL, smb);
		if (ret) {
			dev_err(&client->dev,
				"request irq %d failed\n", client->irq);
			return ret;
		}

		enable_irq_wake(client->irq);
	}

	schedule_delayed_work(&smb->delayed_init_work,
			      msecs_to_jiffies(SMB1360_POWERON_DELAY_MS));

	return 0;
}

static void smb1360_shutdown(struct i2c_client *client)
{
	int ret;
	struct smb1360 *smb = i2c_get_clientdata(client);

	ret = regulator_disable_regmap(smb->otg_vreg);
	if (ret)
		dev_err(smb->dev, "couldn't disable OTG: %d\n", ret);

	if (smb->shdn_after_pwroff) {
		ret = smb1360_poweroff(smb);
		if (ret)
			dev_err(smb->dev, "couldn't shutdown: %d\n", ret);
	}
}

static int smb1360_suspend(struct device *dev)
{
	int ret;
	struct smb1360 *smb = dev_get_drvdata(dev);

	ret = regmap_write(smb->regmap, IRQ_CFG_REG, IRQ_DCIN_UV_BIT
						| IRQ_BAT_HOT_COLD_SOFT_BIT
						| IRQ_HOT_COLD_HARD_BIT);
	if (ret < 0)
		dev_err(smb->dev, "couldn't set irq_cfg: %d\n", ret);

	ret = regmap_write(smb->regmap, IRQ2_CFG_REG, IRQ2_BATT_MISSING_BIT
						| IRQ2_VBAT_LOW_BIT
						| IRQ2_POWER_OK_BIT);
	if (ret < 0)
		dev_err(smb->dev, "couldn't set irq2_cfg: %d\n", ret);

	ret = regmap_write(smb->regmap, IRQ3_CFG_REG, IRQ3_SOC_FULL_BIT
					| IRQ3_SOC_MIN_BIT
					| IRQ3_SOC_EMPTY_BIT);
	if (ret < 0)
		dev_err(smb->dev, "couldn't set irq3_cfg: %d\n", ret);

	return 0;
}

static int smb1360_resume(struct device *dev)
{
	int ret;
	struct smb1360 *smb = dev_get_drvdata(dev);

	ret = smb1360_configure_irq(smb);
	if (ret)
		return ret;

	power_supply_changed(smb->psy);

	return 0;
}

static const struct dev_pm_ops smb1360_pm_ops = {
	.resume = smb1360_resume,
	.suspend = smb1360_suspend,
};

#ifdef CONFIG_OF
static const struct of_device_id smb1360_match_table[] = {
	{ .compatible = "qcom,smb1360" },
	{ },
};
MODULE_DEVICE_TABLE(of, smb1360_match_table);
#endif

static struct i2c_driver smb1360_driver = {
	.driver	= {
		.name = "smb1360",
		.of_match_table = of_match_ptr(smb1360_match_table),
		.pm = &smb1360_pm_ops,
	},
	.probe_new = smb1360_probe,
	.shutdown = smb1360_shutdown,
};

module_i2c_driver(smb1360_driver);

MODULE_DESCRIPTION("SMB1360 Charger and Fuel Gauge");
MODULE_LICENSE("GPL v2");
