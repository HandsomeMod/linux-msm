// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for HMX852xES chipset
 *
 * Mainly inspired by Melfas MMS114/MMS152 touchscreen device driver
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * Based on the Himax Android Driver Sample Code Ver 0.3 for HMX852xES chipset:
 * Copyright (c) 2014 Himax Corporation.
 */

#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>

#define HX852_COORD_SIZE(fingers)	((fingers) * sizeof(struct hx852_coord))
#define HX852_WIDTH_SIZE(fingers)	ALIGN(fingers, 4)
#define HX852_BUF_SIZE(fingers)		(HX852_COORD_SIZE(fingers) + \
					HX852_WIDTH_SIZE(fingers) + \
					sizeof(struct hx852_touch_info))

#define HX852_MAX_FINGERS		12
#define HX852_MAX_KEY_COUNT		3
#define HX852_MAX_BUF_SIZE		HX852_BUF_SIZE(HX852_MAX_FINGERS)

#define HX852_SLEEP_MODE_ON		0x80
#define HX852_SLEEP_MODE_OFF		0x81
#define HX852_TOUCH_EVENTS_OFF		0x82
#define HX852_TOUCH_EVENTS_ON		0x83
#define HX852_READ_ALL_EVENTS		0x86

#define HX852_REG_SRAM_SWITCH		0x8C
#define HX852_REG_SRAM_ADDR		0x8B
#define HX852_REG_FLASH_RPLACE		0x5A

#define HX852_ENTER_TEST_MODE_SEQ	{HX852_REG_SRAM_SWITCH, 0x14}
#define HX852_LEAVE_TEST_MODE_SEQ	{HX852_REG_SRAM_SWITCH, 0x00}
#define HX852_GET_CONFIG_SEQ		{HX852_REG_SRAM_ADDR, 0x00, 0x70}

#define ABS_PRESSURE_MIN		0
#define ABS_PRESSURE_MAX		200
#define ABS_WIDTH_MIN			0
#define ABS_WIDTH_MAX			200

struct hx852_data {
	struct touchscreen_properties props;
	struct i2c_client *client;
	struct input_dev *input_dev;

	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[2];

	int max_fingers;

	bool had_finger_pressed;
	u8 last_key;

	int keycount;
	u32 keycodes[HX852_MAX_KEY_COUNT];
};

struct hx852_config {
	u8 rx_num;
	u8 tx_num;
	u8 max_pt;
	u8 padding1[3];
	u8 x_res[2];
	u8 y_res[2];
	u8 padding2[2];
};

struct hx852_command {
	u8 data[3];
	u8 len;
	u8 sleep;
};

struct hx852_coord {
	u8 x[2];
	u8 y[2];
};

struct hx852_touch_info {
	u8 finger_num;
	u8 finger_pressed[2];
	u8 padding;
};

static const u8 hx852_internal_keymappings[HX852_MAX_KEY_COUNT] = {0x01, 0x02, 0x04};

static int hx852_i2c_read(struct hx852_data *ts, uint8_t command, uint8_t *data,
			  uint8_t length)
{
	struct i2c_client *client = ts->client;
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &command,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = data,
		}
	};

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret != ARRAY_SIZE(msg)) {
		dev_err(&client->dev, "failed to read command %#x: %d\n",
			command, ret);
		return ret;
	}

	return 0;
}

static int hx852_i2c_write(struct hx852_data *ts, struct hx852_command *cmds,
			   unsigned int ncmds)
{
	struct i2c_client *client = ts->client;
	int ret;
	int i;

	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
	};

	for (i = 0; i < ncmds; i++) {
		msg.len = cmds[i].len;
		msg.buf = cmds[i].data;

		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret != 1) {
			dev_err(&client->dev,
				"failed to write command %d (%#x): %d\n", i,
				cmds[i].data[0], ret);
			return ret;
		}

		msleep(cmds[i].sleep);
	}

	return 0;
}

