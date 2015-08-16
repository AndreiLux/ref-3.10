/*
 * arizona_control.c - WolfonMicro / CirrusLogic Arizona kernel-space register control
 *
 * @Author	: Andrei F. <https://github.com/AndreiLux>
 * @Date	: June 2013 	- Arizona original implementation.
 * 		: December 2014	- Florida impl.
 * 		: August 2015 	- Clearwater impl.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#include <linux/sysfs.h>
#include <linux/miscdevice.h>

#include <linux/mfd/arizona/registers.h>
#include <linux/mfd/arizona/control.h>

#if 0
#define DEBUG
#endif

#define NOT_INIT		123456
#define DEF_HP_MEDIA_VOL	113

static struct snd_soc_codec *codec = NULL;
static int ignore_next = 0;

static ssize_t show_arizona_property(struct device *dev,
				    struct device_attribute *attr, char *buf);

static ssize_t store_arizona_property(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count);

enum control_type {
	CTL_VIRTUAL = 0,
	CTL_ACTIVE,
	CTL_INERT,
	CTL_HIDDEN,
	CTL_MONITOR,
};

#define _ctl(name_, type_, reg_, mask_, shift_, hook_)\
{ 									\
	.attribute = {							\
			.attr = {					\
				  .name = name_,			\
				  .mode = 0664,				\
				},					\
			.show = show_arizona_property,			\
			.store = store_arizona_property,		\
		     },							\
	.type	= type_ ,						\
	.reg 	= reg_ ,						\
	.mask 	= mask_ ,						\
	.shift 	= shift_ ,						\
	.value 	= NOT_INIT ,						\
	.ctlval = 0 ,							\
	.hook 	= hook_							\
}

struct arizona_control {
	const struct device_attribute	attribute;
	enum control_type 		type;
	u16				reg;
	u16				mask;
	u8				shift;
	int				value;
	u16				ctlval;
	unsigned int 			(*hook)(struct arizona_control *ctl);
};

/* Prototypes */

#define _write(reg, regval)	\
	ignore_next = true;	\
	codec->write(codec, reg, regval);

#define _read(reg) codec->read(codec, reg)

static inline void _ctl_set(struct arizona_control *ctl, int val)
{
	ctl->value = val;
	_write(ctl->reg, 
		(_read(ctl->reg) & ~ctl->mask) | (ctl->hook(ctl) << ctl->shift));
}

static unsigned int _delta(struct arizona_control *ctl,
			   unsigned int val, int offset)
{
	if ((val + offset) > (ctl->mask >> ctl->shift))
		return (ctl->mask >> ctl->shift);

	if ((val + offset) < 0 )
		return 0;

	return (val + offset);
}

/* Value hooks */

static unsigned int __simple(struct arizona_control *ctl)
{
	return ctl->value;
}

static unsigned int __delta(struct arizona_control *ctl)
{
	return _delta(ctl, ctl->ctlval, ctl->value);
}

static unsigned int __hp_volume(struct arizona_control *ctl)
{
	return ctl->ctlval == DEF_HP_MEDIA_VOL ? ctl->value : ctl->ctlval;
}

static unsigned int hp_callback(struct arizona_control *ctl);
static unsigned int eq_gain(struct arizona_control *ctl);
static unsigned int eq_gains_all(struct arizona_control *ctl);

/* Sound controls */

enum sound_control {
	HPLDVOL = 0, HPRDVOL, HPLIVOL, HPRIVOL, EPVOL,
	EQ_HP, EQ_HP_CH, HP_MONO, 
	EQ1ENA, EQ2ENA, EQ3ENA, EQ4ENA,
	POUT1L,
	
#define ME(s)	s, s##VOL
	ME(EQ1MIX1), ME(EQ1MIX2), ME(EQ2MIX1), ME(EQ2MIX2), ME(EQ3MIX1), ME(EQ4MIX1),
	ME(HPOUT1L1), ME(HPOUT1L2), ME(HPOUT1R1), ME(HPOUT1R2),
	ME(AIF4TX1), ME(AIF4TX2),
	
