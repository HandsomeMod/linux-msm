// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for MStar msg2638 touchscreens
 *
 * Copyright (c) 2021 Vincent Knecht <vincent.knecht@mailoo.org>
 *
 * Checksum and IRQ handler based on mstar_drv_common.c and mstar_drv_mutual_fw_control.c
 * Copyright (c) 2006-2012 MStar Semiconductor, Inc.
 *
 * Driver structure based on zinitix.c by Michael Srba <Michael.Srba@seznam.cz>
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#define MODE_DATA_RAW			0x5A

#define MAX_SUPPORTED_FINGER_NUM	5

#define CHIP_ON_DELAY_MS		15
#define FIRMWARE_ON_DELAY_MS		50
#define RESET_DELAY_MIN_US		10000
#define RESET_DELAY_MAX_US		11000

struct point_coord {
	u16	x;
	u16	y;
};

struct packet {
	u8	xy_hi; /* higher bits of x and y coordinates */
	u8	x_low;
	u8	y_low;
	u8	pressure;
};

struct touch_event {
	u8	mode;
	struct	packet pkt[MAX_SUPPORTED_FINGER_NUM];
	u8	proximity;
	u8	checksum;
};

struct msg2638_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct touchscreen_properties prop;
	struct regulator_bulk_data supplies[2];
	struct gpio_desc *reset_gpiod;
};

static int msg2638_init_regulators(struct msg2638_ts_data *msg2638)
{
	struct i2c_client *client = msg2638->client;
	int ret;

	msg2638->supplies[0].supply = "vdd";
	msg2638->supplies[1].supply = "vddio";
	ret = devm_regulator_bulk_get(&client->dev,
				      ARRAY_SIZE(msg2638->supplies),
				      msg2638->supplies);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to get regulators: %d\n", ret);
		return ret;
	}

	return 0;
}

static void msg2638_power_on(struct msg2638_ts_data *msg2638)
{
	gpiod_set_value_cansleep(msg2638->reset_gpiod, 1);
	usleep_range(RESET_DELAY_MIN_US, RESET_DELAY_MAX_US);
	gpiod_set_value_cansleep(msg2638->reset_gpiod, 0);
	msleep(FIRMWARE_ON_DELAY_MS);
}

static void msg2638_report_finger(struct msg2638_ts_data *msg2638, int slot,
				const struct point_coord *pc)
{
	input_mt_slot(msg2638->input_dev, slot);
	input_mt_report_slot_state(msg2638->input_dev, MT_TOOL_FINGER, true);
	touchscreen_report_pos(msg2638->input_dev, &msg2638->prop, pc->x, pc->y, true);
	input_report_abs(msg2638->input_dev, ABS_MT_TOUCH_MAJOR, 1);
}

static u8 msg2638_checksum(u8 *data, u32 length)
{
	s32 sum = 0;
	u32 i;

	for (i = 0; i < length; i++)
		sum += data[i];

	return (u8)((-sum) & 0xFF);
}

static irqreturn_t msg2638_ts_irq_handler(int irq, void *msg2638_handler)
{
	struct msg2638_ts_data *msg2638 = msg2638_handler;
	struct i2c_client *client = msg2638->client;
	struct touch_event touch_event;
	struct point_coord coord;
	struct i2c_msg msg[1];
	struct packet *p;
	u32 len;
	int ret;
	int i;

	len = sizeof(struct touch_event);
	memset(&touch_event, 0, len);

	msg[0].addr = client->addr;
	msg[0].flags = I2C_M_RD;
	msg[0].len = len;
	msg[0].buf = (u8 *)&touch_event;

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret != 1) {
		dev_err(&client->dev, "Failed I2C transfer in irq handler!\n");
		goto out;
	}

	if (touch_event.mode != MODE_DATA_RAW)
		goto out;

	if (msg2638_checksum((u8 *)&touch_event, len - 1) != touch_event.checksum) {
		dev_err(&client->dev, "Failed checksum!\n");
		goto out;
	}

	for (i = 0; i < MAX_SUPPORTED_FINGER_NUM; i++) {
		p = &touch_event.pkt[i];
		/* Ignore non-pressed finger data */
		if (p->xy_hi == 0xFF && p->x_low == 0xFF && p->y_low == 0xFF)
			continue;

		coord.x = (((p->xy_hi & 0xF0) << 4) | p->x_low);
		coord.y = (((p->xy_hi & 0x0F) << 8) | p->y_low);
		msg2638_report_finger(msg2638, i, &coord);
	}

	input_mt_sync_frame(msg2638->input_dev);
	input_sync(msg2638->input_dev);

out:
	return IRQ_HANDLED;
}

static int msg2638_start(struct msg2638_ts_data *msg2638)
{
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(msg2638->supplies), msg2638->supplies);
	if (ret) {
		dev_err(&msg2638->client->dev, "Failed to enable regulators: %d\n", ret);
		return ret;
	}

	msleep(CHIP_ON_DELAY_MS);
	msg2638_power_on(msg2638);
	enable_irq(msg2638->client->irq);

	return 0;
}

