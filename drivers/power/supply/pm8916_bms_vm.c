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

#define PM8916_PERPH_TYPE 0x04
#define PM8916_BMS_VM_TYPE 0x020D

#define PM8916_SEC_ACCESS 0xD0
#define PM8916_SEC_MAGIC 0xA5

#define PM8916_BMS_VM_STATUS1 0x08
#define PM8916_BMS_VM_FSM_STATE(x) (((x) & 0b00111000) >> 3)
#define PM8916_BMS_VM_FSM_STATE_S2 0x2

#define PM8916_BMS_VM_MODE_CTL 0x40
#define PM8916_BMS_VM_MODE_FORCE_S3 (BIT(0) | BIT(1))
#define PM8916_BMS_VM_MODE_NORMAL (BIT(1) | BIT(3))

#define PM8916_BMS_VM_EN_CTL 0x46
#define PM8916_BMS_ENABLED BIT(7)

#define PM8916_BMS_VM_FIFO_LENGTH_CTL 0x47
#define PM8916_BMS_VM_S1_SAMPLE_INTERVAL_CTL 0x55
#define PM8916_BMS_VM_S2_SAMPLE_INTERVAL_CTL 0x56
#define PM8916_BMS_VM_S3_S7_OCV_DATA0 0x6A
#define PM8916_BMS_VM_BMS_FIFO_REG_0_LSB 0xC0

// NOTE: downstream has a comment saying that using 1 fifo is broken in hardware
#define PM8916_BMS_VM_FIFO_COUNT 2 // 2 .. 8

#define PM8916_BMS_VM_S1_SAMPLE_INTERVAL 10
#define PM8916_BMS_VM_S2_SAMPLE_INTERVAL 10

struct pm8916_bms_vm_battery {
	struct device *dev;
	struct power_supply_desc desc;
	struct power_supply *battery;
	struct power_supply_battery_info info;
	struct regmap *regmap;
	unsigned int reg;
	unsigned int boot_ocv;
	unsigned int last_ocv;
	unsigned int fake_ocv;
	unsigned int vbat_now;
};

static int pm8916_bms_vm_battery_get_property(struct power_supply *psy,
					      enum power_supply_property psp,
					      union power_supply_propval *val)
{
	struct pm8916_bms_vm_battery *bat = power_supply_get_drvdata(psy);
	struct power_supply_battery_info *info = &bat->info;
	int supplied;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		supplied = power_supply_am_i_supplied(psy);

		if (supplied < 0 && supplied != -ENODEV) {
			return supplied;
		} else if (supplied && supplied != -ENODEV) {
			if (power_supply_batinfo_ocv2cap(info, bat->fake_ocv, 20) > 98)
				val->intval = POWER_SUPPLY_STATUS_FULL;
			else
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
		} else {
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		return 0;

	case POWER_SUPPLY_PROP_HEALTH:
		if (bat->vbat_now < info->voltage_min_design_uv)
			val->intval = POWER_SUPPLY_HEALTH_DEAD;
		else if (bat->vbat_now > info->voltage_max_design_uv)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		return 0;

	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = power_supply_batinfo_ocv2cap(info, bat->fake_ocv, 20);
		return 0;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = bat->vbat_now;
		return 0;

	case POWER_SUPPLY_PROP_VOLTAGE_BOOT:
		// NOTE Returning last known ocv value here - it changes after suspend
		val->intval = bat->last_ocv;
		return 0;

	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = bat->fake_ocv;
		return 0;

	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = info->voltage_min_design_uv;
		return 0;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = info->voltage_max_design_uv;
		return 0;

	default:
		return -EINVAL;
	}
}

static enum power_supply_property pm8916_bms_vm_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_BOOT,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CAPACITY,
};