	EQAFREQS, EQBFREQS,
	EQHPL1G, EQHPL2G, EQHPL3G, EQHPL4G, EQHPL5G, EQHPL6G, EQHPL7G, EQHPL8G,
	EQHPR1G, EQHPR2G, EQHPR3G, EQHPR4G, EQHPR5G, EQHPR6G, EQHPR7G, EQHPR8G,
	
#define EE(n) EQ##n##B1G, EQ##n##B2G, EQ##n##B3G, EQ##n##B4G, EQ##n##B5G
	EE(1), EE(2), EE(3), EE(4), 
};

static struct arizona_control ctls[] = {
	/* Volumes */
	
	_ctl("hp_left_dvol", CTL_ACTIVE, ARIZONA_DAC_DIGITAL_VOLUME_1L,
		ARIZONA_OUT1L_VOL_MASK, ARIZONA_OUT1L_VOL_SHIFT, __hp_volume),
	_ctl("hp_right_dvol", CTL_ACTIVE, ARIZONA_DAC_DIGITAL_VOLUME_1R, 
		ARIZONA_OUT1R_VOL_MASK, ARIZONA_OUT1R_VOL_SHIFT, __hp_volume),
	
	_ctl("hp_left_ivol", CTL_ACTIVE, ARIZONA_DAC_VOLUME_LIMIT_1L,
		ARIZONA_OUT1L_VOL_LIM_MASK, ARIZONA_OUT1L_VOL_LIM_SHIFT, __delta),
	_ctl("hp_right_ivol", CTL_ACTIVE, ARIZONA_DAC_VOLUME_LIMIT_1R, 
		ARIZONA_OUT1R_VOL_LIM_MASK, ARIZONA_OUT1R_VOL_LIM_SHIFT, __delta),

	_ctl("ep_dvol", CTL_ACTIVE, ARIZONA_DAC_DIGITAL_VOLUME_3L,
		ARIZONA_OUT3L_VOL_MASK, ARIZONA_OUT3L_VOL_SHIFT, __delta),

	/* Master switches */

	_ctl("switch_eq_hp", CTL_VIRTUAL, 0, 0, 0, hp_callback),
	_ctl("switch_eq_hp_per_ch", CTL_VIRTUAL, 0, 0, 0, eq_gains_all),
	_ctl("switch_hp_mono", CTL_VIRTUAL, 0, 0, 0, hp_callback),

	/* Internal switches */

	_ctl("eq1_enable", CTL_INERT, ARIZONA_EQ1_1, ARIZONA_EQ1_ENA_MASK,
		ARIZONA_EQ1_ENA_SHIFT, __simple),
	_ctl("eq2_enable", CTL_INERT, ARIZONA_EQ2_1, ARIZONA_EQ2_ENA_MASK,
		ARIZONA_EQ2_ENA_SHIFT, __simple),
	_ctl("eq3_enable", CTL_INERT, ARIZONA_EQ3_1, ARIZONA_EQ3_ENA_MASK,
		ARIZONA_EQ3_ENA_SHIFT, __simple),
	_ctl("eq4_enable", CTL_INERT, ARIZONA_EQ4_1, ARIZONA_EQ4_ENA_MASK,
		ARIZONA_EQ4_ENA_SHIFT, __simple),

	/* Path domain */

	_ctl("out1l_enable", CTL_MONITOR, ARIZONA_OUTPUT_ENABLES_1,
		ARIZONA_OUT1L_ENA_MASK, ARIZONA_OUT1L_ENA_SHIFT, hp_callback),

	/* Mixers */

#define MIXER(s, r, n)								   \
	_ctl( s "_input"#n"_source", CTL_INERT, ARIZONA_##r##_INPUT_##n##_SOURCE,  \
			0xff, 0, __simple),\
	_ctl( s "_input"#n"_volume", CTL_INERT, ARIZONA_##r##_INPUT_##n##_VOLUME,  \
			ARIZONA_MIXER_VOL_MASK, ARIZONA_MIXER_VOL_SHIFT, __simple)
	
