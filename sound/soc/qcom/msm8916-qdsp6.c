// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <uapi/linux/input-event-codes.h>
#include <dt-bindings/sound/qcom,q6afe.h>

#include "common.h"
#include "qdsp6/q6afe.h"

enum {
	MI2S_PRIMARY,
	MI2S_SECONDARY,
	MI2S_TERTIARY,
	MI2S_QUATERNARY,
	MI2S_COUNT
};

struct msm8916_qdsp6_data {
	void __iomem *mic_iomux;
	void __iomem *spkr_iomux;
	struct snd_soc_jack jack;
	bool jack_setup;
	unsigned int mi2s_clk_count[MI2S_COUNT];
};

#define MIC_CTRL_TER_WS_SLAVE_SEL	BIT(21)
#define MIC_CTRL_QUA_WS_SLAVE_SEL_10	BIT(17)
#define MIC_CTRL_TLMM_SCLK_EN		BIT(1)
#define SPKR_CTL_PRI_WS_SLAVE_SEL_11	(BIT(17) | BIT(16))
#define DEFAULT_MCLK_RATE		9600000
#define MI2S_BCLK_RATE			1536000

static int msm8916_qdsp6_get_mi2s_id(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	int id = asoc_rtd_to_cpu(rtd, 0)->id;

	if (id < PRIMARY_MI2S_RX || id > QUATERNARY_MI2S_TX) {
		dev_err(card->dev, "Unsupported CPU DAI: %d\n", id);
		return -EINVAL;
	}

	return (id - PRIMARY_MI2S_RX) / 2;
}

static int msm8916_qdsp6_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_soc_dai *codec_dai;
	struct snd_soc_component *component;
	struct snd_soc_card *card = rtd->card;
	struct msm8916_qdsp6_data *pdata = snd_soc_card_get_drvdata(card);
	int i, rval, mi2s;

	mi2s = msm8916_qdsp6_get_mi2s_id(rtd);
	if (mi2s < 0)
		return mi2s;

	switch (mi2s) {
	case MI2S_PRIMARY:
		writel(readl(pdata->spkr_iomux) | SPKR_CTL_PRI_WS_SLAVE_SEL_11,
			pdata->spkr_iomux);
		break;
	case MI2S_QUATERNARY:
		/* Configure the Quat MI2S to TLMM */
		writel(readl(pdata->mic_iomux) | MIC_CTRL_QUA_WS_SLAVE_SEL_10 |
			MIC_CTRL_TLMM_SCLK_EN,
			pdata->mic_iomux);
		break;
	case MI2S_TERTIARY:
		writel(readl(pdata->mic_iomux) | MIC_CTRL_TER_WS_SLAVE_SEL |
			MIC_CTRL_TLMM_SCLK_EN,
			pdata->mic_iomux);

		break;
	default:
		dev_err(card->dev, "unsupported cpu dai configuration\n");
		return -ENOTSUPP;

	}

	if (!pdata->jack_setup) {
		struct snd_jack *jack;

		rval = snd_soc_card_jack_new(card, "Headset Jack",
					     SND_JACK_HEADSET |
					     SND_JACK_HEADPHONE |
					     SND_JACK_BTN_0 | SND_JACK_BTN_1 |
					     SND_JACK_BTN_2 | SND_JACK_BTN_3 |
					     SND_JACK_BTN_4,
					     &pdata->jack, NULL, 0);

		if (rval < 0) {
			dev_err(card->dev, "Unable to add Headphone Jack\n");
			return rval;
		}

		jack = pdata->jack.jack;

		snd_jack_set_key(jack, SND_JACK_BTN_0, KEY_PLAYPAUSE);
		snd_jack_set_key(jack, SND_JACK_BTN_1, KEY_VOICECOMMAND);
		snd_jack_set_key(jack, SND_JACK_BTN_2, KEY_VOLUMEUP);
		snd_jack_set_key(jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN);
		pdata->jack_setup = true;
	}

	for_each_rtd_codec_dais(rtd, i, codec_dai) {
		component = codec_dai->component;

		/* Set default mclk for internal codec */
		rval = snd_soc_component_set_sysclk(component, 0, 0, DEFAULT_MCLK_RATE,
				       SND_SOC_CLOCK_IN);
		if (rval != 0 && rval != -ENOTSUPP) {
			dev_warn(card->dev, "Failed to set mclk: %d\n", rval);
			return rval;
		}
		rval = snd_soc_component_set_jack(component, &pdata->jack, NULL);
		if (rval != 0 && rval != -ENOTSUPP) {
			dev_warn(card->dev, "Failed to set jack: %d\n", rval);
			return rval;
		}
	}

	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);

	return 0;
}