static int hx852_read_config(struct hx852_data *ts)
{
	struct hx852_config conf;
	int x_max, y_max;
	int error;

	struct hx852_command pre_read_msg_seq[] = {
		{ .data = HX852_ENTER_TEST_MODE_SEQ, .len = 2, .sleep = 10 },
		{ .data = HX852_GET_CONFIG_SEQ, .len = 3, .sleep = 10 },
	};

	struct hx852_command post_read_msg_seq[] = {
		{ .data = HX852_LEAVE_TEST_MODE_SEQ, .len = 2, .sleep = 10 },
	};

	error = hx852_i2c_write(ts, pre_read_msg_seq,
				ARRAY_SIZE(pre_read_msg_seq));
	if (error)
		return error;

	error = hx852_i2c_read(ts, HX852_REG_FLASH_RPLACE,
			       (u8 *)&conf, sizeof(conf));
	if (error)
		return error;

	error = hx852_i2c_write(ts, post_read_msg_seq,
				ARRAY_SIZE(post_read_msg_seq));
	if (error)
		return error;

	ts->max_fingers = (conf.max_pt & 0xF0) >> 4;
	if (ts->max_fingers > HX852_MAX_FINGERS) {
		dev_err(&ts->client->dev,
			"max supported fingers: %d, yours: %d\n",
			HX852_MAX_FINGERS, ts->max_fingers);
		return -EINVAL;
	}

	x_max = get_unaligned_be16(conf.x_res) - 1;
	y_max = get_unaligned_be16(conf.y_res) - 1;

	if (x_max && y_max) {
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
				     0, x_max, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
				     0, y_max, 0, 0);
	}

	return 0;
}

static int hx852_power_on(struct hx852_data *ts)
{
	struct i2c_client *client = ts->client;
	int error;

	struct hx852_command ts_resume_cmd_seq[] = {
		{ .data = { HX852_TOUCH_EVENTS_ON }, .len = 1, .sleep = 30 },
		{ .data = { HX852_SLEEP_MODE_OFF }, .len = 1, .sleep = 50 },
	};

	error = regulator_bulk_enable(ARRAY_SIZE(ts->supplies), ts->supplies);
	if (error < 0) {
		dev_err(&client->dev,
			"Failed to enable regulators: %d\n", error);
		return error;
	}

	gpiod_set_value_cansleep(ts->reset_gpio, 0);
	msleep(20);
	gpiod_set_value_cansleep(ts->reset_gpio, 1);

	msleep(50);
	error = hx852_i2c_write(ts, ts_resume_cmd_seq,
				ARRAY_SIZE(ts_resume_cmd_seq));
	if (error)
		return error;

	return 0;
}

static int hx852_init(struct hx852_data *ts)
{
	int error;

	struct hx852_command pre_conf_cmd_seq[] = {
		{ .data = { HX852_TOUCH_EVENTS_OFF }, .len = 1, .sleep = 50 },
		{ .data = { HX852_SLEEP_MODE_ON }, .len = 1, .sleep = 0 },
	};

	error = hx852_power_on(ts);
	if (error)
		return error;

	error = hx852_i2c_write(ts, pre_conf_cmd_seq,
				ARRAY_SIZE(pre_conf_cmd_seq));
	if (error)
		return error;

	error = hx852_read_config(ts);
	if (error)
		return error;

	error = regulator_bulk_disable(ARRAY_SIZE(ts->supplies), ts->supplies);
	if (error)
		dev_warn(&ts->client->dev,
			 "failed to disable regulators: %d\n", error);

	return 0;
}

static void hx852_process_btn_touch(struct hx852_data *ts, int current_key)
{
	int i;

	for (i = 0; i < ts->keycount; i++) {
		if (hx852_internal_keymappings[i] == current_key)
			input_report_key(ts->input_dev, ts->keycodes[i], 1);
		else if (hx852_internal_keymappings[i] == ts->last_key)
			input_report_key(ts->input_dev, ts->keycodes[i], 0);
	}

	ts->last_key = current_key;
}

static void hx852_process_display_touch(struct hx852_data *ts,
					struct hx852_coord *coord,
					u8 *width, u16 finger_pressed)
{
	unsigned int x, y, w;
	int i;

	ts->had_finger_pressed = false;

	for (i = 0; i < ts->max_fingers; i++) {
		if (!(finger_pressed & BIT(i)))
			continue;

		x = get_unaligned_be16(coord[i].x);
		y = get_unaligned_be16(coord[i].y);
		w = width[i];

		input_mt_slot(ts->input_dev, i);
		input_mt_report_slot_state(ts->input_dev,
					   MT_TOOL_FINGER, 1);

		touchscreen_report_pos(ts->input_dev, &ts->props, x, y,
				       true);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
		ts->had_finger_pressed = true;
	}

	input_mt_sync_frame(ts->input_dev);
}

