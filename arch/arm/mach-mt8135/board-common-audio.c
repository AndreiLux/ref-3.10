#include <mach/board-common-audio.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#if defined(CONFIG_MTK_SOUND)
static struct mt_audio_platform_data audio_platform_data;
static u64 AudDrv_dmamask = 0xffffffffUL;
static struct platform_device AudDrv_device = {
	.name = "AudDrv_driver_device",
	.id = 0,
	.dev = {
		.platform_data = &audio_platform_data,
		.dma_mask = &AudDrv_dmamask,
		.coherent_dma_mask = 0xffffffffUL,
		}
};
#endif

#if defined(CONFIG_SND_SOC_MT8135)
static struct platform_device mtk_soc_dl1_pcm_device = {
	.name = "mt-soc-dl1-pcm",
	.id = PLATFORM_DEVID_NONE,
};

static struct platform_device mtk_soc_ul1_pcm_device = {
	.name = "mt-soc-ul1-pcm",
	.id = PLATFORM_DEVID_NONE,
};

static struct platform_device mtk_soc_pmic_codec_device = {
	.name = "mt-soc-codec",
	.id = PLATFORM_DEVID_NONE,
};

static struct platform_device mtk_soc_stub_dai_device = {
	.name = "mt-soc-dai-driver",
	.id = PLATFORM_DEVID_NONE,
};

static struct platform_device mtk_soc_stub_codec_device = {
	.name = "mt-soc-codec-stub",
	.id = PLATFORM_DEVID_NONE,
};

static struct platform_device mtk_soc_btsco_pcm_device = {
	.name = "mt-soc-btsco-pcm",
	.id = PLATFORM_DEVID_NONE,
};

static struct platform_device mtk_soc_routing_pcm_device = {
	.name = "mt-soc-routing-pcm",
	.id = PLATFORM_DEVID_NONE,
};

static struct platform_device mtk_soc_hdmi_pcm_device = {
	.name = "mt-soc-hdmi-pcm",
	.id = PLATFORM_DEVID_NONE,
};

static struct platform_device mtk_soc_hdmi_dai_device = {
	.name = "mt-audio-hdmi",
	.id = PLATFORM_DEVID_NONE,
};

static struct platform_device mtk_soc_dl1_awb_pcm_device = {
	.name = "mt-soc-dl1-awb-pcm",
	.id = PLATFORM_DEVID_NONE,
};


static struct mt_audio_platform_data audio_platform_data;

static struct platform_device mtk_soc_machine_device = {
	.name = "mt-soc-machine",
	.id = PLATFORM_DEVID_NONE,
	.dev = {
		.platform_data = &audio_platform_data,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		},
};

static struct platform_device *mtk_soc_audio_devices[] __initdata = {
	&mtk_soc_dl1_pcm_device,
	&mtk_soc_ul1_pcm_device,
	&mtk_soc_pmic_codec_device,
	&mtk_soc_stub_dai_device,
	&mtk_soc_stub_codec_device,
	&mtk_soc_btsco_pcm_device,
	&mtk_soc_routing_pcm_device,
	&mtk_soc_hdmi_pcm_device,
	&mtk_soc_hdmi_dai_device,
	&mtk_soc_machine_device,
	&mtk_soc_dl1_awb_pcm_device,
};
#endif

int __init mt_audio_init(struct mt_audio_custom_gpio_data *gpio_data, int mt_audio_board_channel_type)
{
	int ret = 0;

#if defined(CONFIG_SND_SOC_MT8135)
	memcpy(&(audio_platform_data.gpio_data), gpio_data, sizeof(struct mt_audio_custom_gpio_data));
	audio_platform_data.mt_audio_board_channel_type = mt_audio_board_channel_type;
	ret = platform_add_devices(mtk_soc_audio_devices, ARRAY_SIZE(mtk_soc_audio_devices));
	if (ret)
		pr_err("%s platform_add_devices fail %d\n", __func__, ret);
#elif defined(CONFIG_MTK_SOUND)
	memcpy(&(audio_platform_data.gpio_data), gpio_data, sizeof(struct mt_audio_custom_gpio_data));
	ret = platform_device_register(&AudDrv_device);
	if (ret)
		pr_err("%s platform_device_register fail %d\n", __func__, ret);
#endif

	return ret;
}
