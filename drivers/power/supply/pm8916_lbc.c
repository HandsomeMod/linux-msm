// SPDX-License-Identifier: GPL-2.0-only

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/extcon-provider.h>

/* Two bytes: type + subtype */
#define PM8916_PERPH_TYPE 0x04
#define PM8916_LBC_CHGR_TYPE 0x1502
#define PM8916_LBC_BAT_IF_TYPE 0x1602
#define PM8916_LBC_USB_TYPE 0x1702
#define PM8916_LBC_MISC_TYPE 0x1802

#define PM8916_LBC_CHGR_CHG_OPTION 0x08
#define PM8916_LBC_CHGR_PMIC_CHARGER BIT(7)

#define PM8916_LBC_CHGR_CHG_STATUS 0x09

#define PM8916_INT_RT_STS 0x10

#define PM8916_LBC_USB_USBIN_VALID BIT(1)

#define PM8916_LBC_CHGR_VDD_MAX 0x40
#define PM8916_LBC_CHGR_VDD_SAFE 0x41
#define PM8916_LBC_CHGR_IBAT_MAX 0x44
#define PM8916_LBC_CHGR_IBAT_SAFE 0x45

#define PM8916_LBC_CHGR_TCHG_MAX_EN 0x60
#define PM8916_LBC_CHGR_TCHG_MAX_ENABLED BIT(7)
#define PM8916_LBC_CHGR_TCHG_MAX 0x61

#define PM8916_LBC_CHGR_CHG_CTRL 0x49
#define PM8916_LBC_CHGR_CHG_EN BIT(7)
#define PM8916_LBC_CHGR_PSTG_EN BIT(5)

#define PM8916_LBC_CHGR_MIN_CURRENT 90000
#define PM8916_LBC_CHGR_MAX_CURRENT 1440000

#define PM8916_LBC_CHGR_MIN_VOLTAGE 4000000
#define PM8916_LBC_CHGR_MAX_VOLTAGE 4775000
#define PM8916_LBC_CHGR_VOLTAGE_STEP 25000

#define PM8916_LBC_CHGR_MIN_TIME 4
#define PM8916_LBC_CHGR_MAX_TIME 256

struct pm8916_lbc_charger {
	struct device *dev;
	struct extcon_dev *edev;
	struct power_supply_desc desc;
	struct power_supply *charger;
	struct power_supply_battery_info info;
	struct regmap *regmap;
	unsigned int reg[4];
	unsigned int charge_voltage_max;
	unsigned int charge_voltage_safe;
	unsigned int charge_current_max;
	unsigned int charge_current_safe;
};

static const unsigned int pm8916_lbc_charger_cable[] = {
	EXTCON_USB,
	EXTCON_NONE,
};

enum {
	LBC_CHGR = 0,
	LBC_BAT_IF,
	LBC_USB,
	LBC_MISC,
};

static int pm8916_lbc_charger_enable(struct pm8916_lbc_charger *chg)
{
	int ret = 0;
	unsigned int tmp;

	chg->charge_voltage_max = min_t(u32, max_t(u32, chg->charge_voltage_max,
					PM8916_LBC_CHGR_MIN_VOLTAGE), chg->charge_voltage_safe);

	tmp = (chg->charge_voltage_max - PM8916_LBC_CHGR_MIN_VOLTAGE);
	tmp /= PM8916_LBC_CHGR_VOLTAGE_STEP;

	ret = regmap_write(chg->regmap, chg->reg[LBC_CHGR] + PM8916_LBC_CHGR_VDD_MAX, tmp);
	if (ret)
		goto error;

	chg->charge_current_max = min(chg->charge_current_max, chg->charge_current_safe);

	tmp = max_t(u32, PM8916_LBC_CHGR_MIN_CURRENT,
		    min_t(u32, chg->charge_current_max, PM8916_LBC_CHGR_MAX_CURRENT));

	tmp = chg->charge_current_safe / PM8916_LBC_CHGR_MIN_CURRENT - 1;

	ret = regmap_write(chg->regmap, chg->reg[LBC_CHGR] + PM8916_LBC_CHGR_IBAT_MAX, tmp);
	if (ret)
		goto error;

	ret = regmap_write(chg->regmap, chg->reg[LBC_CHGR] + PM8916_LBC_CHGR_CHG_CTRL,
			   PM8916_LBC_CHGR_CHG_EN | PM8916_LBC_CHGR_PSTG_EN);
	if (ret)
		goto error;

	return ret;

error:
	dev_err(chg->dev, "Failed to enable charging: %d\n", ret);
	return ret;
}