	MIXER("eq1", EQ1MIX, 1),
	MIXER("eq1", EQ1MIX, 2),
	
	MIXER("eq2", EQ2MIX, 1),
	MIXER("eq2", EQ2MIX, 2),
	
	MIXER("eq3", EQ3MIX, 1),
	MIXER("eq4", EQ4MIX, 1),
	
	MIXER("out1l", OUT1LMIX, 1),
	MIXER("out1l", OUT1LMIX, 2),
	
	MIXER("out1r", OUT1RMIX, 1),
	MIXER("out1r", OUT1RMIX, 2),
	
	MIXER("aif4tx1", AIF4TX1MIX, 1),
	MIXER("aif4tx2", AIF4TX1MIX, 1),

	/* EQ Configurables */

	_ctl("eq_A_freqs", CTL_VIRTUAL, 0, 0, 0, __simple),
	_ctl("eq_B_freqs", CTL_VIRTUAL, 0, 0, 0, __simple),
	
#define EL(n) _ctl("eq_hpl_gain_"#n, CTL_VIRTUAL, 0, 0, 0, eq_gain)
#define ER(n) _ctl("eq_hpr_gain_"#n, CTL_VIRTUAL, 0, 0, 0, eq_gain)
#define ELA() EL(1), EL(2), EL(3), EL(4), EL(5), EL(6), EL(7), EL(8)
#define ERA() ER(1), ER(2), ER(3), ER(4), ER(5), ER(6), ER(7), ER(8)