static int msm8916_qdsp6_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8916_qdsp6_data *data = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int mi2s, ret;

	mi2s = msm8916_qdsp6_get_mi2s_id(rtd);
	if (mi2s < 0)
		return mi2s;

	if (++data->mi2s_clk_count[mi2s] > 1)
		return 0;

	ret = snd_soc_dai_set_sysclk(cpu_dai, LPAIF_BIT_CLK, MI2S_BCLK_RATE, 0);
	if (ret)
		dev_err(card->dev, "Failed to enable LPAIF bit clk: %d\n", ret);
	return ret;
}

static void msm8916_qdsp6_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm8916_qdsp6_data *data = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int mi2s, ret;

	mi2s = msm8916_qdsp6_get_mi2s_id(rtd);
	if (mi2s < 0)
		return;

	if (--data->mi2s_clk_count[mi2s] > 0)
		return;

	ret = snd_soc_dai_set_sysclk(cpu_dai, LPAIF_BIT_CLK, 0, 0);
	if (ret)
		dev_err(card->dev, "Failed to disable LPAIF bit clk: %d\n", ret);
}

static const struct snd_soc_ops msm8916_qdsp6_be_ops = {
	.startup = msm8916_qdsp6_startup,
	.shutdown = msm8916_qdsp6_shutdown,
};

static int msm8916_qdsp6_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					 struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_mask *fmt = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);

	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;
	snd_mask_set_format(fmt, SNDRV_PCM_FORMAT_S16_LE);

	return 0;
}

static void msm8916_qdsp6_add_ops(struct snd_soc_card *card)
{
	struct snd_soc_dai_link *link;
	int i;

	for_each_card_prelinks(card, i, link) {
		if (link->no_pcm) {
			link->init = msm8916_qdsp6_dai_init;
			link->ops = &msm8916_qdsp6_be_ops;
			link->be_hw_params_fixup = msm8916_qdsp6_hw_params_fixup;
		}
	}
}

static int msm8916_qdsp6_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct snd_soc_card *card;
	struct msm8916_qdsp6_data *data;
	struct resource *res;
	int ret;

	card = devm_kzalloc(dev, sizeof(*card), GFP_KERNEL);
	if (!card)
		return -ENOMEM;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	card->dev = dev;
	card->owner = THIS_MODULE;
	card->components = "qdsp6";
	ret = qcom_snd_parse_of(card);
	if (ret)
		return ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mic-iomux");
	data->mic_iomux = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->mic_iomux))
		return PTR_ERR(data->mic_iomux);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spkr-iomux");
	data->spkr_iomux = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->spkr_iomux))
		return PTR_ERR(data->spkr_iomux);

	snd_soc_card_set_drvdata(card, data);
	msm8916_qdsp6_add_ops(card);

	return devm_snd_soc_register_card(&pdev->dev, card);
}

static const struct of_device_id msm8916_qdsp6_device_id[]  = {
	{ .compatible = "qcom,msm8916-qdsp6-sndcard" },
	{},
};
MODULE_DEVICE_TABLE(of, msm8916_qdsp6_device_id);

static struct platform_driver msm8916_qdsp6_platform_driver = {
	.driver = {
		.name = "qcom-msm8916-qdsp6",
		.of_match_table = of_match_ptr(msm8916_qdsp6_device_id),
	},
	.probe = msm8916_qdsp6_platform_probe,
};
module_platform_driver(msm8916_qdsp6_platform_driver);

MODULE_AUTHOR("Minecrell <minecrell@minecrell.net>");
MODULE_DESCRIPTION("MSM8916 QDSP6 ASoC Machine Driver");
MODULE_LICENSE("GPL v2");