static int pm8916_lbc_charger_get_property(struct power_supply *psy,
					   enum power_supply_property psp,
					   union power_supply_propval *val)
{
	struct pm8916_lbc_charger *chg = power_supply_get_drvdata(psy);
	int ret = 0;
	unsigned int tmp;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = regmap_read(chg->regmap, chg->reg[LBC_USB] + PM8916_INT_RT_STS, &tmp);
		if (ret)
			return ret;
		val->intval = !!(tmp & PM8916_LBC_USB_USBIN_VALID);
		return 0;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = chg->charge_current_max;
		return 0;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = chg->charge_voltage_max;
		return 0;

	default:
		return -EINVAL;
	};
}

static enum power_supply_property pm8916_lbc_charger_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
};

static irqreturn_t pm8916_lbc_charger_state_changed_irq(int irq, void *data)
{
	struct pm8916_lbc_charger *chg = data;
	unsigned int tmp;
	int ret;

	power_supply_changed(chg->charger);

	ret = regmap_read(chg->regmap, chg->reg[LBC_USB] + PM8916_INT_RT_STS, &tmp);
	if (ret)
		return IRQ_HANDLED;
	extcon_set_state_sync(chg->edev, EXTCON_USB, !!(tmp & PM8916_LBC_USB_USBIN_VALID));

	return IRQ_HANDLED;
}

static int pm8916_lbc_charger_probe_dt(struct pm8916_lbc_charger *chg)
{
	struct device *dev = chg->dev;
	struct device_node *np = dev->of_node;

	int ret = 0;
	unsigned int tmp;

	of_property_read_u32(np, "qcom,vdd-safe", &chg->charge_voltage_safe);
	if (chg->charge_voltage_safe < PM8916_LBC_CHGR_MIN_VOLTAGE)
		return -EINVAL;

	chg->charge_voltage_safe = max_t(u32, PM8916_LBC_CHGR_MIN_VOLTAGE, min_t(u32,
					 PM8916_LBC_CHGR_MAX_VOLTAGE, chg->charge_voltage_safe));

	chg->charge_voltage_max = chg->charge_voltage_safe;

	tmp = (chg->charge_voltage_safe - PM8916_LBC_CHGR_MIN_VOLTAGE);
	tmp /= PM8916_LBC_CHGR_VOLTAGE_STEP;

	ret = regmap_write(chg->regmap, chg->reg[LBC_CHGR] + PM8916_LBC_CHGR_VDD_SAFE, tmp);
	if (ret)
		return ret;

	of_property_read_u32(np, "qcom,ibat-safe", &chg->charge_current_safe);

	chg->charge_current_safe = max_t(u32, PM8916_LBC_CHGR_MIN_CURRENT, min_t(u32,
					 chg->charge_current_safe, PM8916_LBC_CHGR_MAX_CURRENT));

	// LBC will pull as much current as it can until it reaches this limit.
	chg->charge_current_max = chg->charge_current_safe;

	tmp = chg->charge_current_safe / PM8916_LBC_CHGR_MIN_CURRENT - 1;
	ret = regmap_write(chg->regmap, chg->reg[LBC_CHGR] + PM8916_LBC_CHGR_IBAT_SAFE, tmp);
	if (ret)
		return ret;

	ret = regmap_write(chg->regmap, chg->reg[LBC_CHGR] + PM8916_LBC_CHGR_TCHG_MAX_EN, 0x00);
	if (ret)
		return ret;

	return ret;
}