	ELA(), ERA(),
	
#define EQ_BANDS(n)										\
	_ctl("eq"#n"_gain_1", CTL_INERT, ARIZONA_EQ##n##_1, ARIZONA_EQ##n##_B1_GAIN_MASK,	\
		ARIZONA_EQ##n##_B1_GAIN_SHIFT, __simple),					\
	_ctl("eq"#n"_gain_2", CTL_INERT, ARIZONA_EQ##n##_1, ARIZONA_EQ##n##_B2_GAIN_MASK,	\
		ARIZONA_EQ##n##_B2_GAIN_SHIFT, __simple),					\
	_ctl("eq"#n"_gain_3", CTL_INERT, ARIZONA_EQ##n##_1, ARIZONA_EQ##n##_B3_GAIN_MASK,	\
		ARIZONA_EQ##n##_B3_GAIN_SHIFT, __simple),					\
	_ctl("eq"#n"_gain_4", CTL_INERT, ARIZONA_EQ##n##_2, ARIZONA_EQ##n##_B4_GAIN_MASK,	\
		ARIZONA_EQ##n##_B4_GAIN_SHIFT, __simple),					\
	_ctl("eq"#n"_gain_5", CTL_INERT, ARIZONA_EQ##n##_2, ARIZONA_EQ##n##_B5_GAIN_MASK,	\
		ARIZONA_EQ##n##_B5_GAIN_SHIFT, __simple)					\

	EQ_BANDS(1),
	EQ_BANDS(2),
	EQ_BANDS(3),
	EQ_BANDS(4),
};

static void _set_gain(int b)
{
	int pc = ctls[EQ_HP_CH].value;
	
#define BV_OFFSET	EQHPR1G - EQHPL1G
#define CH_OFFSET	EQ2B1G - EQ1B1G	
#define band_set(a, b)									\
	_ctl_set(&ctls[a], ctls[b].value + 12);						\
	_ctl_set(&ctls[a + CH_OFFSET], ctls[pc ? (b + BV_OFFSET) : b].value + 12);	\
	
	switch (b) {
		case 0:
		case 1: band_set(EQ1B1G, EQHPL1G); if (b) break;
		case 2: band_set(EQ1B2G, EQHPL2G); if (b) break;
		case 3: band_set(EQ1B3G, EQHPL3G); if (b) break;
		case 4: band_set(EQ1B4G, EQHPL4G); if (b) break;
		case 5: band_set(EQ3B2G, EQHPL5G); if (b) break;
		case 6: band_set(EQ3B3G, EQHPL6G); if (b) break;
		case 7: band_set(EQ3B4G, EQHPL7G); if (b) break;
		case 8: band_set(EQ3B5G, EQHPL8G); if (b) break;
	}
}

static unsigned int eq_gain(struct arizona_control *ctl)
{
	ptrdiff_t b = ctl - &ctls[EQHPL1G] + 1;
	
	if (b > 8)
		b -= 8;
	
	_set_gain((int) b);
	
	return ctl->value; 
}

static unsigned int eq_gains_all(struct arizona_control *ctl)
{
	_set_gain(0);
	
	return ctl->ctlval; 
}

static unsigned int hp_callback(struct arizona_control *ctl)
{
	bool eq = ctls[EQ_HP].value;
	bool mono = ctls[HP_MONO].value;
	bool power = ctls[POUT1L].value;

	if (power) {
		if (eq) {
			if (mono) {
				_ctl_set(&ctls[EQ1MIX1], 32);
				_ctl_set(&ctls[EQ1MIX2], 33);
				
				_ctl_set(&ctls[EQ3MIX1], 80);
				
				_ctl_set(&ctls[HPOUT1L1], 82);
				_ctl_set(&ctls[HPOUT1R1], 82);
			} else {
				_ctl_set(&ctls[EQ1MIX1], 32);
				_ctl_set(&ctls[EQ1MIX2], 0);
				
				_ctl_set(&ctls[EQ2MIX1], 33);
				_ctl_set(&ctls[EQ3MIX1], 80);
				_ctl_set(&ctls[EQ4MIX1], 81);
				
				_ctl_set(&ctls[HPOUT1L1], 82);
				_ctl_set(&ctls[HPOUT1R1], 83);
			}
			
			_set_gain(0);
			_ctl_set(&ctls[EQ1B5G], 12);
			_ctl_set(&ctls[EQ3B1G], 12);
			_ctl_set(&ctls[EQ2B5G], 12);
			_ctl_set(&ctls[EQ4B1G], 12);
		} else {
			_ctl_set(&ctls[HPOUT1L1], 32);
			_ctl_set(&ctls[HPOUT1R1], 33);
			
			_ctl_set(&ctls[HPOUT1L2], mono ? 33 : 0);
			_ctl_set(&ctls[HPOUT1R2], mono ? 32 : 0);
		}
	}
	
	_ctl_set(&ctls[EQ1ENA], power && eq);
	_ctl_set(&ctls[EQ2ENA], power && eq && !mono);
	_ctl_set(&ctls[EQ3ENA], power && eq);
	_ctl_set(&ctls[EQ4ENA], power && eq && !mono);

	return ctl->ctlval;
}

/* Interface */

static bool is_delta(struct arizona_control *ctl)
{
	if (ctl->hook == __delta)
		return true;

	return false;
}

static bool is_ignorable(struct arizona_control *ctl)
{
	switch (ctl->type) {
		case CTL_INERT:
		case CTL_MONITOR:
			return true;
		default:
			return false;
	}
}

void arizona_control_regmap_hook(struct regmap *pmap, unsigned int reg,
		      		unsigned int *val)
{
	struct arizona_control *ctl = &ctls[0];
	unsigned int virgin = *val;

	if (codec == NULL || pmap != codec->control_data)
		return;

	if (ignore_next) {
		ignore_next = false;
		return;
	}

#ifdef DEBUG
	printk("%s: pre: %x -> %u\n", __func__, reg, *val);
#endif

	while (ctl < (&ctls[0] + ARRAY_SIZE(ctls))) {
		if (ctl->reg == reg && ctl->type != CTL_VIRTUAL) {
			ctl->ctlval = (virgin & ctl->mask) >> ctl->shift;

			if (unlikely(ctl->value == NOT_INIT)) {
				if (is_delta(ctl))
					ctl->value = 0;
				else
					ctl->value = ctl->ctlval;
			}

			if (ctl->type == CTL_MONITOR) {
				ctl->value = ctl->ctlval;
				ctl->hook(ctl);
				goto next;
			}

			if (is_ignorable(ctl))
				goto next;

			*val &= ~ctl->mask;
			*val |= ctl->hook(ctl) << ctl->shift;
			
#ifdef DEBUG
			printk("%s: %s: %x -> %u\n", __func__, ctl->attribute.attr.name, virgin, *val);
#endif
		}
next:
		++ctl;
	}
}

/**** Sysfs ****/

static unsigned int _eq_freq_store(unsigned int reg, bool pair,
				   const char *buf, size_t count)
{
	unsigned int vals[18] = { 0 };
	int bytes, len = 0, n = 0;

	while (n < 18) {
		sscanf(buf + len, "%x%n", &vals[n], &bytes);
		len += bytes;
		_write(reg + n, vals[n]);
		if (pair)
			_write(reg + n + 22, vals[n]);
		++n;
	}

	return count;
}

static unsigned int _eq_freq_show(unsigned int reg, char *buf)
{
	int len = 0, n = 0;

	while (n < 18)
		len += sprintf(buf + len, "0x%04x ", _read(reg + n++));

	len += sprintf(buf + len, "\n");

	return len;
}

static ssize_t show_arizona_property(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct arizona_control *ctl = (struct arizona_control*)(attr);

	if (ctl == &ctls[EQAFREQS])
		return _eq_freq_show(ARIZONA_EQ1_3, buf);

	if (ctl == &ctls[EQBFREQS])
		return _eq_freq_show(ARIZONA_EQ3_3, buf);

	if (unlikely(ctl->value == NOT_INIT)) {
		ctl->ctlval = (_read(ctl->reg) & ctl->mask) >> ctl->shift;
		if (is_delta(ctl))
			ctl->value = 0;
		else
			ctl->value = ctl->ctlval;
	}

#ifdef DEBUG
	return sprintf(buf, "%d %d %u\n", ctl->value,
		(_read(ctl->reg) & ctl->mask) >> ctl->shift, ctl->ctlval);
#endif
	return sprintf(buf, "%d\n", ctl->value);
};

static ssize_t store_arizona_property(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct arizona_control *ctl = (struct arizona_control*)(attr);
	unsigned int regval;
	int val;

	if (ctl == &ctls[EQAFREQS])
		return _eq_freq_store(ARIZONA_EQ1_3, true, buf, count);

	if (ctl == &ctls[EQBFREQS])
		return _eq_freq_store(ARIZONA_EQ3_3, true, buf, count);
	
	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	if (ctl->type != CTL_VIRTUAL) {
		if (val > (ctl->mask >> ctl->shift))
			val = (ctl->mask >> ctl->shift);

		if (val < -(ctl->mask >> ctl->shift))
			val = -(ctl->mask >> ctl->shift);

		ctl->value = val;

		regval = _read(ctl->reg);
		regval &= ~ctl->mask;
		regval |= ctl->hook(ctl) << ctl->shift;

		_write(ctl->reg, regval);
	} else {
		ctl->value = val;
		ctl->hook(ctl);
	}

	return count;
};

static struct miscdevice sound_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "arizona_control",
};

void arizona_control_init(struct snd_soc_codec *pcodec)
{
	int i, ret;

	codec = pcodec;

	misc_register(&sound_dev);

	for(i = 0; i < ARRAY_SIZE(ctls); i++) {
#ifndef DEBUG
		if (ctls[i].type != CTL_HIDDEN && !is_ignorable(&ctls[i]))
#endif
			ret = sysfs_create_file(&sound_dev.this_device->kobj,
						&ctls[i].attribute.attr);
	}
	
	for (i = EQHPL1G; i <= EQHPR8G; i++)
		ctls[i].value = 0;
	
	for (i = EQ_HP; i <= HP_MONO; i++)
		ctls[i].value = 0;
	
}