static int msg2638_stop(struct msg2638_ts_data *msg2638)
{
	int ret;

	disable_irq(msg2638->client->irq);

	ret = regulator_bulk_disable(ARRAY_SIZE(msg2638->supplies), msg2638->supplies);
	if (ret) {
		dev_err(&msg2638->client->dev, "Failed to disable regulators: %d\n", ret);
		return ret;
	}

	return 0;
}

static int msg2638_input_open(struct input_dev *dev)
{
	struct msg2638_ts_data *msg2638 = input_get_drvdata(dev);

	return msg2638_start(msg2638);
}

static void msg2638_input_close(struct input_dev *dev)
{
	struct msg2638_ts_data *msg2638 = input_get_drvdata(dev);

	msg2638_stop(msg2638);
}

static int msg2638_init_input_dev(struct msg2638_ts_data *msg2638)
{
	struct input_dev *input_dev;
	int ret;

	input_dev = devm_input_allocate_device(&msg2638->client->dev);
	if (!input_dev) {
		dev_err(&msg2638->client->dev, "Failed to allocate input device.\n");
		return -ENOMEM;
	}

	input_set_drvdata(input_dev, msg2638);
	msg2638->input_dev = input_dev;

	input_dev->name = "MStar TouchScreen";
	input_dev->phys = "input/ts";
	input_dev->id.bustype = BUS_I2C;
	input_dev->open = msg2638_input_open;
	input_dev->close = msg2638_input_close;

	input_set_capability(input_dev, EV_ABS, ABS_MT_POSITION_X);
	input_set_capability(input_dev, EV_ABS, ABS_MT_POSITION_Y);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);

	touchscreen_parse_properties(input_dev, true, &msg2638->prop);
	if (!msg2638->prop.max_x || !msg2638->prop.max_y) {
		dev_err(&msg2638->client->dev,
			"touchscreen-size-x and/or touchscreen-size-y not set in dts\n");
		return -EINVAL;
	}

	ret = input_mt_init_slots(input_dev, MAX_SUPPORTED_FINGER_NUM,
				  INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
	if (ret) {
		dev_err(&msg2638->client->dev, "Failed to initialize MT slots: %d\n", ret);
		return ret;
	}

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&msg2638->client->dev, "Failed to register input device: %d\n", ret);
		return ret;
	}

	return 0;
}

static int msg2638_ts_probe(struct i2c_client *client)
{
	struct msg2638_ts_data *msg2638;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "Failed to assert adapter's support for plain I2C.\n");
		return -ENXIO;
	}

	msg2638 = devm_kzalloc(&client->dev, sizeof(*msg2638), GFP_KERNEL);
	if (!msg2638)
		return -ENOMEM;

	msg2638->client = client;
	i2c_set_clientdata(client, msg2638);

	ret = msg2638_init_regulators(msg2638);
	if (ret) {
		dev_err(&client->dev, "Failed to initialize regulators: %d\n", ret);
		return ret;
	}

	msg2638->reset_gpiod = devm_gpiod_get(&client->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(msg2638->reset_gpiod)) {
		ret = PTR_ERR(msg2638->reset_gpiod);
		dev_err(&client->dev, "Failed to request reset GPIO: %d\n", ret);
		return ret;
	}

	ret = msg2638_init_input_dev(msg2638);
	if (ret) {
		dev_err(&client->dev, "Failed to initialize input device: %d\n", ret);
		return ret;
	}

	irq_set_status_flags(client->irq, IRQ_NOAUTOEN);
	ret = devm_request_threaded_irq(&client->dev, client->irq,
					NULL, msg2638_ts_irq_handler,
					IRQF_ONESHOT, client->name, msg2638);
	if (ret) {
		dev_err(&client->dev, "Failed to request IRQ: %d\n", ret);
		return ret;
	}

	return 0;
}

static int __maybe_unused msg2638_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct msg2638_ts_data *msg2638 = i2c_get_clientdata(client);

	mutex_lock(&msg2638->input_dev->mutex);

	if (input_device_enabled(msg2638->input_dev))
		msg2638_stop(msg2638);

	mutex_unlock(&msg2638->input_dev->mutex);

	return 0;
}

static int __maybe_unused msg2638_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct msg2638_ts_data *msg2638 = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&msg2638->input_dev->mutex);

	if (input_device_enabled(msg2638->input_dev))
		ret = msg2638_start(msg2638);

	mutex_unlock(&msg2638->input_dev->mutex);

	return ret;
}

static SIMPLE_DEV_PM_OPS(msg2638_pm_ops, msg2638_suspend, msg2638_resume);

static const struct of_device_id msg2638_of_match[] = {
	{ .compatible = "mstar,msg2638" },
	{ }
};
MODULE_DEVICE_TABLE(of, msg2638_of_match);

static struct i2c_driver msg2638_ts_driver = {
	.probe_new = msg2638_ts_probe,
	.driver = {
		.name = "MStar-TS",
		.pm = &msg2638_pm_ops,
		.of_match_table = of_match_ptr(msg2638_of_match),
	},
};
module_i2c_driver(msg2638_ts_driver);

MODULE_AUTHOR("Vincent Knecht <vincent.knecht@mailoo.org>");
MODULE_DESCRIPTION("MStar MSG2638 touchscreen driver");
MODULE_LICENSE("GPL v2");
