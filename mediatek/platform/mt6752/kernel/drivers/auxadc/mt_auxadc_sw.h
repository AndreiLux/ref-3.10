#ifndef _MTK_ADC_SW_H
#define _MTK_ADC_SW_H

#define ADC_CHANNEL_MAX 16

#define MT_PDN_PERI_AUXADC MT_CG_INFRA_AUXADC

extern int IMM_auxadc_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
extern int IMM_auxadc_GetOneChannelValue_Cali(int Channel, int*voltage);
extern void mt_auxadc_hal_init(void);
extern void mt_auxadc_hal_suspend(void);
extern void mt_auxadc_hal_resume(void);
extern int mt_auxadc_dump_register(char *buf);

#ifdef MTK_DUAL_INPUT_CHARGER_SUPPORT
extern void mt_auxadc_enableBackgroundDection(u16 channel, u16 volt, u16 period, u16 debounce, u16 tFlag);
extern void mt_auxadc_disableBackgroundDection(u16 channel);
extern u16 mt_auxadc_getCurrentChannel(void);
extern u16 mt_auxadc_getCurrentTrigger(void);
#endif

#endif   /*_MTK_ADC_SW_H*/