static irqreturn_t pm8916_bms_vm_fifo_update_done_irq(int irq, void *data)
{
	struct pm8916_bms_vm_battery *bat = data;
	struct power_supply_battery_info *info = &bat->info;
	u16 vbat_data[PM8916_BMS_VM_FIFO_COUNT];
	int i, ret, delta = 0, loc_delta;
	unsigned int tmp = 0;
	int supplied;

	ret = regmap_bulk_read(bat->regmap, bat->reg + PM8916_BMS_VM_BMS_FIFO_REG_0_LSB,
			       &vbat_data, PM8916_BMS_VM_FIFO_COUNT * 2);
	if (ret)
		return IRQ_HANDLED;

	supplied = power_supply_am_i_supplied(bat->battery);
	// We assume that we don't charge if no charger is present.
	if (supplied == -ENODEV)
		supplied = 0;
	else if (supplied < 0)
		return IRQ_HANDLED;

	for (i = 0; i < PM8916_BMS_VM_FIFO_COUNT; i++) {
		tmp = vbat_data[i] * 300 - 100000;

		loc_delta = tmp - bat->vbat_now;
		delta += loc_delta;
		bat->vbat_now = tmp;
	}

	/*
	 * NOTE: Since VM-BMS is mostly implemented in software, OCV needs to be estimated.
	 * This driver makes some assumptions to estimate OCV from averaged VBAT measurements
	 * and initial OCV measurements taken on boot or while in suspend:
	 *
	 *  - When charger is online, ocv can only increase.
	 *  - When charger is offline, ocv can only decrease and ocv > vbat.
	 *  - ocv can't change more than 0.025v between the measurements.
	 *  - When charger is in CV mode (vbat = const vbat-max), ocv increases by
	 *    0.004v every measurement until it reaches vbat.
	 *
	 * Those assumptions give somewhat realistic estimation of ocv and capacity, though
	 * in some worst case scenarios it will perform poorly.
	 * Ideally proper BMS algorithm should be implemented in userspace.
	 */

	if ((supplied && delta > 0) || (!supplied && delta < 0))
		if (abs(delta) < 25000) /* 0.025v */
			bat->fake_ocv += delta;

	if (!supplied && bat->fake_ocv < bat->vbat_now)
		bat->fake_ocv = bat->vbat_now;

	regmap_write(bat->regmap, bat->reg + PM8916_BMS_VM_STATUS1, 0);
	regmap_read(bat->regmap, bat->reg + PM8916_BMS_VM_STATUS1, &tmp);

	if (PM8916_BMS_VM_FSM_STATE(tmp) == PM8916_BMS_VM_FSM_STATE_S2 &&
	    bat->fake_ocv < bat->vbat_now - 10000 /* 0.01v */) {
		bat->fake_ocv += 4000; /* 0.004v */
	}

	if (supplied && bat->fake_ocv > info->voltage_max_design_uv)
		bat->fake_ocv = info->voltage_max_design_uv;

	power_supply_changed(bat->battery);

	return IRQ_HANDLED;
}