static irqreturn_t hx852_interrupt(int irq, void *ptr)
{
	struct hx852_data *ts = ptr;
	struct input_dev *input_dev = ts->input_dev;

	u8 buf[HX852_MAX_BUF_SIZE];
	u8 current_key;
	u16 finger_pressed;
	int error;

	struct hx852_coord *coord = (struct hx852_coord *)buf;
	u8 *width = &buf[HX852_COORD_SIZE(ts->max_fingers)];
	struct hx852_touch_info *info = (struct hx852_touch_info *)
		&width[HX852_WIDTH_SIZE(ts->max_fingers)];

	mutex_lock(&input_dev->mutex);
	if (!input_dev->users) {
		mutex_unlock(&input_dev->mutex);
		return IRQ_HANDLED;
	}
	mutex_unlock(&input_dev->mutex);

	error = hx852_i2c_read(ts, HX852_READ_ALL_EVENTS, buf,
			       HX852_BUF_SIZE(ts->max_fingers));
	if (error)
		return IRQ_HANDLED;

	finger_pressed = get_unaligned_le16(info->finger_pressed);

	current_key = finger_pressed >> HX852_MAX_FINGERS;
	if (current_key == 0x0F)
		current_key = 0x00;

	if (info->finger_num == 0xff || !(info->finger_num & 0x0f))
		finger_pressed = 0;

	if (finger_pressed || ts->had_finger_pressed)
		hx852_process_display_touch(ts, coord, width, finger_pressed);
	else if (ts->keycount && (current_key || ts->last_key))
		hx852_process_btn_touch(ts, current_key);

	input_sync(ts->input_dev);

	return IRQ_HANDLED;
}

static int hx852_start(struct hx852_data *ts)
{
	struct i2c_client *client = ts->client;
	int error;

	error = hx852_power_on(ts);
	if (error)
		return error;

	enable_irq(client->irq);

	return 0;
}

static void hx852_stop(struct hx852_data *ts)
{
	struct i2c_client *client = ts->client;
	int error;

	struct hx852_command ts_sleep_cmd_seq[] = {
		{ .data = { HX852_TOUCH_EVENTS_OFF }, .len = 1, .sleep = 40 },
		{ .data = { HX852_SLEEP_MODE_ON }, .len = 1, .sleep = 50 },
	};

	disable_irq(client->irq);

	hx852_i2c_write(ts, ts_sleep_cmd_seq, ARRAY_SIZE(ts_sleep_cmd_seq));

	error = regulator_bulk_disable(ARRAY_SIZE(ts->supplies), ts->supplies);
	if (error)
		dev_warn(&client->dev,
			 "failed to disable regulators: %d\n", error);
}

static int hx852_input_open(struct input_dev *dev)
{
	struct hx852_data *ts = input_get_drvdata(dev);

	return hx852_start(ts);
}

static void hx852_input_close(struct input_dev *dev)
{
	struct hx852_data *ts = input_get_drvdata(dev);

	hx852_stop(ts);
}

static int hx852_parse_properties(struct hx852_data *ts)
{
	struct device *dev = &ts->client->dev;
	int error = 0;

	ts->keycount = device_property_count_u32(dev, "linux,keycodes");

	if (ts->keycount <= 0) {
		ts->keycount = 0;
		return 0;
	}

	if (ts->keycount > HX852_MAX_KEY_COUNT) {
		dev_err(dev, "max supported keys: %d, yours: %d\n",
			HX852_MAX_KEY_COUNT, ts->keycount);
		return -EINVAL;
	}

	error = device_property_read_u32_array(dev, "linux,keycodes",
					       ts->keycodes, ts->keycount);
	if (error)
		dev_err(dev,
			"failed to read linux,keycodes property: %d\n", error);

	return error;
}

