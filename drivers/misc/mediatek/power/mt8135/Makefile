include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -Werror

ifeq ($(CONFIG_MTK_INTERNAL_MHL_SUPPORT), y)
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/mhltx/
endif

ifeq ($(MTK_BATTERY_NO_HAL), yes)
ccflags-y += -DMTK_BATTERY_NO_HAL
endif

obj-$(CONFIG_MTK_PMIC)          += pmic_mt6397.o

ifeq ($(CONFIG_MTK_BATTERY_THROTTLING),y)
    obj-$(CONFIG_MTK_BATTERY_THROTTLING) += mt_battery_throttle.o
else
    ifeq ($(CONFIG_MTK_BATTERY_PROTECT),y)
        obj-$(CONFIG_MTK_BATTERY_PROTECT) += mt_battery_protect.o
    endif
endif

ifneq ($(MTK_BATTERY_NO_HAL), yes)
obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter_hal.o
endif

ifeq ($(CONFIG_MTK_BQ27541_SUPPORT),y)
    obj-$(CONFIG_MTK_SMART_BATTERY) += bq27541.o
else
    ifeq ($(CONFIG_MTK_BQ27741_SUPPORT),y)
        obj-$(CONFIG_MTK_SMART_BATTERY) += bq27741.o
    endif
endif

ifeq ($(CONFIG_MTK_BQ24196_SUPPORT),y)
    ifeq ($(MTK_BATTERY_NO_HAL), yes)
        obj-$(CONFIG_MTK_SMART_BATTERY) += mt_battery_bq24196.o mt6320_fuel_gauge_bq24196.o bq24196.o
    else
        obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_bq24196.o bq24196.o tbl_helper_func.o
    endif
else
    ifeq ($(CONFIG_MTK_NCP1851_SUPPORT),y)
        obj-$(CONFIG_MTK_SMART_BATTERY) += mt_battery_ncp1851.o mt6320_fuel_gauge_ncp1851.o ncp1851.o
    else
        ifeq ($(CONFIG_MTK_BQ24297_SUPPORT),y)
            obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_bq24297.o bq24297.o tbl_helper_func.o
        else
            ifeq ($(CONFIG_MTK_FAN5405_SUPPORT),y)
                obj-$(CONFIG_MTK_SMART_BATTERY) += fan5405.o charging_hw_fan5405.o
            else
                ifeq ($(CONFIG_MTK_BQ24156_SUPPORT),y)
                    obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_bq24156.o bq24156.o tbl_helper_func.o
                else
                    ifeq ($(CONFIG_MTK_BQ24158_SUPPORT),y)
                        obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_bq24158.o bq24158.o tbl_helper_func.o
                    else
			      obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_pmic.o
			  endif
		endif
            endif
        endif
    endif
endif
