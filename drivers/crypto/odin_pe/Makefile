# Makefile for Odin PE Linux device driver

MODULE_NAME = dx_pe

obj-$(CONFIG_CRYPTO_DEV_ODIN_PE) := $(MODULE_NAME).o
$(MODULE_NAME)-objs := dx_driver.o sep_init.o crypto_ctx_mgr.o sep_sysfs.o desc_mgr.o lli_mgr.o crypto_api.o sep_request_mgr.o sep_power.o ipsec_qs.o ike_evt_agent.o

# Handle SHA support capability
ifneq ($(DX_CONFIG_SHA),)
	SEP_SUPPORT_SHA = $(DX_CONFIG_SHA)
else # default
	SEP_SUPPORT_SHA = 256
endif
$(info SEP_SUPPORT_SHA=$(SEP_SUPPORT_SHA))
EXTRA_CFLAGS += -DMODULE_NAME=\"$(MODULE_NAME)\" -DSEP_SUPPORT_SHA=$(SEP_SUPPORT_SHA) -DDEBUG
ifneq ($(CONFIG_ODIN_SMMU_SES),y)
EXTRA_CFLAGS += -DCONFIG_NOT_COHERENT_CACHE
endif

ifeq ($(DEBUG),1) # Remove optimization flags
define remove_o2
	CFLAGS_REMOVE_$(1) = -O2
endef
$(foreach obj,$(MODULE_NAME)-objs,$(eval $(call remove_o2,$(obj))))
EXTRA_CFLAGS += -DDEBUG -O1
endif

$(info $(if $(DEBUG),DEBUG=$(DEBUG)))
#$(info EXTRA_CFLAGS=$(EXTRA_CFLAGS))
