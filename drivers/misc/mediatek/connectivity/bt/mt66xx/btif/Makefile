###############################################################################
# Bluetooth character device driver

###############################################################################

# ---------------------------------------------------
# Compile Options
# ---------------------------------------------------

# Force build fail on modpost warning
KBUILD_MODPOST_FAIL_ON_WARNINGS := y

# Rissu: Add LOG_TAG
LOG_TAG="[BT]"

$(info $(LOG_TAG) TARGET_BUILD_VARIANT = $(TARGET_BUILD_VARIANT))
ifeq ("$(TARGET_BUILD_VARIANT)","user")
    ccflags-y += -D FW_LOG_DEFAULT_ON=0
    ccflags-y += -D BUILD_QA_DBG=0
else
    ccflags-y += -D FW_LOG_DEFAULT_ON=1
    ccflags-y += -D BUILD_QA_DBG=1
endif


ccflags-y += -D CONNAC20_CHIPID=$(BT_PLATFORM)
$(info $(LOG_TAG) BT_PLATFORM = $(BT_PLATFORM))
$(info $(LOG_TAG) srctree = $(srctree))

# ---------------------------------------------------
# Compile Options: set feature flag (1: enable, 0: disable)
# ---------------------------------------------------

# build btif interface
ccflags-y += -D CHIP_IF_BTIF

# Use device node or hci_dev as native interface
ccflags-y += -D USE_DEVICE_NODE=1

# Customized fw update feature
ccflags-y += -D CUSTOMER_FW_UPDATE=0

# pm_qos control
ccflags-y += -D PM_QOS_CONTROL=0

# No function, only for build pass
ccflags-y += -D CONFIG_MP_WAKEUP_SOURCE_SYSFS_STAT=1

# Customized feature, load 1b fw bin
#ccflags-y += -D BT_CUS_FEATURE

# ---------------------------------------------------
# Include Path
# ---------------------------------------------------
CONN_INFRA_SRC := $(TOP)/conninfra
CONNFEM_SRC := $(TOP)/connfem
WMT_SRC := $(TOP)/common
BTIF_SRC := $(srctree)/drivers/misc/mediatek/btif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/connectivity/common
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/connectivity/power_throttling
ccflags-y += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(CONN_INFRA_SRC)/include
ccflags-y += -I$(CONN_INFRA_SRC)/debug_utility/include
ccflags-y += -I$(CONN_INFRA_SRC)/debug_utility/metlog
ccflags-y += -I$(CONN_INFRA_SRC)/debug_utility/
ccflags-y += -I$(CONNFEM_SRC)/include
ccflags-y += -I$(WMT_SRC)/debug_utility
ccflags-y += -I$(BTIF_SRC)/common/inc

ccflags-y += -I$(src)/core/include
ccflags-y += -I$(src)/connsys/connac_2_0
ccflags-y += -I$(src)/../include
ccflags-y += -I$(src)/../include/btif

# ---------------------------------------------------
# Objects List
# ---------------------------------------------------
MODULE_NAME := bt_drv_$(BT_PLATFORM)
obj-y += $(MODULE_NAME).o

CORE_OBJS := btmtk_dbg.o btmtk_irq.o btmtk_char_dev.o ../btmtk_fw_log.o ../btmtk_main.o
CHIP_OBJS := btmtk_mt66xx.o
HIF_OBJS := btmtk_btif_main.o btmtk_queue.o

$(MODULE_NAME)-objs  += $(CORE_OBJS)
$(MODULE_NAME)-objs  += $(HIF_OBJS)
$(MODULE_NAME)-objs  += $(CHIP_OBJS)