#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# Import rissu's changes on 21/12/24
TOP := $(srctree)/$(src)
TARGET_BUILD_VARIANT := user
export TOP TARGET_BUILD_VARIANT

# Connectivity combo driver
subdir-ccflags-y += -I$(srctree)/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/clkbuf/src
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/clkbuf_v1/$(MTK_PLATFORM)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ifeq ($(CONFIG_MTK_PMIC_CHIP_MT6359),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6359
endif
ifeq ($(CONFIG_MTK_PMIC_NEW_ARCH),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include
endif
subdir-ccflags-y += -I$(srctree)/drivers/mmc/core
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/
subdir-ccflags-y += -I$(srctree)/drivers/clk/mediatek/
subdir-ccflags-y += -I$(srctree)/drivers/pinctrl/mediatek/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/power_throttling/

# Do Nothing, move to standalone repo
MODULE_NAME := connadp
obj-$(CONFIG_MTK_COMBO) += $(MODULE_NAME).o

$(MODULE_NAME)-objs += common/connectivity_build_in_adapter.o
$(MODULE_NAME)-objs += common/wmt_build_in_adapter.o
$(MODULE_NAME)-objs += power_throttling/adapter.o
$(MODULE_NAME)-objs += power_throttling/core.o
ifeq ($(CONFIG_CONN_PWR_DEBUG),y)
    $(MODULE_NAME)-objs += power_throttling/test.o
endif

ifeq ($(CONFIG_MTK_COMBO), y)
    ccflags-y += -D CFG_CONNADP_BUILD_IN
endif

MTK_PLATFORM_ID := 6877

# rissu: a34x configurations
export MTK_COMBO_CHIP=SOC5_0
export MTK_ANDROID_WMT=y
export MTK_ANDROID_EMI=y
export ADAPTOR_OPTS=$(MTK_COMBO_CHIP)
export BT_PLATFORM=$(MTK_PLATFORM_ID)
export CONFIG_MTK_COMBO_WIFI_HIF=axi
export CONFIG_MTK_WIFI_CONNINFRA_SUPPORT=y
export CONFIG_MTK_WIFI_CONNAC2X=y
export CFG_FM_CHIP=mt6635
export CFG_FM_CHIP_ID=$(MTK_PLATFORM_ID)
export CFG_BUILD_CONNAC2=true
export CONNAC_VER=2_0
export WLAN_CHIP_ID=$(MTK_PLATFORM_ID)
export WIFI_IP_SET=1

obj-y += conninfra/ wlan/adaptor/ wlan/core/gen4m/ bt/mt66xx/btif/ gps/ fmradio/ connfem/