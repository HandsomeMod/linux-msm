// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2021 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct tianma_nt35521_5p5 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator_bulk_data supplies[2];
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline
struct tianma_nt35521_5p5 *to_tianma_nt35521_5p5(struct drm_panel *panel)
{
	return container_of(panel, struct tianma_nt35521_5p5, panel);
}

#define dsi_generic_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_generic_write(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

#define dsi_dcs_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static void tianma_nt35521_5p5_reset(struct tianma_nt35521_5p5 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(20);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(120);
}

static int tianma_nt35521_5p5_on(struct tianma_nt35521_5p5 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	dsi_generic_write_seq(dsi, 0xff, 0xaa, 0x55, 0xa5, 0x80);
	dsi_generic_write_seq(dsi, 0x6f, 0x11, 0x00);
	dsi_generic_write_seq(dsi, 0xf7, 0x20, 0x00);
	dsi_generic_write_seq(dsi, 0x6f, 0x11);
	dsi_generic_write_seq(dsi, 0xf3, 0x01);
	dsi_generic_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x00);
	dsi_generic_write_seq(dsi, 0xb1, 0x60);
	dsi_generic_write_seq(dsi, 0xbd, 0x01, 0xa0, 0x0c, 0x08, 0x01);
	dsi_generic_write_seq(dsi, 0x6f, 0x02);
	dsi_generic_write_seq(dsi, 0xb8, 0x01);
	dsi_generic_write_seq(dsi, 0xbb, 0x11, 0x11);
	dsi_generic_write_seq(dsi, 0xbc, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xb6, 0x06);
	dsi_generic_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x01);
	dsi_generic_write_seq(dsi, 0xb0, 0x09, 0x09);
	dsi_generic_write_seq(dsi, 0xb1, 0x09, 0x09);
	dsi_generic_write_seq(dsi, 0xb3, 0x28, 0x28);
	dsi_generic_write_seq(dsi, 0xb4, 0x0f, 0x0f);
	dsi_generic_write_seq(dsi, 0xb5, 0x03, 0x03);
	dsi_generic_write_seq(dsi, 0xb9, 0x34, 0x34);
	dsi_generic_write_seq(dsi, 0xba, 0x15, 0x15);
	dsi_generic_write_seq(dsi, 0xbc, 0x58, 0x00);
	dsi_generic_write_seq(dsi, 0xbd, 0x58, 0x00);
	dsi_generic_write_seq(dsi, 0xc0, 0x04);
	dsi_generic_write_seq(dsi, 0xca, 0x00);
	dsi_generic_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x02);
	dsi_generic_write_seq(dsi, 0xee, 0x03);
	dsi_generic_write_seq(dsi, 0xb0,
			      0x00, 0x05, 0x00, 0x2e, 0x00, 0x43, 0x00, 0x6c,
			      0x00, 0x86, 0x00, 0xaf, 0x00, 0xd0, 0x01, 0x02);
	dsi_generic_write_seq(dsi, 0xb1,
			      0x01, 0x2c, 0x01, 0x67, 0x01, 0x96, 0x01, 0xe4,
			      0x02, 0x22, 0x02, 0x24, 0x02, 0x60, 0x02, 0x9e);
	dsi_generic_write_seq(dsi, 0xb2,
			      0x02, 0xc5, 0x02, 0xf8, 0x03, 0x1d, 0x03, 0x4e,
			      0x03, 0x68, 0x03, 0x7d, 0x03, 0xa2, 0x03, 0xc7);
	dsi_generic_write_seq(dsi, 0xb3, 0x03, 0xd7, 0x03, 0xdb);
	dsi_generic_write_seq(dsi, 0xb4,
			      0x00, 0x99, 0x00, 0xa3, 0x00, 0xb8, 0x00, 0xc8,
			      0x00, 0xd7, 0x00, 0xf1, 0x01, 0x07, 0x01, 0x2c);
	dsi_generic_write_seq(dsi, 0xb5,
			      0x01, 0x4b, 0x01, 0x7f, 0x01, 0xab, 0x01, 0xf2,
			      0x02, 0x2b, 0x02, 0x2d, 0x02, 0x64, 0x02, 0xa2);
	dsi_generic_write_seq(dsi, 0xb6,
			      0x02, 0xc9, 0x02, 0xfa, 0x03, 0x1c, 0x03, 0x49,
			      0x03, 0x65, 0x03, 0x78, 0x03, 0x9e, 0x03, 0xc4);
	dsi_generic_write_seq(dsi, 0xb7, 0x03, 0xda, 0x03, 0xdb);
	dsi_generic_write_seq(dsi, 0xb8,
			      0x00, 0x02, 0x00, 0x03, 0x00, 0x11, 0x00, 0x41,
			      0x00, 0x62, 0x00, 0x92, 0x00, 0xb5, 0x00, 0xec);
	dsi_generic_write_seq(dsi, 0xb9,
			      0x01, 0x17, 0x01, 0x58, 0x01, 0x8a, 0x01, 0xdd,
			      0x02, 0x1e, 0x02, 0x1f, 0x02, 0x5b, 0x02, 0x9b);
	dsi_generic_write_seq(dsi, 0xba,
			      0x02, 0xc5, 0x02, 0xf9, 0x03, 0x22, 0x03, 0x5c,
			      0x03, 0x8f, 0x03, 0xfd, 0x03, 0xfd, 0x03, 0xfd);
	dsi_generic_write_seq(dsi, 0xbb, 0x03, 0xfe, 0x03, 0xfe);
	dsi_generic_write_seq(dsi, 0x6f, 0x02);
	dsi_generic_write_seq(dsi, 0xf7, 0x47);
	dsi_generic_write_seq(dsi, 0x6f, 0x0a);
	dsi_generic_write_seq(dsi, 0xf7, 0x02);
	dsi_generic_write_seq(dsi, 0x6f, 0x17);
	dsi_generic_write_seq(dsi, 0xf4, 0x70);
	dsi_generic_write_seq(dsi, 0x6f, 0x11);
	dsi_generic_write_seq(dsi, 0xf3, 0x01);
	dsi_generic_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x03);
	dsi_generic_write_seq(dsi, 0xb0, 0x20, 0x00);
	dsi_generic_write_seq(dsi, 0xb1, 0x20, 0x00);
	dsi_generic_write_seq(dsi, 0xb2, 0x05, 0x00, 0x00, 0x00, 0x90);
	dsi_generic_write_seq(dsi, 0xb3, 0x05, 0x00, 0x00, 0x00, 0x90);
	dsi_generic_write_seq(dsi, 0xb4, 0x05, 0x00, 0x00, 0x00, 0x90);
	dsi_generic_write_seq(dsi, 0xb5, 0x05, 0x00, 0x00, 0x00, 0x90);
	dsi_generic_write_seq(dsi, 0xb6, 0x05, 0x00, 0x00, 0x00, 0x90);
	dsi_generic_write_seq(dsi, 0xb7, 0x05, 0x00, 0x00, 0x00, 0x90);
	dsi_generic_write_seq(dsi, 0xb8, 0x05, 0x00, 0x00, 0x00, 0x90);
	dsi_generic_write_seq(dsi, 0xb9, 0x05, 0x00, 0x00, 0x00, 0x90);
	dsi_generic_write_seq(dsi, 0xba, 0x53, 0x01, 0x00, 0x01, 0x00);
	dsi_generic_write_seq(dsi, 0xbb, 0x53, 0x01, 0x00, 0x01, 0x00);
	dsi_generic_write_seq(dsi, 0xbc, 0x53, 0x01, 0x00, 0x01, 0x00);
	dsi_generic_write_seq(dsi, 0xbd, 0x53, 0x01, 0x00, 0x01, 0x00);
	dsi_generic_write_seq(dsi, 0xc4, 0x60);
	dsi_generic_write_seq(dsi, 0xc5, 0x40);
	dsi_generic_write_seq(dsi, 0xc6, 0x60);
	dsi_generic_write_seq(dsi, 0xc7, 0x40);
	dsi_generic_write_seq(dsi, 0x6f, 0x01);
	dsi_generic_write_seq(dsi, 0xf9, 0x46);
	dsi_generic_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x05);
	dsi_generic_write_seq(dsi, 0xed, 0x30);
	dsi_generic_write_seq(dsi, 0xe5, 0x00);
	dsi_generic_write_seq(dsi, 0xb0, 0x17, 0x06);
	dsi_generic_write_seq(dsi, 0xb8, 0x00);
	dsi_generic_write_seq(dsi, 0xbd, 0x03, 0x03, 0x01, 0x00, 0x03);
	dsi_generic_write_seq(dsi, 0xb1, 0x17, 0x06);
	dsi_generic_write_seq(dsi, 0xb9, 0x00, 0x03);
	dsi_generic_write_seq(dsi, 0xb2, 0x17, 0x06);
	dsi_generic_write_seq(dsi, 0xba, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xb3, 0x17, 0x06);
	dsi_generic_write_seq(dsi, 0xbb, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xb4, 0x17, 0x06);
	dsi_generic_write_seq(dsi, 0xb5, 0x17, 0x06);
	dsi_generic_write_seq(dsi, 0xb6, 0x17, 0x06);
	dsi_generic_write_seq(dsi, 0xb7, 0x17, 0x06);
	dsi_generic_write_seq(dsi, 0xbc, 0x00, 0x03);
	dsi_generic_write_seq(dsi, 0xe5, 0x06);
	dsi_generic_write_seq(dsi, 0xe6, 0x06);
	dsi_generic_write_seq(dsi, 0xe7, 0x06);
	dsi_generic_write_seq(dsi, 0xe8, 0x06);
	dsi_generic_write_seq(dsi, 0xe9, 0x06);
	dsi_generic_write_seq(dsi, 0xea, 0x06);
	dsi_generic_write_seq(dsi, 0xeb, 0x06);
	dsi_generic_write_seq(dsi, 0xec, 0x06);
	dsi_generic_write_seq(dsi, 0xc0, 0x0b);
	dsi_generic_write_seq(dsi, 0xc1, 0x09);
	dsi_generic_write_seq(dsi, 0xc2, 0x0b);
	dsi_generic_write_seq(dsi, 0xc3, 0x09);
	dsi_generic_write_seq(dsi, 0xc4, 0x10);
	dsi_generic_write_seq(dsi, 0xc5, 0x10);
	dsi_generic_write_seq(dsi, 0xc6, 0x10);
	dsi_generic_write_seq(dsi, 0xc7, 0x10);
	dsi_generic_write_seq(dsi, 0xc8, 0x08, 0x20);
	dsi_generic_write_seq(dsi, 0xc9, 0x04, 0x20);
	dsi_generic_write_seq(dsi, 0xca, 0x07, 0x00);
	dsi_generic_write_seq(dsi, 0xcb, 0x03, 0x00);
	dsi_generic_write_seq(dsi, 0xd1, 0x00, 0x05, 0x00, 0x07, 0x10);
	dsi_generic_write_seq(dsi, 0xd2, 0x00, 0x05, 0x04, 0x07, 0x10);
	dsi_generic_write_seq(dsi, 0xd3, 0x00, 0x00, 0x0a, 0x07, 0x10);
	dsi_generic_write_seq(dsi, 0xd4, 0x00, 0x00, 0x0a, 0x07, 0x10);
	dsi_generic_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x05);
	dsi_generic_write_seq(dsi, 0xd0,
			      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xd5,
			      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			      0x00, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xd6,
			      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			      0x00, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xd7,
			      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			      0x00, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x06);
	dsi_generic_write_seq(dsi, 0xb0, 0x12, 0x10);
	dsi_generic_write_seq(dsi, 0xb1, 0x18, 0x16);
	dsi_generic_write_seq(dsi, 0xb2, 0x00, 0x02);
	dsi_generic_write_seq(dsi, 0xb3, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xb4, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xb5, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xb6, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xb7, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xb8, 0x31, 0x08);
	dsi_generic_write_seq(dsi, 0xb9, 0x2e, 0x2d);
	dsi_generic_write_seq(dsi, 0xba, 0x2d, 0x2e);
	dsi_generic_write_seq(dsi, 0xbb, 0x09, 0x31);
	dsi_generic_write_seq(dsi, 0xbc, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xbd, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xbe, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xbf, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xc0, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xc1, 0x03, 0x01);
	dsi_generic_write_seq(dsi, 0xc2, 0x17, 0x19);
	dsi_generic_write_seq(dsi, 0xc3, 0x11, 0x13);
	dsi_generic_write_seq(dsi, 0xe5, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xc4, 0x17, 0x19);
	dsi_generic_write_seq(dsi, 0xc5, 0x11, 0x13);
	dsi_generic_write_seq(dsi, 0xc6, 0x03, 0x01);
	dsi_generic_write_seq(dsi, 0xc7, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xc8, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xc9, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xca, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xcb, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xcc, 0x31, 0x09);
	dsi_generic_write_seq(dsi, 0xcd, 0x2d, 0x2e);
	dsi_generic_write_seq(dsi, 0xce, 0x2e, 0x2d);
	dsi_generic_write_seq(dsi, 0xcf, 0x08, 0x31);
	dsi_generic_write_seq(dsi, 0xd0, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xd1, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xd2, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xd3, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xd4, 0x31, 0x31);
	dsi_generic_write_seq(dsi, 0xd5, 0x00, 0x02);
	dsi_generic_write_seq(dsi, 0xd6, 0x12, 0x10);
	dsi_generic_write_seq(dsi, 0xd7, 0x18, 0x16);
	dsi_generic_write_seq(dsi, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xd9, 0x00, 0x00, 0x00, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xe7, 0x00);
	dsi_generic_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x00);
	dsi_generic_write_seq(dsi, 0xe6, 0xff, 0xff, 0xfa, 0xfa);
	dsi_generic_write_seq(dsi, 0xe8,
			      0xf3, 0xe8, 0xe0, 0xd8, 0xce, 0xc4, 0xba, 0xb0,
			      0xa6, 0x9c);
	dsi_generic_write_seq(dsi, 0xcc,
			      0x41, 0x36, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00,
			      0x00, 0x00, 0x00, 0x00, 0x40, 0x08, 0xa5, 0x05);
	dsi_generic_write_seq(dsi, 0xd1,
			      0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x03,
			      0x03, 0x03, 0x02, 0x02, 0x02, 0x01, 0x01, 0x00);
	dsi_generic_write_seq(dsi, 0xd7,
			      0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
			      0x00, 0x00, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xd8,
			      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			      0x00, 0x00, 0x02, 0x01, 0x00);
	dsi_generic_write_seq(dsi, 0xd9, 0x02, 0x09);
	dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_POWER_SAVE, 0x81);
	dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x24);

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x0000);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_CABC_MIN_BRIGHTNESS, 0x28);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}
	msleep(20);

	return 0;
}