static int pm8916_lbc_charger_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pm8916_lbc_charger *chg;
	struct power_supply_config psy_cfg = {};
	struct power_supply_desc *desc;
	int ret, len, irq;
	unsigned int tmp;

	chg = devm_kzalloc(dev, sizeof(*chg), GFP_KERNEL);
	if (!chg)
		return -ENOMEM;

	chg->dev = dev;

	chg->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chg->regmap)
		return -ENODEV;

	len = of_property_count_u32_elems(dev->of_node, "reg");
	if (len < 0) {
		return len;
	} else if (len != 4) {
		dev_err(dev, "Wrong amount of reg values: %d (4 expected)\n", len);
		return ret;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_threaded_irq(dev, irq, NULL, pm8916_lbc_charger_state_changed_irq,
					IRQF_ONESHOT, "pm8916_lbc", chg);
	if (ret)
		return ret;

	ret = of_property_read_u32_array(dev->of_node, "reg", chg->reg, len);
	if (ret)
		return ret;

	ret = regmap_bulk_read(chg->regmap, chg->reg[LBC_CHGR] + PM8916_PERPH_TYPE, &tmp, 2);
	if (ret)
		goto comm_error;
	if (tmp != PM8916_LBC_CHGR_TYPE)
		goto type_error;

	ret = regmap_bulk_read(chg->regmap, chg->reg[LBC_BAT_IF] + PM8916_PERPH_TYPE, &tmp, 2);
	if (ret)
		goto comm_error;
	if (tmp != PM8916_LBC_BAT_IF_TYPE)
		goto type_error;

	ret = regmap_bulk_read(chg->regmap, chg->reg[LBC_USB] + PM8916_PERPH_TYPE, &tmp, 2);
	if (ret)
		goto comm_error;
	if (tmp != PM8916_LBC_USB_TYPE)
		goto type_error;

	ret = regmap_bulk_read(chg->regmap, chg->reg[LBC_MISC] + PM8916_PERPH_TYPE, &tmp, 2);
	if (ret)
		goto comm_error;
	if (tmp != PM8916_LBC_MISC_TYPE)
		goto type_error;

	ret = regmap_read(chg->regmap, chg->reg[LBC_CHGR] + PM8916_LBC_CHGR_CHG_OPTION, &tmp);
	if (ret)
		goto comm_error;
	if (tmp != PM8916_LBC_CHGR_PMIC_CHARGER) {
		dev_err(dev, "The system is using an external charger\n");
		return -ENODEV;
	}

	ret = pm8916_lbc_charger_probe_dt(chg);
	if (ret) {
		dev_err(dev, "Error while parsing device tree: %d\n", ret);
		return ret;
	}

	desc = &chg->desc;
	desc->name = "pm8916-lbc-chgr";
	desc->type = POWER_SUPPLY_TYPE_USB;
	desc->properties = pm8916_lbc_charger_properties;
	desc->num_properties = ARRAY_SIZE(pm8916_lbc_charger_properties);
	desc->get_property = pm8916_lbc_charger_get_property;
	psy_cfg.drv_data = chg;
	psy_cfg.of_node = dev->of_node;

	chg->charger = devm_power_supply_register(dev, desc, &psy_cfg);
	if (IS_ERR(chg->charger)) {
		dev_err(dev, "Unable to register charger\n");
		return PTR_ERR(chg->charger);
	}

	ret = power_supply_get_battery_info(chg->charger, &chg->info);
	if (ret) {
		dev_err(dev, "Unable to get battery info: %d\n", ret);
		return ret;
	}

	chg->edev = devm_extcon_dev_allocate(dev, pm8916_lbc_charger_cable);
	if (IS_ERR(chg->edev))
		return PTR_ERR(chg->edev);

	ret = devm_extcon_dev_register(dev, chg->edev);
	if (ret < 0) {
		dev_err(dev, "failed to register extcon device: %d\n", ret);
		return ret;
	}

	ret = regmap_read(chg->regmap, chg->reg[LBC_USB] + PM8916_INT_RT_STS, &tmp);
	if (ret)
		goto comm_error;

	extcon_set_state_sync(chg->edev, EXTCON_USB, !!(tmp & PM8916_LBC_USB_USBIN_VALID));

	chg->charge_voltage_max = chg->info.voltage_max_design_uv;
	ret = pm8916_lbc_charger_enable(chg);
	if (ret)
		return ret;

	return 0;

comm_error:
	dev_err(dev, "Unable to communicate with device: %d\n", ret);
	return ret;

type_error:
	dev_err(dev, "Device reported wrong type: 0x%X\n", tmp);
	return -ENODEV;
}

static const struct of_device_id pm8916_lbc_charger_of_match[] = {
	{ .compatible = "qcom,pm8916-lbc", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm8916_lbc_charger_of_match);

static struct platform_driver pm8916_lbc_charger_driver = {
	.driver = {
		.name = "pm8916-lbc",
		.of_match_table = of_match_ptr(pm8916_lbc_charger_of_match),
	},
	.probe = pm8916_lbc_charger_probe,
};
module_platform_driver(pm8916_lbc_charger_driver);

MODULE_DESCRIPTION("pm8916 LBC driver");
MODULE_AUTHOR("Nikita Travkin <nikitos.tr@gmail.com>");
MODULE_LICENSE("GPL");
