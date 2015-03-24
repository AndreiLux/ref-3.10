ccflags-y += -I$(srctree) -Werror
obj-  := dummy.o

obj-$(CONFIG_THERMAL) += mtk_thermal_platform.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa_thput.o
#obj-$(CONFIG_THERMAL) += mtk_ts_skin.o

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
endif

# Only included in non-EVB projects
ifeq ($(CONFIG_POWER_EXT), y)
else
    ifeq ($(CONFIG_MTK_SMART_BATTERY), y)
        # Always align the Makefile in mediatek/platform/mt8135/kernel/drivers/power/
        ifeq ($(CONFIG_MTK_NCP1851_SUPPORT),y)
            obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
        else
            ifeq ($(CONFIG_MTK_BQ24196_SUPPORT),y)
                obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
            else
                # FAN5405 or PMIC6320 pulse charging or new charger IC
                obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
            endif
        endif
    endif
endif

ifeq ($(CONFIG_THERMAL),y)
    obj-y += mtk_thermal_budget.o
    obj-y += mtk_ts_cpu.o
    obj-y += mtk_ts_pmic.o
    obj-y += mtk_ts_pa.o

    ifeq ($(CONFIG_MODEM_UMTS_TDD128_MODE),y)
        obj-y += mtk_ts_tdpa.o
    endif

    ifeq ($(CONFIG_POWER_EXT), y)
    else
        ifeq ($(CONFIG_MTK_SMART_BATTERY), y)
            # Always align the Makefile in mediatek/platform/mt6589/kernel/drivers/power/
            ifeq ($(CONFIG_MTK_NCP1851_SUPPORT),y)
                obj-y += mtk_ts_battery.o
            else
                ifeq ($(CONFIG_MTK_BQ24196_SUPPORT),y)
                    obj-y += mtk_ts_battery.o
                else
                    # FAN5405 or PMIC6320 pulse charging
                    obj-y += mtk_ts_battery.o
                endif
            endif
        endif
    endif
endif

ifeq ($(CONFIG_MTK_TMP103_SUPPORT),y)
#obj-$(CONFIG_THERMAL) += mtk_ts_pcb.o
endif