static int hx852_probe(struct i2c_client *client)
{
	struct hx852_data *ts;
	struct input_dev *input_dev;
	int i;
	int error;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev,
			"plain i2c-level commands not supported\n");
		return -ENODEV;
	}

	ts = devm_kzalloc(&client->dev, sizeof(struct hx852_data), GFP_KERNEL);
	input_dev = devm_input_allocate_device(&client->dev);
	if (!ts || !input_dev) {
		dev_err(&client->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	ts->client = client;
	ts->input_dev = input_dev;

	ts->supplies[0].supply = "vcca";
	ts->supplies[1].supply = "vccd";
	error = devm_regulator_bulk_get(&client->dev,
					ARRAY_SIZE(ts->supplies), ts->supplies);
	if (error < 0) {
		dev_err(&client->dev, "Failed to get regulators: %d\n", error);
		return error;
	}

	ts->reset_gpio = devm_gpiod_get_optional(&client->dev, "reset",
						 GPIOD_OUT_LOW);
	if (IS_ERR(ts->reset_gpio)) {
		error = PTR_ERR(ts->reset_gpio);
		dev_err(&client->dev, "failed to get reset gpio: %d\n", error);
		return error;
	}

	input_set_capability(input_dev, EV_ABS, ABS_MT_POSITION_X);
	input_set_capability(input_dev, EV_ABS, ABS_MT_POSITION_Y);

	error = hx852_init(ts);
	if (error)
		return error;

	touchscreen_parse_properties(input_dev, true, &ts->props);
	error = hx852_parse_properties(ts);
	if (error)
		return error;

	error = input_mt_init_slots(input_dev, ts->max_fingers,
				    INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
	if (error)
		return error;

	for (i = 0; i < ts->keycount; i++)
		input_set_capability(ts->input_dev, EV_KEY, ts->keycodes[i]);

	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);

	input_dev->name = "himax-touchscreen";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->open = hx852_input_open;
	input_dev->close = hx852_input_close;

	input_set_drvdata(input_dev, ts);
	i2c_set_clientdata(client, ts);

	if (!client->irq) {
		dev_err(&client->dev, "client->irq not found\n");
		return -EINVAL;
	}

	error = devm_request_threaded_irq(&client->dev, client->irq, NULL,
					  hx852_interrupt, IRQF_ONESHOT,
					  NULL, ts);
	if (error) {
		dev_err(&client->dev, "request irq %d failed\n",
			client->irq);
		return error;
	}

	disable_irq(client->irq);

	error = input_register_device(ts->input_dev);
	if (error) {
		dev_err(&client->dev, "failed to register input device\n");
		return error;
	}

	return 0;
}

static int __maybe_unused hx852_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hx852_data *ts = i2c_get_clientdata(client);
	struct input_dev *input_dev = ts->input_dev;
	int i;

	mutex_lock(&input_dev->mutex);
	if (input_dev->users)
		hx852_stop(ts);
	mutex_unlock(&input_dev->mutex);

	if (ts->had_finger_pressed)
		input_mt_sync_frame(input_dev);

	if (ts->last_key) {
		for (i = 0; i < ts->keycount; i++) {
			if (hx852_internal_keymappings[i] == ts->last_key)
				input_report_key(ts->input_dev,
						 ts->keycodes[i], 0);
		}
	}

	if (ts->had_finger_pressed || ts->last_key)
		input_sync(input_dev);

	ts->last_key = 0;
	ts->had_finger_pressed = false;

	return 0;
}

static int __maybe_unused hx852_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hx852_data *ts = i2c_get_clientdata(client);
	struct input_dev *input_dev = ts->input_dev;
	int error = 0;

	mutex_lock(&input_dev->mutex);
	if (input_dev->users)
		error = hx852_start(ts);
	mutex_unlock(&input_dev->mutex);

	return error;
}

static SIMPLE_DEV_PM_OPS(hx852_pm_ops, hx852_suspend, hx852_resume);

#ifdef CONFIG_OF
static const struct of_device_id hx852_dt_match[] = {
	{ .compatible = "himax,852x" },
	{ }
};
MODULE_DEVICE_TABLE(of, hx852_dt_match);
#endif

static struct i2c_driver hx852_driver = {
	.driver = {
		.name = "Himax852xes",
		.pm = &hx852_pm_ops,
		.of_match_table = of_match_ptr(hx852_dt_match),
	},
	.probe_new = hx852_probe,
};

module_i2c_driver(hx852_driver);

MODULE_DESCRIPTION("Driver for HMX852xES chipset");
MODULE_LICENSE("GPL");
