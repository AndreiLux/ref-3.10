# common driver code for CMDQ

# drivers/cmdq
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/cmdq \
             -I$(MTK_PATH_PLATFORM)/core/include \
             -I$(MTK_PATH_PLATFORM)/drivers/dispsys 
# Enable MET
ccflags-y += -DMET_USER_EVENT_SUPPORT


obj-y += cmdq_record.o
obj-y += cmdq_core.o
obj-y += cmdq_driver.o
obj-y += cmdq_mmp.o
obj-y += cmdq_mdp_common.o

obj-y += cmdq_test.o