static int pm8916_bms_vm_battery_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pm8916_bms_vm_battery *bat;
	struct power_supply_config psy_cfg = {};
	struct power_supply_desc *desc;
	int ret, irq;
	unsigned int tmp;

	bat = devm_kzalloc(dev, sizeof(*bat), GFP_KERNEL);
	if (!bat)
		return -ENOMEM;

	bat->dev = dev;

	bat->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!bat->regmap)
		return -ENODEV;

	of_property_read_u32(dev->of_node, "reg", &bat->reg);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_threaded_irq(dev, irq, NULL, pm8916_bms_vm_fifo_update_done_irq,
					IRQF_ONESHOT, "pm8916_vm_bms", bat);
	if (ret)
		return ret;

	ret = regmap_bulk_read(bat->regmap, bat->reg + PM8916_PERPH_TYPE, &tmp, 2);
	if (ret)
		goto comm_error;

	if (tmp != PM8916_BMS_VM_TYPE) {
		dev_err(dev, "Device reported wrong type: 0x%X\n", tmp);
		return -ENODEV;
	}

	ret = regmap_write(bat->regmap, bat->reg + PM8916_BMS_VM_S1_SAMPLE_INTERVAL_CTL,
			   PM8916_BMS_VM_S1_SAMPLE_INTERVAL);
	if (ret)
		goto comm_error;
	ret = regmap_write(bat->regmap, bat->reg + PM8916_BMS_VM_S2_SAMPLE_INTERVAL_CTL,
			   PM8916_BMS_VM_S2_SAMPLE_INTERVAL);
	if (ret)
		goto comm_error;
	ret = regmap_write(bat->regmap, bat->reg + PM8916_BMS_VM_FIFO_LENGTH_CTL,
			   PM8916_BMS_VM_FIFO_COUNT << 4 | PM8916_BMS_VM_FIFO_COUNT);
	if (ret)
		goto comm_error;
	ret = regmap_write(bat->regmap,
			   bat->reg + PM8916_BMS_VM_EN_CTL, PM8916_BMS_ENABLED);
	if (ret)
		goto comm_error;

	ret = regmap_bulk_read(bat->regmap,
			       bat->reg + PM8916_BMS_VM_S3_S7_OCV_DATA0, &tmp, 2);
	if (ret)
		goto comm_error;

	bat->boot_ocv = tmp * 300;
	bat->last_ocv = bat->boot_ocv;
	bat->fake_ocv = bat->boot_ocv;
	bat->vbat_now = bat->boot_ocv;

	desc = &bat->desc;
	desc->name = "pm8916-bms-vm";
	desc->type = POWER_SUPPLY_TYPE_BATTERY;
	desc->properties = pm8916_bms_vm_battery_properties;
	desc->num_properties = ARRAY_SIZE(pm8916_bms_vm_battery_properties);
	desc->get_property = pm8916_bms_vm_battery_get_property;
	psy_cfg.drv_data = bat;
	psy_cfg.of_node = dev->of_node;

	bat->battery = devm_power_supply_register(dev, desc, &psy_cfg);
	if (IS_ERR(bat->battery)) {
		if (PTR_ERR(bat->battery) != -EPROBE_DEFER)
			dev_err(dev, "Unable to register battery: %ld\n", PTR_ERR(bat->battery));
		return PTR_ERR(bat->battery);
	}

	ret = power_supply_get_battery_info(bat->battery, &bat->info);
	if (ret) {
		dev_err(dev, "Unable to get battery info: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, bat);

	return 0;

comm_error:
	dev_err(dev, "Unable to communicate with device: %d\n", ret);
	return ret;
}

static int pm8916_bms_vm_battery_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct pm8916_bms_vm_battery *bat = platform_get_drvdata(pdev);
	int ret;

	ret = regmap_write(bat->regmap,
			   bat->reg + PM8916_SEC_ACCESS, PM8916_SEC_MAGIC);
	if (ret)
		goto error;
	ret = regmap_write(bat->regmap,
			   bat->reg + PM8916_BMS_VM_MODE_CTL, PM8916_BMS_VM_MODE_FORCE_S3);
	if (ret)
		goto error;

	return 0;

error:
	dev_err(bat->dev, "Failed to force S3 mode: %d\n", ret);
	return ret;
}

static int pm8916_bms_vm_battery_resume(struct platform_device *pdev)
{
	struct pm8916_bms_vm_battery *bat = platform_get_drvdata(pdev);
	int ret;
	unsigned int tmp;

	ret = regmap_bulk_read(bat->regmap,
			       bat->reg + PM8916_BMS_VM_S3_S7_OCV_DATA0, &tmp, 2);

	if (tmp * 300 != bat->last_ocv) {
		bat->last_ocv = tmp * 300;
		bat->fake_ocv = bat->last_ocv;
	}

	ret = regmap_write(bat->regmap,
			   bat->reg + PM8916_SEC_ACCESS, PM8916_SEC_MAGIC);
	if (ret)
		goto error;
	ret = regmap_write(bat->regmap,
			   bat->reg + PM8916_BMS_VM_MODE_CTL, PM8916_BMS_VM_MODE_NORMAL);
	if (ret)
		goto error;

	return 0;

error:
	dev_err(bat->dev, "Failed to return normal mode: %d\n", ret);
	return ret;
}

static const struct of_device_id pm8916_bms_vm_battery_of_match[] = {
	{ .compatible = "qcom,pm8916-bms-vm", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm8916_bms_vm_battery_of_match);

static struct platform_driver pm8916_bms_vm_battery_driver = {
	.driver = {
		.name = "pm8916-bms-vm",
		.of_match_table = of_match_ptr(pm8916_bms_vm_battery_of_match),
	},
	.probe = pm8916_bms_vm_battery_probe,
	.suspend = pm8916_bms_vm_battery_suspend,
	.resume = pm8916_bms_vm_battery_resume,
};
module_platform_driver(pm8916_bms_vm_battery_driver);

MODULE_DESCRIPTION("pm8916 BMS-VM driver");
MODULE_AUTHOR("Nikita Travkin <nikitos.tr@gmail.com>");
MODULE_LICENSE("GPL");
