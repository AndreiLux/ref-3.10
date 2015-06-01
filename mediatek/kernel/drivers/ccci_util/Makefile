include $(MTK_ROOT_BUILD)/Makefile

obj-y += ccci_util_lib.o
ccci_util_lib-y := ccci_util_lib_fo.o
ccci_util_lib-y += ccci_util_lib_load_img.o
ccci_util_lib-y += ccci_util_lib_sys.o
ccci_util_lib-y += ccci_private_log.o
ccci_util_lib-y += ccci_util_lib_main.o

ifeq ($(MTK_SECURITY_SW_SUPPORT), yes)
ccflags-y  += -DENABLE_MD_IMG_SECURITY_FEATURE
endif
ifeq ($(MTK_SEC_MODEM_NVRAM_ANTI_CLONE), yes)
ccflags-y  += -DMTK_SEC_MODEM_NVRAM_ANTI_CLONE
endif