static int tianma_nt35521_5p5_off(struct tianma_nt35521_5p5 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	msleep(20);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	return 0;
}

static int tianma_nt35521_5p5_prepare(struct drm_panel *panel)
{
	struct tianma_nt35521_5p5 *ctx = to_tianma_nt35521_5p5(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators: %d\n", ret);
		return ret;
	}

	tianma_nt35521_5p5_reset(ctx);

	ret = tianma_nt35521_5p5_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int tianma_nt35521_5p5_unprepare(struct drm_panel *panel)
{
	struct tianma_nt35521_5p5 *ctx = to_tianma_nt35521_5p5(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = tianma_nt35521_5p5_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode tianma_nt35521_5p5_mode = {
	.clock = (720 + 88 + 12 + 88) * (1280 + 20 + 3 + 20) * 60 / 1000,
	.hdisplay = 720,
	.hsync_start = 720 + 88,
	.hsync_end = 720 + 88 + 12,
	.htotal = 720 + 88 + 12 + 88,
	.vdisplay = 1280,
	.vsync_start = 1280 + 20,
	.vsync_end = 1280 + 20 + 3,
	.vtotal = 1280 + 20 + 3 + 20,
	.width_mm = 68,
	.height_mm = 121,
};

static int tianma_nt35521_5p5_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &tianma_nt35521_5p5_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs tianma_nt35521_5p5_panel_funcs = {
	.prepare = tianma_nt35521_5p5_prepare,
	.unprepare = tianma_nt35521_5p5_unprepare,
	.get_modes = tianma_nt35521_5p5_get_modes,
};

static int tianma_nt35521_5p5_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = backlight_get_brightness(bl);
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return 0;
}

static const struct backlight_ops tianma_nt35521_5p5_bl_ops = {
	.update_status = tianma_nt35521_5p5_bl_update_status,
};

static struct backlight_device *
tianma_nt35521_5p5_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 250,
		.max_brightness = 250,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &tianma_nt35521_5p5_bl_ops, &props);
}

static int tianma_nt35521_5p5_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct tianma_nt35521_5p5 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->supplies[0].supply = "vsp";
	ctx->supplies[1].supply = "vsn";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to get regulators\n");

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &tianma_nt35521_5p5_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ctx->panel.backlight = tianma_nt35521_5p5_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static int tianma_nt35521_5p5_remove(struct mipi_dsi_device *dsi)
{
	struct tianma_nt35521_5p5 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id tianma_nt35521_5p5_of_match[] = {
	{ .compatible = "huawei,tianma-nt35521" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, tianma_nt35521_5p5_of_match);

static struct mipi_dsi_driver tianma_nt35521_5p5_driver = {
	.probe = tianma_nt35521_5p5_probe,
	.remove = tianma_nt35521_5p5_remove,
	.driver = {
		.name = "panel-tianma-nt35521-5p5",
		.of_match_table = tianma_nt35521_5p5_of_match,
	},
};
module_mipi_dsi_driver(tianma_nt35521_5p5_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for TIANMA_NT35521_5P5_720P_VIDEO");
MODULE_LICENSE("GPL v2");
