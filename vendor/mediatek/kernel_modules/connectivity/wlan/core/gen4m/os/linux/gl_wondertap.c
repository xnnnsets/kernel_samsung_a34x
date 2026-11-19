/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 MediaTek Inc.
 */

/*
 *    \brief  Main routines of Linux driver interface for Wi-Fi Wondertap
 *
 *    This file contains the main routines of Linux driver for MediaTek Inc.
 *    802.11 Wireless LAN Adapters.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

#include <linux/poll.h>
#include <linux/kmod.h>
#include <linux/component.h>
#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <net/ieee80211_radiotap.h>
#ifdef WONDERTAP_TODO
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#endif
#include "precomp.h"
#include "debug.h"
#include "gl_os.h"
#include "gl_wext.h"
#include "wlan_lib.h"
#include "gl_cfg80211.h"
#include "radiotap.h"

#if CFG_SUPPORT_WONDERTAP
#define CFG_WONDERTAP_USE_AUX_BUS 1
#if !CFG_SUPPORT_WONDERTAP_UT
#include "wonder/wonder_ven_cmd.h"
#include "wonder/wondertap.h"
#endif
#endif

#if (CFG_SUPPORT_WONDERTAP == 1)

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define	SKIP_ROLE_NONE 0
#define	SKIP_ROLE_ALL 1
#define	SKIP_ROLE_EXCEPT_MAIN 2

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
static uint8_t *ifname = WON_INF_NAME;
static uint8_t *ifname2 = WON_INF_NAME;
static uint16_t mode = RUNNING_WON_MODE;

static struct GLUE_INFO *g_prGlueInfo;
static struct platform_device *g_pdev;
#if CFG_WONDERTAP_USE_AUX_BUS
static struct wondertap_aux_dev *g_prWonAuxDev;
#endif

static uint8_t g_aucWonBSSID[MAC_ADDR_LEN];

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#define PROC_WON_MAX_BUF_SIZE        3000
#define PROC_WON_UID_SHELL 2000
#define PROC_WON_GID_WIFI 1010
#define PROC_WON_ROOT_NAME "wondertap"
#define PROC_WON_COUNTRY "country"
#define PROC_WON_CAPA "capa"
#define PROC_WON_ADDR "addr"
#define PROC_WON_CHANNEL "channel"
#define PROC_WON_FILTER "filter"
#define PROC_WON_RATE "rate"

#define MAX_WON_COUNTRY_LEN 4

#define WON_DEFAULT_INDEX 0

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
/* Net Device Hooks */
static int wonOpen(struct net_device *prDev);

static int wonStop(struct net_device *prDev);

static struct net_device_stats *wonGetStats(struct net_device *prDev);

static void wonSetMulticastList(struct net_device *prDev);
static void wonSetMulticastListWorkQueue(struct work_struct *work);

static netdev_tx_t wonHardStartXmit(struct sk_buff *prSkb,
		struct net_device *prDev);

static int wonSetMACAddress(struct net_device *prDev, void *addr);

static int wonDoIOCTL(struct net_device *prDev,
		struct ifreq *prIFReq,
		int i4Cmd);

#if KERNEL_VERSION(5, 15, 0) <= CFG80211_VERSION_CODE
static int wonDoPrivIOCTL(struct net_device *prDev, struct ifreq *prIfReq,
		void __user *prData, int i4Cmd);
#endif

static uint8_t wonFsmInit(struct ADAPTER *prAdapter, uint8_t aucMacAddr[]);
static void wonFsmUninit(struct ADAPTER *prAdapter);
static void wonFuncAcquireCh(struct ADAPTER *prAdapter,
	uint8_t ucBssIdx, struct WON_CHNL_REQ_INFO *prChnlReqInfo);
static void wonFuncReleaseCh(struct ADAPTER *prAdapter,
	uint8_t ucBssIdx, struct WON_CHNL_REQ_INFO *prChnlReqInfo);

/* Forward declarations for our static functions */
static int wondertap_init(void **handle,
	const struct wondertap_init_params *params);

static void wondertap_deinit(void *handle,
	const struct wondertap_deinit_params *params);

static int wondertap_get_capability(void *handle,
	struct wondertap_capability *caps);

static int wondertap_set_freq(void *handle,
	const struct wondertap_set_freq_params *params);

static int wondertap_set_filter(void *handle,
	enum wondertap_filter_type type,
	const void *params);

static int wondertap_set_fixed_tx_rate(void *handle,
	const struct wondertap_fixed_tx_rate_params *params);

static int wondertap_set_tx_rate_mask(void *handle,
	const struct wondertap_tx_rate_mask_params *params);

/* The structure that holds our implemented functions */
static struct wondertap_ops ops = {
	.init = wondertap_init,
	.deinit = wondertap_deinit,
	.get_capabilities = wondertap_get_capability,
	.set_freq = wondertap_set_freq,
	.set_filter = wondertap_set_filter,
	.set_fixed_tx_rate = wondertap_set_fixed_tx_rate,
	.set_tx_rate_mask = wondertap_set_tx_rate_mask,
};

#if CFG_SUPPORT_WONDERTAP_UT
/**
 * @brief Private data structure for the WonderTap driver.
 *
 * This structure holds the version information and the operations
 * table for the specific vendor implementation.
 */
struct wondertap_priv {
	/** @brief The version of the WonderTap interface being used. */
	enum wondertap_ver ver;
	/** @brief Pointer to the vendor-specific operations table. */
	const struct wondertap_ops *wonder_ops;
};
#endif /* CFG_SUPPORT_WONDERTAP_UT */

static const struct wondertap_priv dhd_wonder_priv = {
	.ver = WONDER_VERSION_1_4,
	.wonder_ops = &ops,
};


/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
static int vendor_wondertap_bind(
	struct device *dev,
	struct device *wonDev,
	void *data)
{
#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter, g_pdev=%p\n", g_pdev);
#endif

	dev_info(dev, "%s(): Bound to wonDev %s\n", __func__, dev_name(wonDev));
	if (g_pdev)
		platform_set_drvdata(g_pdev, (void *) &dhd_wonder_priv);

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif

	return 0;
}
static void vendor_wondertap_unbind(
	struct device *dev,
	struct device *wonDev,
	void *data)
{
#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	dev_info(dev, "%s(): Unbound\n", __func__);

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif
}

static const struct component_ops vendor_component_ops = {
	.bind = vendor_wondertap_bind,
	.unbind = vendor_wondertap_unbind,
};

/* vendor platform driver probe function */
static int vendor_wonder_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif
	g_pdev = pdev;
	component_add(dev, &vendor_component_ops);

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif

	return 0;
}

static int vendor_wonder_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	component_del(dev, &vendor_component_ops);

	g_pdev = NULL;
#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif

	return 0;
}

static const struct of_device_id vendor_wonder_dt_ids[] = {
{
	.compatible = "android,vendor_wlan-wonder" },
	{ /* sentinel */ }
};

#if (CFG_WONDERTAP_USE_AUX_BUS == 0)
MODULE_DEVICE_TABLE(of, vendor_wonder_dt_ids);

static struct platform_driver vendor_wonder_driver = {
	.probe = vendor_wonder_probe,
	.remove = vendor_wonder_remove,
	.driver = {
		.name = "vendor_wonder_dev",
		.of_match_table = vendor_wonder_dt_ids,
	},
};
#endif

struct proc_dir_entry *prProcWonRoot;

static ssize_t procWonRateRead(
	struct file *filp,
	char __user *buf,
	size_t count,
	loff_t *f_pos)
{
	uint8_t *pucProcBuf = kalMemZAlloc(
		PROC_WON_MAX_BUF_SIZE, VIR_MEM_TYPE);
	uint32_t u4CopySize;
	int32_t i4Ret = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *ad = NULL;

	prGlueInfo = (struct GLUE_INFO *)
		pde_data(file_inode(filp));

	if (*f_pos > 0 ||
		!buf ||
		!pucProcBuf ||
		!prGlueInfo) {
		i4Ret = 0;
		goto freeBuf;
	}
	ad = prGlueInfo->prAdapter;

	kalSnprintf(pucProcBuf,
		PROC_WON_MAX_BUF_SIZE,
		"bss:%u\n"
		"preamble:%u\n"
		"bw:%u\n"
		"gi:%u\n"
		"nss:%u\n"
		"mcs:%u\n",
		0, 0, 0, 0, 0, 0);

	u4CopySize = kalStrLen(pucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(
		buf, pucProcBuf, u4CopySize)) {
		DBGLOG(WON, WARN,
			"copy to user failed\n");
		i4Ret = -EFAULT;
		goto freeBuf;
	}

	*f_pos += u4CopySize;
	i4Ret = u4CopySize;

freeBuf:

	if (pucProcBuf)
		kalMemFree(pucProcBuf,
			VIR_MEM_TYPE,
			PROC_WON_MAX_BUF_SIZE);

	return i4Ret;
}

static void wonCopyFixTxRateParam(struct GLUE_INFO *prGlueInfo,
	const struct wondertap_fixed_tx_rate_params *params)
{
	struct GL_WON_INFO *prWONInfo = NULL;

	prWONInfo = prGlueInfo->prWONInfo[WON_DEFAULT_INDEX];
	prWONInfo->tx_rate.preamble = params->preamble;
	prWONInfo->tx_rate.bw = params->bw;
	prWONInfo->tx_rate.gi = params->gi;
	prWONInfo->tx_rate.nss = params->nss;
	prWONInfo->tx_rate.mcs = params->mcs;
}

static ssize_t procWonRateWrite(
	struct file *file,
	const char __user *buffer,
	size_t count,
	loff_t *data)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint8_t *pucProcBuf = kalMemZAlloc(
		PROC_WON_MAX_BUF_SIZE, VIR_MEM_TYPE);
	uint32_t u4CopySize = PROC_WON_MAX_BUF_SIZE;
	int32_t i4Ret = 0;
	uint8_t bss = 0;
	struct wondertap_fixed_tx_rate_params params;
	uint8_t preamble = 0;
	uint8_t bw = 0;
	uint8_t gi = 0;
	uint8_t nss = 0;
	uint8_t mcs = 0;
	int num = 0;

	prGlueInfo = (struct GLUE_INFO *)
		pde_data(file_inode(file));

	if (buffer == NULL ||
		pucProcBuf == NULL ||
		prGlueInfo == NULL) {
		i4Ret = 0;
		goto freeBuf;
	}

	u4CopySize = (count < u4CopySize)
		? count
		: (u4CopySize - 1);

	if (u4CopySize < 2) {
		DBGLOG(WON, WARN,
			"Invalid len[%u]\n",
			u4CopySize);
		i4Ret = -EFAULT;
		goto freeBuf;
	}
	if (copy_from_user(
		pucProcBuf, buffer, u4CopySize)) {
		DBGLOG(WON, WARN,
			"error of copy from user\n");
		i4Ret = -EFAULT;
		goto freeBuf;
	}
	pucProcBuf[u4CopySize] = '\0';

	num = sscanf(
		pucProcBuf,
		"%u %u %u %u %u %u",
		&bss,
		&preamble,
		&bw,
		&gi,
		&nss,
		&mcs);
	if (num != 6) {
		DBGLOG(WON, ERROR,
			"can not get correct data\n");
		goto freeBuf;
	}
	prAdapter = prGlueInfo->prAdapter;
	params.preamble = preamble;
	params.bw = bw;
	params.gi = gi;
	params.nss = nss;
	params.mcs = mcs;
	wonCopyFixTxRateParam(prGlueInfo, &params);

	wondertapSetFixRateByBss(
		prAdapter,
		TRUE,
		bss);

	i4Ret = u4CopySize;
freeBuf:
	if (pucProcBuf)
		kalMemFree(pucProcBuf,
			VIR_MEM_TYPE,
			PROC_WON_MAX_BUF_SIZE);
	return i4Ret;
}

static ssize_t procWonAddrRead(
	struct file *filp,
	char __user *buf,
	size_t count,
	loff_t *f_pos)
{
	uint8_t *pucProcBuf = kalMemZAlloc(
		PROC_WON_MAX_BUF_SIZE, VIR_MEM_TYPE);
	uint32_t u4CopySize;
	uint8_t ucRoleIdx = 0;
	int32_t i4Ret = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *ad = NULL;

	prGlueInfo = (struct GLUE_INFO *)
		pde_data(file_inode(filp));

	if (*f_pos > 0 ||
		!buf ||
		!pucProcBuf ||
		!prGlueInfo) {
		i4Ret = 0;
		goto freeBuf;
	}
	ad = prGlueInfo->prAdapter;

	if (ad->rWifiVar.aucWonAddress[ucRoleIdx])
		kalSnprintf(pucProcBuf,
			PROC_WON_MAX_BUF_SIZE,
			"Current Addr:" MACSTR "\n",
			ad->rWifiVar.aucWonAddress[ucRoleIdx]);

	u4CopySize = kalStrLen(pucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(
		buf, pucProcBuf, u4CopySize)) {
		DBGLOG(WON, WARN,
			"copy to user failed\n");
		i4Ret = -EFAULT;
		goto freeBuf;
	}

	*f_pos += u4CopySize;
	i4Ret = u4CopySize;

freeBuf:

	if (pucProcBuf)
		kalMemFree(pucProcBuf,
			VIR_MEM_TYPE,
			PROC_WON_MAX_BUF_SIZE);

	return i4Ret;
}

static ssize_t procWonCapaRead(
	struct file *filp,
	char __user *buf,
	size_t count,
	loff_t *f_pos)
{
	uint8_t *pucProcBuf = kalMemZAlloc(
		PROC_WON_MAX_BUF_SIZE, VIR_MEM_TYPE);
	uint32_t u4CopySize;
	int32_t i4Ret = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *ad = NULL;

	prGlueInfo = (struct GLUE_INFO *)
		pde_data(file_inode(filp));

	if (*f_pos > 0 ||
		!buf ||
		!pucProcBuf ||
		!prGlueInfo) {
		i4Ret = 0;
		goto freeBuf;
	}
	ad = prGlueInfo->prAdapter;

	kalSnprintf(pucProcBuf,
		PROC_WON_MAX_BUF_SIZE,
		"RateAdapt:%u\n"
		"Coexist STA:%u,SAP:%u,P2P:%u,NAN:%u,RTT:%u\n"
		"AGG:%u %u\n"
		"Dynamic Freq/Rate:%u %u\n"
		"Custom Mgmt/Data Retry:%u %u\n"
		"FrameTypeFilter: %u\n",
		CAPS_RATE_ADAPT,
		CAPS_STA_COEXIST,
		CAPS_SAP_COEXIST,
		CAPS_P2P_COEXIST,
		CAPS_NAN_COEXIST,
		CAPS_RANGING_COEXIST,
		CAPS_AMSDU_AGG,
		CAPS_AMPDU_AGG,
		CAPS_DYNAMIC_FREQ,
		CAPS_DYNAMIC_RATE,
		CAPS_CUSTOM_MGMT_RETRY,
		CAPS_CUSTOM_DATA_RETRY,
		CAPS_FRAME_FILTER);

	u4CopySize = kalStrLen(pucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(
		buf, pucProcBuf, u4CopySize)) {
		DBGLOG(WON, WARN,
			"copy to user failed\n");
		i4Ret = -EFAULT;
		goto freeBuf;
	}

	*f_pos += u4CopySize;
	i4Ret = u4CopySize;

freeBuf:

	if (pucProcBuf)
		kalMemFree(pucProcBuf,
			VIR_MEM_TYPE,
			PROC_WON_MAX_BUF_SIZE);

	return i4Ret;
}

static ssize_t procWonCountryRead(
	struct file *filp,
	char __user *buf,
	size_t count,
	loff_t *f_pos)
{
	uint8_t *pucProcBuf = kalMemZAlloc(
		PROC_WON_MAX_BUF_SIZE, VIR_MEM_TYPE);
	uint32_t u4CopySize;
	uint32_t country = 0;
	char acCountryStr[MAX_WON_COUNTRY_LEN + 1] = {0};
	int32_t i4Ret = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;

	prGlueInfo = (struct GLUE_INFO *)
		pde_data(file_inode(filp));

	if (*f_pos > 0 ||
		!buf ||
		!pucProcBuf ||
		!prGlueInfo) {
		i4Ret = 0;
		goto freeBuf;
	}
	prAdapter = prGlueInfo->prAdapter;

	country = prAdapter->rWifiVar.u2CountryCode;
	/* rlmDomainU32ToAlpha(country, acCountryStr); */
	acCountryStr[0] = (country & 0xff00) >> 8;
	acCountryStr[1] = (country & 0x00ff);

	if (country)
		kalSnprintf(pucProcBuf,
			PROC_WON_MAX_BUF_SIZE,
			"Current Country Code: %s\n",
			acCountryStr);
	else
		kalSnprintf(pucProcBuf,
			PROC_WON_MAX_BUF_SIZE,
			"Current Country Code: NULL\n");

	u4CopySize = kalStrLen(pucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(
		buf, pucProcBuf, u4CopySize)) {
		DBGLOG(WON, WARN,
			"copy to user failed\n");
		i4Ret = -EFAULT;
		goto freeBuf;
	}

	*f_pos += u4CopySize;
	i4Ret = u4CopySize;

freeBuf:

	if (pucProcBuf)
		kalMemFree(pucProcBuf,
			VIR_MEM_TYPE,
			PROC_WON_MAX_BUF_SIZE);

	return i4Ret;
}

static ssize_t procWonCountryWrite(
	struct file *file,
	const char __user *buffer,
	size_t count,
	loff_t *data)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint8_t *pucProcBuf = kalMemZAlloc(
		PROC_WON_MAX_BUF_SIZE, VIR_MEM_TYPE);
	uint32_t rStatus;
	uint32_t u4CopySize = PROC_WON_MAX_BUF_SIZE;
	int32_t i4Ret = 0;

	prGlueInfo = (struct GLUE_INFO *)
		pde_data(file_inode(file));

	if (buffer == NULL ||
		pucProcBuf == NULL ||
		prGlueInfo == NULL) {
		i4Ret = 0;
		goto freeBuf;
	}

	u4CopySize = (count < u4CopySize)
		? count
		: (u4CopySize - 1);

	if (u4CopySize < 2) {
		DBGLOG(WON, WARN,
			"Invalid country code len[%u]\n",
			u4CopySize);
		i4Ret = -EFAULT;
		goto freeBuf;
	}
	if (copy_from_user(
		pucProcBuf, buffer, u4CopySize)) {
		DBGLOG(WON, WARN,
			"error of copy from user\n");
		i4Ret = -EFAULT;
		goto freeBuf;
	}
	pucProcBuf[u4CopySize] = '\0';

	prAdapter = prGlueInfo->prAdapter;

	rStatus = wondertapSetCountryCode(
		prAdapter, pucProcBuf);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(WON, INFO,
			"failed set country code: %s\n",
			pucProcBuf);
		i4Ret = -EINVAL;
		goto freeBuf;
	}

	i4Ret = u4CopySize;
freeBuf:
	if (pucProcBuf)
		kalMemFree(pucProcBuf,
			VIR_MEM_TYPE,
			PROC_WON_MAX_BUF_SIZE);
	return i4Ret;
}

#if KERNEL_VERSION(5, 6, 0) <= CFG80211_VERSION_CODE
static const struct proc_ops country_won_pops = {
	.proc_read = procWonCountryRead,
	.proc_write = procWonCountryWrite,
};
#else
static const struct file_operations country_won_pops = {
	.owner = THIS_MODULE,
	.read = procWonCountryRead,
	.write = procWonCountryWrite,
};
#endif
#if KERNEL_VERSION(5, 6, 0) <= CFG80211_VERSION_CODE
static const struct proc_ops addr_won_pops = {
	.proc_read = procWonAddrRead,
	.proc_write = NULL,
};
#else
static const struct file_operations addr_won_pops = {
	.owner = THIS_MODULE,
	.read = procWonAddrRead,
	.write = procWonAddrWrite,
};
#endif
#if KERNEL_VERSION(5, 6, 0) <= CFG80211_VERSION_CODE
static const struct proc_ops capa_won_pops = {
	.proc_read = procWonCapaRead,
	.proc_write = NULL,
};
#else
static const struct file_operations capa_won_pops = {
	.owner = THIS_MODULE,
	.read = procWonCapaRead,
	.write = NULL,
};
#endif
#if KERNEL_VERSION(5, 6, 0) <= CFG80211_VERSION_CODE
static const struct proc_ops rate_won_pops = {
	.proc_read = procWonRateRead,
	.proc_write = procWonRateWrite,
};
#else
static const struct file_operations rate_won_pops = {
	.owner = THIS_MODULE,
	.read = procWonRateRead,
	.write = procWonRateWrite,
};
#endif
#if KERNEL_VERSION(5, 6, 0) <= CFG80211_VERSION_CODE
static const struct proc_ops filter_won_pops = {
	.proc_read = procWonCapaRead,
	.proc_write = NULL,
};
#else
static const struct file_operations filter_won_pops = {
	.owner = THIS_MODULE,
	.read = NULL,
	.write = NULL,
};
#endif
#if KERNEL_VERSION(5, 6, 0) <= CFG80211_VERSION_CODE
static const struct proc_ops channel_won_pops = {
	.proc_read = NULL,
	.proc_write = NULL,
};
#else
static const struct file_operations channel_won_pops = {
	.owner = THIS_MODULE,
	.read = NULL,
	.write = NULL,
};
#endif

int32_t procRemoveFsWondertap(
	struct proc_dir_entry *prProcRoot)
{
#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	remove_proc_entry(
		PROC_WON_COUNTRY,
		prProcWonRoot);

	remove_proc_entry(
		PROC_WON_ADDR,
		prProcWonRoot);

	remove_proc_entry(
		PROC_WON_CAPA,
		prProcWonRoot);

	remove_proc_entry(
		PROC_WON_CHANNEL,
		prProcWonRoot);

	remove_proc_entry(
		PROC_WON_FILTER,
		prProcWonRoot);

	remove_proc_entry(
		PROC_WON_RATE,
		prProcWonRoot);

	remove_proc_entry(
		PROC_WON_ROOT_NAME,
		prProcRoot);

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif

	return 0;
} /* end of procRemoveProcfs() */

int32_t procCreateFsWondertap(
	struct GLUE_INFO *prGlueInfo,
	struct proc_dir_entry *prProcRoot)
{
#define PROC_CREATE(NAME, MODE, ROOT, OPS) \
	proc_create_data(NAME, MODE, ROOT, OPS, prGlueInfo)
	struct proc_dir_entry *prEntry;

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	prProcWonRoot = proc_mkdir(PROC_WON_ROOT_NAME,
		prProcRoot);
	if (!prProcWonRoot) {
		DBGLOG(WON, ERROR,
			"prProcWonRoot == NULL\n");
		return -ENOENT;
	}
	proc_set_user(prProcWonRoot,
		KUIDT_INIT(PROC_WON_UID_SHELL),
		KGIDT_INIT(PROC_WON_GID_WIFI));

	prEntry = PROC_CREATE(
		PROC_WON_COUNTRY,
		0664,
		prProcWonRoot,
		&country_won_pops);
	if (prEntry == NULL) {
		DBGLOG(WON, ERROR,
			"Unable to create cc entry\n\r");
		return -1;
	}

	prEntry = PROC_CREATE(
		PROC_WON_ADDR,
		0664,
		prProcWonRoot,
		&addr_won_pops);
	if (prEntry == NULL) {
		DBGLOG(WON, ERROR,
			"Unable to create addr entry\n\r");
		return -1;
	}

	prEntry = PROC_CREATE(
		PROC_WON_CAPA,
		0664,
		prProcWonRoot,
		&capa_won_pops);
	if (prEntry == NULL) {
		DBGLOG(WON, ERROR,
			"Unable to create capa entry\n\r");
		return -1;
	}

	prEntry = PROC_CREATE(
		PROC_WON_CHANNEL,
		0664,
		prProcWonRoot,
		&channel_won_pops);
	if (prEntry == NULL) {
		DBGLOG(WON, ERROR,
			"Unable to create channel entry\n\r");
		return -1;
	}

	prEntry = PROC_CREATE(
		PROC_WON_FILTER,
		0664,
		prProcWonRoot,
		&filter_won_pops);
	if (prEntry == NULL) {
		DBGLOG(WON, ERROR,
			"Unable to create filter entry\n\r");
		return -1;
	}

	prEntry = PROC_CREATE(
		PROC_WON_RATE,
		0664,
		prProcWonRoot,
		&rate_won_pops);
	if (prEntry == NULL) {
		DBGLOG(WON, ERROR,
			"Unable to create rate entry\n\r");
		return -1;
	}

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif

	return 0;
}

static void wonCopyChannelParam(struct GLUE_INFO *prGlueInfo,
	const struct wondertap_set_freq_params *params)
{
	struct GL_WON_INFO *prWONInfo = NULL;

	prWONInfo = prGlueInfo->prWONInfo[WON_DEFAULT_INDEX];
	prWONInfo->channel.freq = params->freq;
	prWONInfo->channel.bandwidth = params->bandwidth;
}

uint32_t wondertapSetCountryCode(
	struct ADAPTER *prAdapter,
	const uint8_t *country_code)
{
	uint8_t country[2] = {0};
	uint32_t u4BufLen = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	/* Country Code */
	country[0] = country_code[0];
	country[1] = country_code[1];

	DBGLOG(WON, INFO,
		"Set country code: %c%c\n",
		country[0], country[1]);

	rStatus = kalIoctl(
		prAdapter->prGlueInfo,
		wlanoidSetCountryCode,
		country, 2, &u4BufLen);

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif

	return rStatus;
}

static const char * const apucDebugWonRoleState[WON_ROLE_STATE_NUM] = {
	"WON_ROLE_STATE_IDLE",
	"WON_ROLE_STATE_SCAN",
	"WON_ROLE_STATE_REQING_CHANNEL",
	"WON_ROLE_STATE_CHNL_ON_HAND",
	"WON_ROLE_STATE_OFF_CHNL_TX",
	"WON_ROLE_STATE_LISTEN_OFFLOAD",
	"WON_ROLE_STATE_NORMAL_TR"
};

uint32_t wonRxIndicateOnePkt(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb, uint16_t ucBssIndex)
{
	struct QUE rQue;
	struct QUE *prQue = &rQue;
#if CFG_WONDERTAP_TX_RX_DEBUG
	u_int8_t fgIsBMC = (prSwRfb->fgIsBC | prSwRfb->fgIsMC);
#endif /* CFG_WONDERTAP_TX_RX_DEBUG */

	QUEUE_INITIALIZE(prQue);

#if CFG_WONDERTAP_TX_RX_DEBUG
	DBGLOG(WON, INFO,
		"WON Rx Mgmt [BSSidx,StaRecIdx,widx,tid,SN,fmt,Bmc]:%u,%u,%u,%u,%u,%u,%u\n",
		ucBssIndex,
		prSwRfb->ucStaRecIdx,
		prSwRfb->ucWlanIdx,
		prSwRfb->ucTid,
		prSwRfb->u2SSN,
		prSwRfb->ucPayloadFormat,
		fgIsBMC);
#endif /* CFG_WONDERTAP_TX_RX_DEBUG */

	GLUE_SET_PKT_BSS_IDX(prSwRfb->pvPacket, ucBssIndex);
	prSwRfb->eDst = RX_PKT_DESTINATION_HOST;

	QUEUE_INSERT_TAIL(prQue, prSwRfb);

	/* enqueue all prSwRfb to NAPI */
	nicRxEnqueueRfbMainToNapi(prAdapter, prQue);
	if (kalScheduleNapiTask(prAdapter) == WLAN_STATUS_NOT_ACCEPTED) {
		/* Handle Non Rx-direct call path */
		nicRxIndicateRfbMainToNapi(prAdapter);
	}

	return WLAN_STATUS_SUCCESS;
}

void wonRxProcessActionFrame(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb)
{
	uint8_t ucBssIndex = 0;

	if (prAdapter->fgIsWONRunning) {
		for (; ucBssIndex < MAX_BSSID_NUM; ucBssIndex++) {
			if (IS_BSS_INDEX_WON(prAdapter, ucBssIndex)) {
				wonRxIndicateOnePkt(prAdapter, prSwRfb,
					ucBssIndex);
				return;
			}
		}
	}

	nicRxReturnRFB(prAdapter, prSwRfb);
}

void
scanWonProcessBeaconAndProbeResp(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb,
	uint32_t *prStatus,
	struct BSS_DESC *prBssDesc,
	struct WLAN_BEACON_FRAME *prWlanBeaconFrame)
{
	u_int8_t fgIsBeacon = FALSE;
	u_int8_t fgIsWonNetRegistered = FALSE;

	/* check for won net device state */
	GLUE_SPIN_LOCK_DECLARATION();
	GLUE_ACQUIRE_SPIN_LOCK(prAdapter->prGlueInfo, SPIN_LOCK_NET_DEV);
	if (prAdapter->fgIsWONRunning &&
		prAdapter->fgIsWONRegistered &&
		prAdapter->rWONNetRegState == ENUM_NET_REG_STATE_REGISTERED)
		fgIsWonNetRegistered = TRUE;
	GLUE_RELEASE_SPIN_LOCK(prAdapter->prGlueInfo, SPIN_LOCK_NET_DEV);

	if (!fgIsWonNetRegistered)
		return;

	if (EQUAL_MAC_ADDR(
		prWlanBeaconFrame->aucBSSID,
		g_aucWonBSSID))
		DBGLOG(WON, INFO,
			"indicate [" MACSTR "][%s][%s][ch %d][r %d][t %u]\n",
			MAC2STR(prWlanBeaconFrame->aucBSSID),
			fgIsBeacon ? "Beacon" : "Probe Response",
			prBssDesc->aucSSID,
			prBssDesc->ucChannelNum,
			prBssDesc->ucRCPI,
			prBssDesc->rUpdateTime);
}

const char *wonFsmGetFsmState(enum ENUM_WON_ROLE_STATE eCurrentState)
{
	if ((uint32_t)eCurrentState <
		WON_ROLE_STATE_NUM)
		return apucDebugWonRoleState[(uint32_t)eCurrentState];

	return "UNKNOWN";
}

void
wonStateInit_IDLE(struct ADAPTER *prAdapter,
		struct WON_ROLE_FSM_INFO *prWonRoleFsmInfo,
		struct BSS_INFO *prWoBssInfo)
{
}

void
wonStateAbort_IDLE(struct ADAPTER *prAdapter,
		struct WON_ROLE_FSM_INFO *prWonRoleFsmInfo,
		struct WON_CHNL_REQ_INFO *prWonChnlReqInfo)
{
	/* AP mode channel hold time. */
	if (prWonChnlReqInfo->fgIsChannelRequested)
		wonFuncReleaseCh(prAdapter,
			prWonRoleFsmInfo->ucBssIndex,
			prWonChnlReqInfo);

	DBGLOG(WON, INFO, "stop role idle timer.\n");
	cnmTimerStopTimer(prAdapter,
		&(prWonRoleFsmInfo->rWonFsmTimeoutTimer));
}

void
wonStateInit_REQING_CHANNEL(struct ADAPTER *prAdapter,
		uint8_t ucBssIdx,
		struct WON_ROLE_FSM_INFO *prWonRoleFsmInfo,
		struct WON_CHNL_REQ_INFO *prChnlReqInfo)
{
	do {
		ASSERT_BREAK((prAdapter != NULL) && (prChnlReqInfo != NULL));

		wonFuncAcquireCh(prAdapter, ucBssIdx, prChnlReqInfo);

		wonFsmStateTransition(prAdapter, prWonRoleFsmInfo,
			WON_ROLE_STATE_NORMAL_TR);
	} while (FALSE);
}

void
wonStateAbort_REQING_CHANNEL(struct ADAPTER *prAdapter,
		struct BSS_INFO *prWonBssInfo,
		struct WON_ROLE_FSM_INFO *prWonRoleFsmInfo,
		enum ENUM_WON_ROLE_STATE eNextState)
{
	uint8_t ucRoleIdx;
	uint8_t ucBssIdx;
	struct WON_CHNL_REQ_INFO *prWonChnlReqInfo;

	if (!prAdapter || !prWonBssInfo || !prWonRoleFsmInfo) {
		DBGLOG(WON, ERROR, "Null ptr %p %p %p\n", prAdapter,
			prWonBssInfo, prWonRoleFsmInfo);
		return;
	}

	ucRoleIdx = prWonRoleFsmInfo->ucRoleIndex;
	ucBssIdx = prWonRoleFsmInfo->ucBssIndex;
	prWonChnlReqInfo = &prWonRoleFsmInfo->rChnlReqInfo;

	if (eNextState != WON_ROLE_STATE_NORMAL_TR)
		wonFuncReleaseCh(prAdapter, ucBssIdx, prWonChnlReqInfo);
}

void
wonStateInit_NORMAL_TX(struct ADAPTER *prAdapter,
		struct WON_ROLE_FSM_INFO *prWonRoleFsmInfo,
		struct WON_CHNL_REQ_INFO *prWonChnlReqInfo)
{
#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, ERROR, "fgIsChannelRequested=%u\n",
		prWonChnlReqInfo->fgIsChannelRequested);
#endif

	cnmTimerStartTimer(prAdapter,
		&(prWonRoleFsmInfo->rWonFsmTimeoutTimer),
		2000);
}

void
wonStateAbort_NORMAL_TX(struct ADAPTER *prAdapter,
		struct WON_ROLE_FSM_INFO *prWonRoleFsmInfo)
{
}

void wonFsmStateTransition(struct ADAPTER *prAdapter,
		struct WON_ROLE_FSM_INFO *prWonRoleFsmInfo,
		enum ENUM_WON_ROLE_STATE eNextState)
{
	u_int8_t fgIsTransitionOut = (u_int8_t) FALSE;
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
	struct WON_CHNL_REQ_INFO *prChnlReqInfo =
		(struct WON_CHNL_REQ_INFO *) NULL;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					  prWonRoleFsmInfo->ucBssIndex);
	prChnlReqInfo = &(prWonRoleFsmInfo->rChnlReqInfo);
	if (!prBssInfo || !prChnlReqInfo)
		return;
	do {
		if (!IS_BSS_ACTIVE(prBssInfo)) {
			if ((eNextState == WON_ROLE_STATE_IDLE &&
			    prWonRoleFsmInfo->eCurrentState ==
			    WON_ROLE_STATE_IDLE)) {
				/* Do not activate network ruring DBDC HW
				 * switch. Otherwise, BSS may use incorrect
				 * CR and result in TRx problems.
				 */
				DBGLOG(WON, STATE,
					"[WON_ROLE][%d](Bss%d): Skip activate network [%s]\n",
					prWonRoleFsmInfo->ucRoleIndex,
					prWonRoleFsmInfo->ucBssIndex,
					wonFsmGetFsmState(eNextState));
			} else {
				nicActivateNetwork(prAdapter,
					NETWORK_ID(prBssInfo->ucBssIndex, 0));
			}
		}

		fgIsTransitionOut = fgIsTransitionOut ? FALSE : TRUE;

		if (!fgIsTransitionOut) {
			DBGLOG(WON, INFO,
				"[WON_ROLE][%d]TRANSITION(Bss%d): [%s] -> [%s]\n",
				prWonRoleFsmInfo->ucRoleIndex,
				prWonRoleFsmInfo->ucBssIndex,
				wonFsmGetFsmState
				(prWonRoleFsmInfo->eCurrentState),
				wonFsmGetFsmState(eNextState));

			/* Transition into current state. */
			prWonRoleFsmInfo->eCurrentState = eNextState;
		}

		switch (prWonRoleFsmInfo->eCurrentState) {
		case WON_ROLE_STATE_IDLE:
			if (!fgIsTransitionOut)
				wonStateInit_IDLE(prAdapter,
					prWonRoleFsmInfo,
					prBssInfo);
			else
				wonStateAbort_IDLE(prAdapter,
					prWonRoleFsmInfo,
					&(prWonRoleFsmInfo->rChnlReqInfo));
			break;
		case WON_ROLE_STATE_REQING_CHANNEL:
			if (!fgIsTransitionOut) {
				wonStateInit_REQING_CHANNEL(prAdapter,
					prWonRoleFsmInfo->ucBssIndex,
					prWonRoleFsmInfo,
					&(prWonRoleFsmInfo->rChnlReqInfo));
			} else {
				wonStateAbort_REQING_CHANNEL(prAdapter,
					prBssInfo,
					prWonRoleFsmInfo, eNextState);
			}
			break;
		case WON_ROLE_STATE_NORMAL_TR:
			if (!fgIsTransitionOut) {
				wonStateInit_NORMAL_TX(prAdapter,
					prWonRoleFsmInfo,
					&(prWonRoleFsmInfo->rChnlReqInfo));
			} else {
				wonStateAbort_NORMAL_TX(prAdapter,
					prWonRoleFsmInfo);
			}
			break;
		default:
			ASSERT(FALSE);
			break;
		}
	} while (fgIsTransitionOut);
}

static int wondertap_init(
	void **handle,
	const struct wondertap_init_params *params)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct net_device *prDev = NULL;
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPriv = NULL;
	struct BSS_INFO *prWonBssInfo = (struct BSS_INFO *) NULL;
	uint8_t aucMacAddr[MAC_ADDR_LEN];

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	/* Find prAdapter */
	if (!params || !handle) {
		DBGLOG(REQ, ERROR, "Null params/handle\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prGlueInfo = g_prGlueInfo;
	if (!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHECK_ALL)) {
		DBGLOG(WON, WARN, "driver is not ready\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prAdapter = prGlueInfo->prAdapter;

	prDev = prGlueInfo->prWonDev[WON_DEFAULT_INDEX];
	if (!prDev) {
		DBGLOG(WON, ERROR, "Null prDev\n");
		return WONDERTAP_STATUS_FAIL;
	}
	/* Assign prDev to the location pointed to by handle */
	/* TBC */
	if (handle) {
		DBGLOG(WON, INFO, "Assign prDev as handle\n");
		*handle = (void *)prDev;
	}

	wonCopyChannelParam(prGlueInfo, &params->channel);
	wonCopyFixTxRateParam(prGlueInfo, &params->tx_rate);

	prNetDevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(prDev);
	COPY_MAC_ADDR(aucMacAddr, params->mac_addr);
	if (!IS_BSS_INDEX_WON(prAdapter,
		prNetDevPriv->ucBssIdx)) {
		/* TODO: Input ch and bw ? */
		prNetDevPriv->ucBssIdx =
			wonFsmInit(prAdapter, aucMacAddr);
		wlanBindBssIdxToNetInterface(
			prGlueInfo,
			prNetDevPriv->ucBssIdx,
			(void *) prDev);
		DBGLOG(WON, INFO,
			"[wonmac] bss%u, prDev = %p\n",
			prNetDevPriv->ucBssIdx,
			prDev);
	}
	prWonBssInfo = GET_BSS_INFO_BY_INDEX(
		prAdapter, prNetDevPriv->ucBssIdx);
	if (!prWonBssInfo) {
		DBGLOG(WON, ERROR, "bss is not won\n");
		return WONDERTAP_STATUS_FAIL;
	}

	prAdapter->fgIsWONRunning = TRUE;

	/* [v1.4] Assign at once */
	DBGLOG(WON, INFO, "wonmac_addr:" MACSTR "\n",
		MAC2STR(params->mac_addr));

	DBGLOG(WON, INFO, "bssid:" MACSTR "\n",
		MAC2STR(params->bssid));

	DBGLOG(WON, INFO, "freq: %u\n",
		params->channel.freq);
	DBGLOG(WON, INFO, "bw: %u\n",
		params->channel.bandwidth);

	/* Update bssid */
	COPY_MAC_ADDR(
		prWonBssInfo->aucBSSID,
		params->bssid);
	COPY_MAC_ADDR(
		g_aucWonBSSID,
		params->bssid);
	/* Update netdev addr */
#if (KERNEL_VERSION(5, 16, 0) <= CFG80211_VERSION_CODE)
	eth_hw_addr_set(prDev, aucMacAddr);
#else
	kalMemCopy(prDev->dev_addr, aucMacAddr, ETH_ALEN);
#endif
	kalMemCopy(prDev->perm_addr, prDev->dev_addr, ETH_ALEN);
	COPY_MAC_ADDR(prWonBssInfo->aucOwnMacAddr,
		aucMacAddr);
	COPY_MAC_ADDR(
		prAdapter->rWifiVar.aucWonAddress[WON_DEFAULT_INDEX],
		aucMacAddr);

	prDev->type = ARPHRD_IEEE80211_RADIOTAP;

	netif_carrier_on(prDev);
#if KERNEL_VERSION(5, 0, 0) <= CFG80211_VERSION_CODE
	dev_change_flags(prDev,
		prDev->flags | IFF_UP,
		NULL);
#else
	dev_change_flags(prDev,
		prDev->flags | IFF_UP);
#endif

	/* Fix Rate, using params cached by wonCopyFixTxRateParam */
	wondertapSetFixRateByBss(prAdapter,
		TRUE,
		prNetDevPriv->ucBssIdx);

	/* Country Code */
	wondertapSetCountryCode(
		prAdapter,
		params->country_code);

	/* TODO: TXD will read wifi.cfg */
	DBGLOG(WON, INFO, "mgmt_retry_limit: %d\n",
		params->mgmt_retry_limit);
	DBGLOG(WON, INFO, "data_retry_limit: %d\n",
		params->data_retry_limit);
	DBGLOG(WON, INFO, "amsdu_enable: %d\n",
		params->amsdu_enable);
	DBGLOG(WON, INFO, "ampdu_enable: %d\n",
		params->ampdu_enable);


#if CFG_ENABLE_WAKE_LOCK
	KAL_WAKE_LOCK(prAdapter, prGlueInfo->prWonWakeLock);
#endif
#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif
	return WONDERTAP_STATUS_OK;
}

static void wondertap_deinit(
	void *handle,
	const struct wondertap_deinit_params *params)
{
	struct net_device *prDev = NULL;
	struct NETDEV_PRIVATE_GLUE_INFO *prDevPrivate = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	/* Find prAdapter */
	if (!params) {
		DBGLOG(WON, ERROR, "Null params\n");
		return;
	}
	prDev = (struct net_device *) handle;
	prDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(prDev);
	if (!prDevPrivate) {
		DBGLOG(WON, ERROR, "Null prDevPrivate\n");
		return;
	}
	prGlueInfo = prDevPrivate->prGlueInfo;
	if (!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHECK_ALL)) {
		DBGLOG(WON, WARN, "driver is not ready\n");
		return;
	}
	prAdapter = prGlueInfo->prAdapter;
	prAdapter->fgIsWONRunning = FALSE;
#if CFG_ENABLE_WAKE_LOCK
	if (KAL_WAKE_LOCK_ACTIVE(prAdapter, prGlueInfo->prWonWakeLock))
		KAL_WAKE_UNLOCK(prAdapter, prGlueInfo->prWonWakeLock);
#endif

	netif_carrier_off(prDev);
#if KERNEL_VERSION(5, 0, 0) <= CFG80211_VERSION_CODE
	dev_change_flags(prDev,
		(prDev->flags &= ~IFF_UP),
		NULL);
#else
	dev_change_flags(prDev,
		(prDev->flags &= ~IFF_UP));
#endif

	/* Country Code */
	wondertapSetCountryCode(
		prAdapter,
		params->country_code);

	/* Deinit Implementation */
	wonPeerRemoveAll(
		prAdapter,
		prDevPrivate->ucBssIdx);

	wonFsmUninit(prAdapter);

	/* not init BSS yet */
	prDevPrivate->ucBssIdx = 0xff;

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif
}

static int wondertap_get_capability(
	void *handle,
	struct wondertap_capability *caps)
{
#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO,
		"Enter caps=%p handle=%p\n", caps, handle);
#endif

	if (!caps) {
		DBGLOG(REQ, ERROR, "Null caps\n");
		return WONDERTAP_STATUS_FAIL;
	}

	caps->version = WONDERAP_VERSION;
	/* caps->raw_bits = 0; */
	/* @brief Dynamic rate adaptation is supported. */
	caps->bits.rate_adaptation = CAPS_RATE_ADAPT;
	/* @brief STA (Station) coexistence is supported. */
	caps->bits.sta_coexist = CAPS_STA_COEXIST;
	/* @brief SAP (Soft AP) coexistence is supported. */
	caps->bits.sap_coexist = CAPS_SAP_COEXIST;
	/* @brief P2P (Wi-Fi Direct) coexistence is supported. */
	caps->bits.p2p_coexist = CAPS_P2P_COEXIST;
	/* @brief NAN (Neighbor Awareness Networking) coexistence is
	 *        supported.
	 */
	caps->bits.nan_coexist = CAPS_NAN_COEXIST;
	/* @brief Ranging coexistence is supported. */
	caps->bits.ranging_coexist = CAPS_RANGING_COEXIST;
	/* @brief A-MSDU aggregation is supported. */
	caps->bits.amsdu_aggregation = CAPS_AMSDU_AGG;
	/* @brief A-MPDU aggregation is supported. */
	caps->bits.ampdu_aggregation = CAPS_AMPDU_AGG;
	/* @brief Dynamic frequency/channel changes are supported. */
	caps->bits.dynamic_freq = CAPS_DYNAMIC_FREQ;
	/* @brief Dynamic setting a fixed TX rate is supported. */
	caps->bits.dynamic_fixed_tx_rate = CAPS_DYNAMIC_RATE;
	/* @brief Setting custom management frame retry limits is supported. */
	caps->bits.custom_mgmt_retry_limit = CAPS_CUSTOM_MGMT_RETRY;
	/* @brief Setting custom data frame retry limits is supported. */
	caps->bits.custom_data_retry_limit = CAPS_CUSTOM_DATA_RETRY;
	/* @brief Frame type filtering is supported. */
	caps->bits.frame_type_filter = CAPS_FRAME_FILTER;

	DBGLOG(WON, INFO,
		"version: %#010x\n", caps->version);
	DBGLOG(WON, INFO,
		"raw_bits: %#010x\n", caps->raw_bits);

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif

	return WONDERTAP_STATUS_OK;
}

static int wondertap_set_freq(
	void *handle,
	const struct wondertap_set_freq_params *params)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prDevPrivate = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	/* Find prAdapter */
	if (!params || !handle) {
		DBGLOG(WON, ERROR, "Null params\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(handle);
	if (!prDevPrivate) {
		DBGLOG(WON, ERROR, "Null prDevPrivate\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prGlueInfo = prDevPrivate->prGlueInfo;
	if (!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHECK_ALL)) {
		DBGLOG(WON, WARN, "driver is not ready\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(WON, INFO, "freq: %u\n", params->freq);
	DBGLOG(WON, INFO, "bw: %u\n", params->bandwidth);

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif

	/* TODO: update bss */

	return WONDERTAP_STATUS_OK;
}

static int wondertap_set_filter(
	void *handle,
	enum wondertap_filter_type type,
	const void *params)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prDevPrivate = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct wondertap_frame_filter_params *filterParams;

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	/* Find prAdapter */
	if (!params || !handle) {
		DBGLOG(WON, ERROR, "Null params\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(handle);
	if (!prDevPrivate) {
		DBGLOG(WON, ERROR, "Null prDevPrivate\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prGlueInfo = prDevPrivate->prGlueInfo;
	if (!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHECK_ALL)) {
		DBGLOG(WON, WARN, "driver is not ready\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prAdapter = prGlueInfo->prAdapter;

	filterParams = (struct wondertap_frame_filter_params *) params;

	DBGLOG(WON, INFO, "filter_type: %d\n", type);
	DBGLOG(WON, INFO, "enabled: %d\n",
		filterParams->enabled);
	DBGLOG(WON, INFO, "frame_type: %d\n",
		filterParams->frame_type);
	DBGLOG(WON, INFO, "frame_subtype: %d\n",
		filterParams->frame_subtype);

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif

	return WONDERTAP_STATUS_OK;
}

static uint8_t fixed_rate_mode_translation(
	uint8_t preamble
)
{
	uint8_t ret;
	/* OFDM */
	switch (preamble) {
	case WONDERTAP_RATE_PREAMBLE_LEGACY:
		/* OFDM */
		ret = 1;
		break;
	case WONDERTAP_RATE_PREAMBLE_HT:
		ret = 2;
		break;
	case WONDERTAP_RATE_PREAMBLE_VHT:
		ret = 4;
		break;
	case WONDERTAP_RATE_PREAMBLE_HE:
		ret = 8;
		break;
	case WONDERTAP_RATE_PREAMBLE_EHT:
		ret = 15;
		break;
	/* error handle later */
	default:
		ret = 16;
	}

	return ret;
}

int generic_fls(int mask)
{
	int bit_position = 0;

	if (mask == 0)
		return 0;

	while (mask != 1) {
		mask = (unsigned int)mask >> 1;
		bit_position++;
	}

	return bit_position + 1;
}

/**
 * @brief Find the minimum maximum MCS index across all NSS in a rate mask.
 *
 * This function scans through the MCS bitmaps for each NSS and finds the
 * highest set bit (maximum MCS) in each. It then returns the minimum of
 * these maximum MCS values across all NSS.
 *
 * @param mcs_bitmap Array of MCS bitmaps, one per NSS
 * @param nss_max Maximum number of spatial streams to check
 * @return The minimum of the maximum MCS indices, or -1 if no bits are set
 *
 * Example:
 *   eht_mcs[0] = 0x3fff (MCS 0~13) ? max = 13
 *   eht_mcs[1] = 0x0fff (MCS 0~11) ? max = 11
 *   Returns: 11
 */
static int find_min_max_mcs(
	const u16 *mcs_bitmap,
	int nss_max)
{
	int min_max_mcs = -1;
	int nss;

	for (nss = 0; nss < nss_max; nss++) {
		int max_mcs;

		if (mcs_bitmap[nss] == 0)
			continue;

		/* Find the highest set bit
		 * (maximum MCS index for this NSS)
		 */
		/* bitops.h */
		max_mcs = generic_fls(
			mcs_bitmap[nss]) - 1;

		/* Update the minimum of
		 * maximum MCS indices
		 */
		if (min_max_mcs < 0 ||
			max_mcs < min_max_mcs)
			min_max_mcs = max_mcs;
	}

	return min_max_mcs;
}

static int wonder_get_max_mcs_idx(
	struct wondertap_tx_rate_mask_params *params,
	struct BSS_INFO *prBssInfo)
{
	uint8_t ret = 0;

	if (RLM_NET_IS_11BE(prBssInfo)) {
		ret = find_min_max_mcs(params->eht_mcs,
			WONDERTAP_EHT_NSS_MAX);
	} else if (RLM_NET_IS_11AX(prBssInfo)) {
		ret = find_min_max_mcs(params->he_mcs,
			WONDERTAP_HE_NSS_MAX);
	} else if (RLM_NET_IS_11AC(prBssInfo)) {
		ret = find_min_max_mcs(params->vht_mcs,
			WONDERTAP_VHT_NSS_MAX);
	} else if (RLM_NET_IS_11N(prBssInfo)) {
		ret = find_min_max_mcs(params->ht_mcs,
			WONDERTAP_HT_NSS_MAX);
	} else if (RLM_NET_IS_11ABG(prBssInfo)) {
		if (params->legacy_rates == 0)
			ret = -1;
		else
			ret = generic_fls(
				params->legacy_rates) - 1;
	}

	return ret;

}

uint32_t wondertapSetFixRate(
	struct ADAPTER *prAdapter,
	u_int8_t fgIsOid,
	uint16_t u2WlanIdx,
	const struct wondertap_fixed_tx_rate_params *tx_rate)
{
#if !CAPS_DYNAMIC_RATE
#define HT_LDPC BIT(0)
#define VHT_LDPC BIT(1)
#define HE_LDPC BIT(2)

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	if (!prAdapter) {
		DBGLOG(WON, ERROR, "Null prAdapter\n");
		return WONDERTAP_STATUS_FAIL;
	}

	DBGLOG(WON, INFO,
	       "preamble: %d, bw: %d, gi:%d, nss:%d, mcs: %d\n",
	       tx_rate->preamble, tx_rate->bw, tx_rate->gi,
	       tx_rate->nss, tx_rate->mcs);

#ifdef CFG_SUPPORT_UNIFIED_COMMAND
{
	struct UNI_CMD_RA_SET_FIXED_RATE_V1 rate = {0};
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u4BufLen = 0;

	rate.u1PhyMode =
		fixed_rate_mode_translation(tx_rate->preamble);
	if (rate.u1PhyMode == 16) {
		DBGLOG(WON, INFO, "Preamble Error\n");
		return WONDERTAP_STATUS_FAIL;
	}
	rate.u2WlanIdx = u2WlanIdx;
	rate.u2HeLtf = 0;
	rate.u2ShortGi = tx_rate->gi;
	rate.u1Stbc = 0;
	rate.u1Bw = tx_rate->bw;
	rate.u1Ecc = HT_LDPC | VHT_LDPC | HE_LDPC;
	rate.u1Mcs = tx_rate->mcs;
	rate.u1Nss = tx_rate->nss;
	rate.u1Spe = 0;
	rate.u1ShortPreamble = 0;
	if (fgIsOid)
		rStatus = kalIoctl(
				   prAdapter->prGlueInfo,
				   wlanoidSetFixRate,
				   &rate,
				   sizeof(rate),
				   &u4BufLen);
	else
		rStatus = wlanSetFixRate(prAdapter, &rate,
					 sizeof(rate), &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS && rStatus != WLAN_STATUS_PENDING) {
		DBGLOG(WON, INFO, "fix rate Error: %u\n", rStatus);
		return WONDERTAP_STATUS_FAIL;
	}
}
#else
	/* TODO: Connac2 Implementation */
#endif

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif
#endif

	return WONDERTAP_STATUS_OK;
}

uint32_t wondertapSetFixRateByBss(
	struct ADAPTER *prAdapter,
	u_int8_t fgIsOid,
	uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint16_t i;
	struct GL_WON_INFO *prWONInfo = NULL;


	if (!prAdapter) {
		DBGLOG(WON, ERROR, "Null prAdapter\n");
		return WONDERTAP_STATUS_FAIL;
	}

	prWONInfo = prAdapter->prGlueInfo->prWONInfo[WON_DEFAULT_INDEX];
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		ucBssIndex);
	if (!prBssInfo || !IS_BSS_WON(prBssInfo)) {
		DBGLOG(WON, ERROR, "bss is not won\n");
		return WONDERTAP_STATUS_FAIL;
	}

	for (i = 0; i < CFG_STA_REC_NUM; i++) {
		prStaRec = (struct STA_RECORD *)
			&prAdapter->arStaRec[i];
		if (prStaRec->fgIsInUse &&
			(prStaRec->ucBssIndex ==
			ucBssIndex)) {
			wondertapSetFixRate(
				prAdapter,
				fgIsOid,
				prStaRec->ucWlanIndex,
				&prWONInfo->tx_rate);
		}
	}

	return WONDERTAP_STATUS_OK;
}

static int wondertap_set_fixed_tx_rate(
	void *handle,
	const struct wondertap_fixed_tx_rate_params *params)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prDevPrivate = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	/* Find prAdapter */
	if (!params || !handle) {
		DBGLOG(WON, ERROR, "Null params\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(handle);
	if (!prDevPrivate) {
		DBGLOG(WON, ERROR, "Null prDevPrivate\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prGlueInfo = prDevPrivate->prGlueInfo;
	if (!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHECK_ALL)) {
		DBGLOG(WON, WARN, "driver is not ready\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prAdapter = prGlueInfo->prAdapter;

	wonCopyFixTxRateParam(prGlueInfo, params);

	wondertapSetFixRateByBss(
		prAdapter,
		TRUE,
		prDevPrivate->ucBssIdx);

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif

	return WONDERTAP_STATUS_OK;
}

#ifdef CFG_SUPPORT_WON_TODO
uint32_t wondertapSetMaxRate(
	struct ADAPTER *prAdapter,
	uint8_t preamble,
	uint8_t bw,
	uint8_t gi,
	uint8_t nss,
	uint8_t mcs)
{
	/* TODO: get the max mcs idx from bitmap */
	u4MaxMcsIdx = wonder_get_max_mcs_idx(params, prBssInfo);
	strLen = kalSnprintf(cmd, sizeof(cmd), "setMaxRate %lu", u4MaxMcsIdx);

	DBGLOG(WON, TRACE, "set_chip to FW %s strlen=%d\n", cmd, strLen);

	rConfig.ucType = CHIP_CONFIG_TYPE_WO_RESPONSE;
	rConfig.u2MsgSize = strLen;
	kalStrnCpy(rConfig.aucCmd, cmd, strLen);

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetChipConfig,
		(void *)&rConfig,
		sizeof(struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT), &u4BufLen,
		ucBssIdx);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return WONDERTAP_STATUS_FAIL;

}
#endif

#if CFG_WONDERTAP_USE_AUX_BUS
static void mtk_wonder_release(struct device *dev)
{
	struct wondertap_aux_dev *wonder_adev =
		container_of(dev, struct wondertap_aux_dev, adev.dev);
	kfree(wonder_adev);
}
#endif

static int wondertap_set_tx_rate_mask(
	void *handle,
	const struct wondertap_tx_rate_mask_params *params)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prDevPrivate = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	/* Find prAdapter */
	if (!params || !handle) {
		DBGLOG(WON, ERROR, "Null params\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(handle);
	if (!prDevPrivate) {
		DBGLOG(WON, ERROR, "Null prDevPrivate\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prGlueInfo = prDevPrivate->prGlueInfo;
	if (!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHECK_ALL)) {
		DBGLOG(WON, WARN, "driver is not ready\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(WON, INFO, "mask: %x\n",
		params->enable_mask);
	DBGLOG(WON, INFO, "rate: %d\n",
		params->legacy_rates);

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif

	return WONDERTAP_STATUS_OK;
}

/* Module init/exit for the vendor driver */
static int wondertap_module_init(void)
{
	int ret = 0;

#if CFG_WONDERTAP_USE_AUX_BUS
	g_prWonAuxDev = kzalloc(sizeof(*g_prWonAuxDev), GFP_KERNEL);
	if (!g_prWonAuxDev)
		return -ENOMEM;

	g_prWonAuxDev->adev.name = "wondertap";
	g_prWonAuxDev->adev.id = 0;
	g_prWonAuxDev->adev.dev.release = mtk_wonder_release;
	g_prWonAuxDev->adev.dev.parent = g_prGlueInfo->prDev;
	g_prWonAuxDev->ver = WONDER_VERSION_1_5;
	g_prWonAuxDev->wonder_ops = &ops;

	ret = auxiliary_device_init(&g_prWonAuxDev->adev);
	if (ret) {
		kfree(g_prWonAuxDev);
		return ret;
	}

	ret = auxiliary_device_add(&g_prWonAuxDev->adev);
	if (ret) {
		auxiliary_device_uninit(&g_prWonAuxDev->adev);
		return ret;
	}
#else
	/* [v1.4] Change to user component_add drv data */
#ifdef WONDERTAP_V1_2
	ret = wondertap_register_ops(&ops);
#else
	ret = platform_driver_register(&vendor_wonder_driver);
#endif
#endif

	DBGLOG(WON, INFO, "ret=%d\n", ret);

	return ret;
}

static void wondertap_module_exit(void)
{
#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

#if CFG_WONDERTAP_USE_AUX_BUS
		if (g_prWonAuxDev) {
			auxiliary_device_delete(&g_prWonAuxDev->adev);
			auxiliary_device_uninit(&g_prWonAuxDev->adev);
			g_prWonAuxDev = NULL;
		}
#else
	/* [v1.4] Change to user component_del drv data */
#ifdef WONDERTAP_V1_2
	wondertap_unregister_ops(&ops);
#else
	platform_driver_unregister(&vendor_wonder_driver);
#endif
#endif

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif

}

static void wonFuncReleaseCh(struct ADAPTER *prAdapter,
		uint8_t ucBssIdx,
		struct WON_CHNL_REQ_INFO *prChnlReqInfo)
{
	struct MSG_CH_ABORT *prMsgChRelease = (struct MSG_CH_ABORT *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prChnlReqInfo != NULL));

		if (!prChnlReqInfo->fgIsChannelRequested)
			break;

		prChnlReqInfo->fgIsChannelRequested = FALSE;

		/* 1. return channel privilege to CNM immediately */
		prMsgChRelease = (struct MSG_CH_ABORT *)
			cnmMemAlloc(prAdapter,
			RAM_TYPE_MSG, sizeof(struct MSG_CH_ABORT));
		if (!prMsgChRelease) {
			DBGLOG(WON, ERROR, "Alloc msg (%zu) failed\n",
				sizeof(struct MSG_CH_ABORT));
			break;
		}
		prMsgChRelease->rMsgHdr.eMsgId = MID_MNY_CNM_CH_ABORT;
		prMsgChRelease->ucBssIndex = ucBssIdx;
		prMsgChRelease->ucTokenID = prChnlReqInfo->ucSeqNumOfChReq++;
		prMsgChRelease->ucExtraChReqNum = prChnlReqInfo->ucChReqNum - 1;
#if CFG_SUPPORT_DBDC
		if (prMsgChRelease->ucExtraChReqNum >= 1)
			prMsgChRelease->eDBDCBand = ENUM_BAND_ALL;
		else
			prMsgChRelease->eDBDCBand = ENUM_BAND_AUTO;

		DBGLOG(WON, INFO,
			"WON abort channel on band %u. ucExtraChReqNum: %d\n",
			prMsgChRelease->eDBDCBand,
			prMsgChRelease->ucExtraChReqNum);
#endif /*CFG_SUPPORT_DBDC*/
		mboxSendMsg(prAdapter,
			MBOX_ID_0,
			(struct MSG_HDR *) prMsgChRelease,
			MSG_SEND_METHOD_UNBUF);

	} while (FALSE);
}

static void wonFuncAcquireCh(struct ADAPTER *prAdapter,
	uint8_t ucBssIdx, struct WON_CHNL_REQ_INFO *prChnlReqInfo)
{
	struct MSG_CH_REQ *prMsgChReq = (struct MSG_CH_REQ *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prChnlReqInfo != NULL));

		wonFuncReleaseCh(prAdapter, ucBssIdx, prChnlReqInfo);

		/* send message to CNM for acquiring channel */
		prMsgChReq = (struct MSG_CH_REQ *)
				cnmMemAlloc(prAdapter,
				RAM_TYPE_MSG, sizeof(struct MSG_CH_REQ));

		if (!prMsgChReq) {
			/* Can't indicate CNM for channel acquiring */
			break;
		}

		prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
		prMsgChReq->ucBssIndex = ucBssIdx;
		prMsgChReq->ucTokenID = ++prChnlReqInfo->ucSeqNumOfChReq;
		prMsgChReq->eReqType = prChnlReqInfo->eChnlReqType;
		prMsgChReq->u4MaxInterval = prChnlReqInfo->u4MaxInterval;
		prMsgChReq->ucPrimaryChannel = prChnlReqInfo->ucReqChnlNum;
		prMsgChReq->eRfSco = prChnlReqInfo->eChnlSco;
		prMsgChReq->eRfBand = prChnlReqInfo->eBand;
		prMsgChReq->eRfChannelWidth = prChnlReqInfo->eChannelWidth;
		prMsgChReq->ucRfCenterFreqSeg1 = prChnlReqInfo->ucCenterFreqS1;
		prMsgChReq->ucRfCenterFreqSeg2 = prChnlReqInfo->ucCenterFreqS2;
#if CFG_SUPPORT_DBDC
		prMsgChReq->eDBDCBand = ENUM_BAND_AUTO;

		DBGLOG(WON, INFO,
		   "WON Request channel on band %u, tokenID: %d, cookie: 0x%llx.\n",
		   prMsgChReq->eDBDCBand,
		   prMsgChReq->ucTokenID,
		   prChnlReqInfo->u8Cookie);

#endif /*CFG_SUPPORT_DBDC*/
		/* Channel request join BSSID. */
		prChnlReqInfo->ucChReqNum = 1;
		prMsgChReq->ucExtraChReqNum = prChnlReqInfo->ucChReqNum - 1;

		mboxSendMsg(prAdapter,
			MBOX_ID_0,
			(struct MSG_HDR *) prMsgChReq,
			MSG_SEND_METHOD_UNBUF);

		prChnlReqInfo->fgIsChannelRequested = TRUE;
	} while (FALSE);
}

void wonFsmRunEventTimeout(struct ADAPTER *prAdapter,
		uintptr_t ulParamPtr)
{
	struct WON_ROLE_FSM_INFO *prWonRoleFsmInfo =
		(struct WON_ROLE_FSM_INFO *) ulParamPtr;
	struct BSS_INFO *prWonBssInfo;
	struct WON_CHNL_REQ_INFO *prWonChnlReqInfo =
		(struct WON_CHNL_REQ_INFO *) NULL;
	uint8_t ucBssIndex;

	if (!prAdapter || !prWonRoleFsmInfo) {
		DBGLOG(WON, ERROR, "prAdapter=%u, prWonRoleFsmInfo=%u\n",
		       !!prAdapter, !!prWonRoleFsmInfo);
		return;
	}

	switch (prWonRoleFsmInfo->eCurrentState) {
	case WON_ROLE_STATE_IDLE:
	case WON_ROLE_STATE_NORMAL_TR:
		prWonChnlReqInfo = &(prWonRoleFsmInfo->rChnlReqInfo);
		ucBssIndex = prWonRoleFsmInfo->ucBssIndex;
		prWonBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
		if (!prWonBssInfo)
			break;

		if (prWonChnlReqInfo->fgIsChannelRequested)
			wonFuncReleaseCh(prAdapter, ucBssIndex,
				prWonChnlReqInfo);

		break;
	default:
		DBGLOG(WON, ERROR,
		       "Current WON State %d is unexpected for FSM timeout event.\n",
		       prWonRoleFsmInfo->eCurrentState);
		ASSERT(FALSE);
		break;
	}
}

static void wonStartBssReqCh(struct ADAPTER *prAdapter,
	struct WON_ROLE_FSM_INFO *prWonRoleFsmInfo,
	struct BSS_INFO *prWonBssInfo)
{
	struct WON_CHNL_REQ_INFO *prChnlReqInfo =
		&prWonRoleFsmInfo->rChnlReqInfo;

	prChnlReqInfo->eChnlReqType = CH_REQ_TYPE_GO_START_BSS;
	prChnlReqInfo->u4MaxInterval = 500; /* ms */
	prChnlReqInfo->ucReqChnlNum = prWonBssInfo->ucPrimaryChannel;
	prChnlReqInfo->eChnlSco = prWonBssInfo->eBssSCO;
	prChnlReqInfo->eBand = prWonBssInfo->eBand;
	prChnlReqInfo->eChannelWidth = prWonBssInfo->ucVhtChannelWidth;
	prChnlReqInfo->ucCenterFreqS1 = prWonBssInfo->ucVhtChannelFrequencyS1;
	prChnlReqInfo->ucCenterFreqS2 = prWonBssInfo->ucVhtChannelFrequencyS2;
	prChnlReqInfo->ucChReqNum = 1;

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO,
		"ucReqChnlNum=%u, eChnlSco=%u, eBand=%u, eChannelWidth=%u, ucCenterFreqS1=%u, ucCenterFreqS2=%u\n",
		prChnlReqInfo->ucReqChnlNum,
		prChnlReqInfo->eChnlSco,
		prChnlReqInfo->eBand,
		prChnlReqInfo->eChannelWidth,
		prChnlReqInfo->ucCenterFreqS1,
		prChnlReqInfo->ucCenterFreqS2);
#endif

	wonFsmStateTransition(prAdapter, prWonRoleFsmInfo,
		WON_ROLE_STATE_REQING_CHANNEL);
}


uint32_t wonSetupStaRec(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec)
{
	uint8_t ucPeerBW;
	uint32_t u4PeerNSS;
	struct WIFI_VAR *prWifiVar;
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *)NULL;

	if (prStaRec == NULL)
		return WLAN_STATUS_FAILURE;

	prWifiVar = &prAdapter->rWifiVar;
	prBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];
	if (!prBssInfo || !IS_BSS_WON(prBssInfo)) {
		DBGLOG(WON, ERROR, "bss is not won\n");
		return WONDERTAP_STATUS_FAIL;
	}

	ucPeerBW = 80;
	u4PeerNSS = prBssInfo->ucOpRxNss;

	prStaRec->fgHasBasicPhyType = TRUE;
	prStaRec->ucPhyTypeSet =
		prBssInfo->ucPhyTypeSet |
		PHY_TYPE_SET_802_11A |
		PHY_TYPE_SET_802_11G; /* 0x7A */
	DBGLOG(WON, INFO, "ucPhyTypeSet %x", prStaRec->ucPhyTypeSet);

	prStaRec->ucDesiredPhyTypeSet =
		prStaRec->ucPhyTypeSet &
		prAdapter->rWifiVar.ucAvailablePhyTypeSet;
	DBGLOG(WON, INFO, "ucDesiredPhyTypeSet %u",
		prStaRec->ucDesiredPhyTypeSet);

	prStaRec->u2BSSBasicRateSet = prBssInfo->u2BSSBasicRateSet; /* 1344 */

	prStaRec->ucNonHTBasicPhyType = PHY_TYPE_ERP_INDEX;

	prStaRec->u2DesiredNonHTRateSet =
		prBssInfo->u2OperationalRateSet; /* 16320 */

	nicTxUpdateStaRecDefaultRate(prAdapter, prStaRec);

	/* fill HT Capabilities */
	prStaRec->ucMcsSet = 0xFF;
	prStaRec->fgSupMcs32 = FALSE;
	prStaRec->aucRxMcsBitmask[0] = 0xFF;
	prStaRec->aucRxMcsBitmask[1] = 0xFF;
	prStaRec->u2RxHighestSupportedRate = 0;

	prStaRec->u4TxRateInfo = 1;
	prStaRec->u2HtCapInfo = 2493;
	/* Set LDPC Tx capability */
	if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxLdpc))
		prStaRec->u2HtCapInfo |= HT_CAP_INFO_LDPC_CAP;
	else if (IS_FEATURE_DISABLED(prWifiVar->ucTxLdpc))
		prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_LDPC_CAP;
	/* Set STBC Tx capability */
	if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxStbc))
		prStaRec->u2HtCapInfo |= HT_CAP_INFO_RX_STBC;
	else if (IS_FEATURE_DISABLED(prWifiVar->ucTxStbc))
		prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_RX_STBC;
	/* Set Short GI Tx capability */
	if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxShortGI)) {
		prStaRec->u2HtCapInfo |= HT_CAP_INFO_SHORT_GI_20M;
		prStaRec->u2HtCapInfo |= HT_CAP_INFO_SHORT_GI_40M;
	} else if (IS_FEATURE_DISABLED(prWifiVar->ucTxShortGI)) {
		prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SHORT_GI_20M;
		prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_SHORT_GI_40M;
	}
	/* Set HT Greenfield Tx capability */
	if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxGf))
		prStaRec->u2HtCapInfo |= HT_CAP_INFO_HT_GF;
	else if (IS_FEATURE_DISABLED(prWifiVar->ucTxGf))
		prStaRec->u2HtCapInfo &= ~HT_CAP_INFO_HT_GF;

	prStaRec->ucAmpduParam = AMPDU_PARAM_DEFAULT_VAL; /* 15 */
	prStaRec->u2HtExtendedCap =
		(HT_EXT_CAP_DEFAULT_VAL &
		 (~(HT_EXT_CAP_PCO | HT_EXT_CAP_PCO_TRANS_TIME_NONE))); /* 0 */
	prStaRec->u4TxBeamformingCap = TX_BEAMFORMING_CAP_DEFAULT_VAL; /* 0 */
	prStaRec->ucAselCap = ASEL_CAP_DEFAULT_VAL; /* 0 */

#if CFG_SUPPORT_802_11AC
	/* fill VHT Capabilities */
	if ((prBssInfo->ucPhyTypeSet & PHY_TYPE_BIT_VHT)) {
		uint8_t ucVhtCapMcsOwnNotSupportOffset = 0;

		DBGLOG(WON, INFO, "peer supports VHT\n");

		prStaRec->u4VhtCapInfo = 58720690;
		/* Set Tx LDPC capability */
		if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxLdpc))
			prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_RX_LDPC;
		else if (IS_FEATURE_DISABLED(prWifiVar->ucTxLdpc))
			prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_RX_LDPC;
		/* Set Tx STBC capability */
		if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxStbc))
			prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_RX_STBC_MASK;
		else if (IS_FEATURE_DISABLED(prWifiVar->ucTxStbc))
			prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_RX_STBC_MASK;
		/* Set Tx TXOP PS capability */
		if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxopPsTx))
			prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_VHT_TXOP_PS;
		else if (IS_FEATURE_DISABLED(prWifiVar->ucTxopPsTx))
			prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_VHT_TXOP_PS;
		/* Set Tx Short GI capability */
		if (IS_FEATURE_FORCE_ENABLED(prWifiVar->ucTxShortGI)) {
			prStaRec->u4VhtCapInfo |= VHT_CAP_INFO_SHORT_GI_80;
			prStaRec->u4VhtCapInfo |=
				VHT_CAP_INFO_SHORT_GI_160_80P80;
		} else if (IS_FEATURE_DISABLED(prWifiVar->ucTxShortGI)) {
			prStaRec->u4VhtCapInfo &= ~VHT_CAP_INFO_SHORT_GI_80;
			prStaRec->u4VhtCapInfo &=
				~VHT_CAP_INFO_SHORT_GI_160_80P80;
		}

		/* Set Vht Rx Mcs Map upon peer's capability
		 * and our capability
		 */
		prStaRec->u2VhtRxMcsMap = 65530;
		prStaRec->u2VhtTxMcsMap = 65530;

		if (wlanGetSupportNss(prAdapter, prStaRec->ucBssIndex) < 8) {
			ucVhtCapMcsOwnNotSupportOffset =
				wlanGetSupportNss(prAdapter,
						  prStaRec->ucBssIndex) *
				2;
			/* Mark Rx Mcs Map which we don't support */
			prStaRec->u2VhtRxMcsMap |=
				BITS(ucVhtCapMcsOwnNotSupportOffset, 15);
		}

		prStaRec->u2VhtRxHighestSupportedDataRate = 0;
		prStaRec->u2VhtTxHighestSupportedDataRate = 0;
		prStaRec->ucVhtOpMode = 0;

		switch (ucPeerBW) {
		case 20:
			prStaRec->ucVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_20;
			break;
		case 40:
			prStaRec->ucVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_40;
			break;
		case 80:
			prStaRec->ucVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_80;
			break;
		case 160:
			prStaRec->ucVhtOpMode |=
				VHT_OP_MODE_CHANNEL_WIDTH_160_80P80;
			break;
		default:
			prStaRec->ucVhtOpMode |= VHT_OP_MODE_CHANNEL_WIDTH_80;
			break;
		}

		prStaRec->ucVhtOpMode |=
			((u4PeerNSS - 1) << VHT_OP_MODE_RX_NSS_OFFSET) &
			VHT_OP_MODE_RX_NSS;
	}
#endif

#if (CFG_SUPPORT_WON_11AX == 1)
	/* fill HE Capabilities */
	if ((prBssInfo->ucPhyTypeSet & PHY_TYPE_BIT_HE)) {
		prStaRec->ucHeMacCapInfo[0] = 1;
		prStaRec->ucHeMacCapInfo[1] = 8;
		prStaRec->ucHeMacCapInfo[2] = 0;
		prStaRec->ucHeMacCapInfo[3] = 26;
		prStaRec->ucHeMacCapInfo[4] = 0;
		prStaRec->ucHeMacCapInfo[5] = 0;
		prStaRec->ucHePhyCapInfo[0] = 4;
		prStaRec->ucHePhyCapInfo[1] = 32;
		prStaRec->ucHePhyCapInfo[2] = 204;
		prStaRec->ucHePhyCapInfo[3] = 18;
		prStaRec->ucHePhyCapInfo[4] = 13;
		prStaRec->ucHePhyCapInfo[5] = 0;
		prStaRec->ucHePhyCapInfo[6] = 175;
		prStaRec->ucHePhyCapInfo[7] = 12;
		prStaRec->ucHePhyCapInfo[8] = 17;
		prStaRec->ucHePhyCapInfo[9] = 0;
		prStaRec->ucHePhyCapInfo[10] = 0;
	}
#endif

#if (CFG_SUPPORT_WON_11BE == 1)
	/* fill EHT Capabilities */
	if ((prBssInfo->ucPhyTypeSet & PHY_TYPE_BIT_EHT)) {
		DBGLOG(WON, INFO, "peer supports EHT\n");
		prEhtCap = prNDL->aucIeEhtCap;
		ehtRlmRecCapInfo(prAdapter, prStaRec,
			prEhtCap);
	}
#endif

	return WLAN_STATUS_SUCCESS;
}

uint32_t wonPeerAddImpl(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t *mac)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;

	if (prAdapter == NULL)
		return WONDERTAP_STATUS_FAIL;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		ucBssIndex);
	if (prBssInfo == NULL) {
		DBGLOG(WON, ERROR,
			"prBssInfo %d is NULL!\n",
			ucBssIndex);
		return WONDERTAP_STATUS_FAIL;
	}

	DBGLOG(WON, INFO,
		"bss: %u, mac_addr:" MACSTR "\n",
		ucBssIndex,
		MAC2STR(mac));

	prStaRec = cnmStaRecAlloc(prAdapter,
		STA_TYPE_WON,
		ucBssIndex,
		mac);
	if (prStaRec) {
#if CFG_SUPPORT_WON_HW
		prStaRec->fgIsQoS = TRUE;
#endif
		prStaRec->fgHasBasicPhyType = TRUE;
		prStaRec->ucPhyTypeSet =
			prBssInfo->ucPhyTypeSet;
		prStaRec->ucDesiredPhyTypeSet =
			prStaRec->ucPhyTypeSet &
			prAdapter->rWifiVar.ucAvailablePhyTypeSet;
		prStaRec->u2BSSBasicRateSet =
			prBssInfo->u2BSSBasicRateSet;
		prStaRec->ucNonHTBasicPhyType = PHY_TYPE_ERP_INDEX;
		prStaRec->u2DesiredNonHTRateSet =
			prBssInfo->u2OperationalRateSet;
		nicTxUpdateStaRecDefaultRate(prAdapter, prStaRec);

		prStaRec->u2StatusCode =
			STATUS_CODE_SUCCESSFUL;
		prStaRec->ucJoinFailureCount = 0;
		/* Assume it will update after state 3 */
		/* qmSetStaRecTxAllowed(prAdapter, prStaRec, TRUE); */

		wonSetupStaRec(prAdapter, prStaRec);

		cnmStaRecChangeState(prAdapter, prStaRec,
			STA_STATE_3);

		/* TODO */
		prBssInfo->prStaRecOfAP = prStaRec;
		/* COPY_MAC_ADDR(prBssInfo->aucBSSID, mac); */

		prBssInfo->eConnectionState = MEDIA_STATE_CONNECTED;

		nicUpdateBss(prAdapter, prBssInfo->ucBssIndex);

#if CFG_WONDERTAP_TRACE
		DBGLOG(WON, INFO, "dump won init bss info\n");
		bssDumpBssInfo(prAdapter, prBssInfo->ucBssIndex);
#endif
	}

	return WONDERTAP_STATUS_OK;
}

uint32_t wonPeerAddOid(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen)
{
	struct CMD_PEER_WON_UPDATE *prCmd;
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;

	if ((prAdapter == NULL) ||
		(pvSetBuffer == NULL) ||
		(pu4SetInfoLen == NULL))
		return WONDERTAP_STATUS_FAIL;

	/* init */
	*pu4SetInfoLen =
		sizeof(struct CMD_PEER_WON_UPDATE);
	prCmd =
		(struct CMD_PEER_WON_UPDATE *) pvSetBuffer;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prCmd->ucBssIdx);
	if (prBssInfo == NULL) {
		DBGLOG(WON, ERROR,
			"prBssInfo %d is NULL!\n",
			prCmd->ucBssIdx);
		return WONDERTAP_STATUS_FAIL;
	}

	DBGLOG(WON, INFO,
		"bss: %u, mac_addr:" MACSTR "\n",
		prCmd->ucBssIdx,
		MAC2STR(prCmd->aucPeerMac));

	prStaRec = cnmStaRecAlloc(prAdapter,
		STA_TYPE_WON,
		prCmd->ucBssIdx,
		prCmd->aucPeerMac);
	if (prStaRec) {
		prStaRec->fgHasBasicPhyType = TRUE;
		prStaRec->ucPhyTypeSet =
			prBssInfo->ucPhyTypeSet;
		prStaRec->ucDesiredPhyTypeSet =
			prStaRec->ucPhyTypeSet &
			prAdapter->rWifiVar.ucAvailablePhyTypeSet;
		prStaRec->u2BSSBasicRateSet =
			prBssInfo->u2BSSBasicRateSet;
		prStaRec->ucNonHTBasicPhyType = PHY_TYPE_ERP_INDEX;
		prStaRec->u2DesiredNonHTRateSet =
			prBssInfo->u2OperationalRateSet;
		nicTxUpdateStaRecDefaultRate(prAdapter, prStaRec);

		cnmStaRecChangeState(prAdapter, prStaRec,
			STA_STATE_3);
		prStaRec->u2StatusCode =
			STATUS_CODE_SUCCESSFUL;
		prStaRec->ucJoinFailureCount = 0;
		/* Assume it will update after state 3 */
		qmSetStaRecTxAllowed(prAdapter,
			prStaRec, TRUE);

		wonSetupStaRec(prAdapter, prStaRec);

		/* TODO */
		prBssInfo->prStaRecOfAP = prStaRec;
		/* COPY_MAC_ADDR(prBssInfo->aucBSSID, mac); */

		prBssInfo->eConnectionState = MEDIA_STATE_CONNECTED;

		nicUpdateBss(prAdapter, prBssInfo->ucBssIndex);

#if CFG_WONDERTAP_TRACE
		DBGLOG(WON, INFO, "dump won init bss info\n");
		bssDumpBssInfo(prAdapter, prBssInfo->ucBssIndex);
#endif
	}

	return WONDERTAP_STATUS_OK;
}

uint32_t wonPeerAdd(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t *mac,
	uint8_t fgIsOid)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct STA_RECORD *prStaRec = NULL;
	struct CMD_PEER_WON_UPDATE rCmd;

	uint32_t rStatus = 0;
	uint32_t u4BufLen = 0;

	if (!prAdapter) {
		DBGLOG(WON, ERROR, "Null prAdapter\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prGlueInfo = prAdapter->prGlueInfo;
	if (!prGlueInfo) {
		DBGLOG(WON, ERROR, "Null prGlueInfo\n");
		return WONDERTAP_STATUS_FAIL;
	}

	prStaRec = cnmGetStaRecByAddress(
		prAdapter,
		(uint8_t) ucBssIndex,
		mac);
	if (prStaRec) {
		DBGLOG(WON, TRACE,
			"Exist sta%u wlan:%u bss:%u, mac:"
			MACSTR "\n",
			prStaRec->ucIndex,
			prStaRec->ucWlanIndex,
			ucBssIndex,
			MAC2STR(mac));

		return WONDERTAP_STATUS_OK;
	}

	kalMemZero(&rCmd, sizeof(rCmd));
	/* create a peer record */
	COPY_MAC_ADDR(rCmd.aucPeerMac, mac);
	rCmd.eStaType = STA_TYPE_DLS_PEER;
	rCmd.ucBssIdx = ucBssIndex;

	DBGLOG(WON, INFO,
		"bss: %u, mac:" MACSTR "\n",
		ucBssIndex,
		MAC2STR(mac));

	if (!fgIsOid)
		rStatus = wonPeerAddImpl(
			prAdapter,
			ucBssIndex,
			mac);
	else
		rStatus = kalIoctl(prGlueInfo,
				   wonPeerAddOid,
				   &rCmd,
				   sizeof(struct CMD_PEER_WON_UPDATE),
				   &u4BufLen);
	/* set fix rate */
	prStaRec = cnmGetStaRecByAddress(
		prAdapter,
		(uint8_t) ucBssIndex,
		mac);
	if (prStaRec) {
		struct GL_WON_INFO *prWONInfo =
			prAdapter->prGlueInfo->prWONInfo[WON_DEFAULT_INDEX];
		wondertapSetFixRate(prAdapter, FALSE, prStaRec->ucWlanIndex,
				    &prWONInfo->tx_rate);
	}

	return rStatus;
}

uint32_t wonPeerRemoveAllOid(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen)
{
	struct CMD_PEER_WON_UPDATE *prCmd;
	struct BSS_INFO *prBssInfo;

	if ((prAdapter == NULL) ||
		(pvSetBuffer == NULL) ||
		(pu4SetInfoLen == NULL))
		return WONDERTAP_STATUS_FAIL;

	/* init */
	*pu4SetInfoLen =
		sizeof(struct CMD_PEER_WON_UPDATE);
	prCmd =
		(struct CMD_PEER_WON_UPDATE *) pvSetBuffer;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prCmd->ucBssIdx);
	if (prBssInfo == NULL) {
		DBGLOG(WON, ERROR,
			"prBssInfo %d is NULL!\n",
			prCmd->ucBssIdx);
		return WONDERTAP_STATUS_FAIL;
	}

	DBGLOG(WON, INFO, "bss: %u\n",
		prCmd->ucBssIdx);

	cnmStaFreeAllStaByNetwork(
		prAdapter,
		prCmd->ucBssIdx,
		STA_REC_EXCLUDE_NONE);

	return WONDERTAP_STATUS_OK;
}

uint32_t wonPeerRemoveAll(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct CMD_PEER_WON_UPDATE rCmd;
	uint32_t rStatus = 0;
	uint32_t u4BufLen = 0;

	if (!prAdapter) {
		DBGLOG(WON, ERROR, "Null prAdapter\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prGlueInfo = prAdapter->prGlueInfo;
	if (!prGlueInfo) {
		DBGLOG(WON, ERROR, "Null prGlueInfo\n");
		return WONDERTAP_STATUS_FAIL;
	}

	/* init */
	kalMemZero(&rCmd, sizeof(rCmd));

	DBGLOG(WON, INFO, "bss: %u\n",
		ucBssIndex);

	/* create a peer record */
	rCmd.eStaType = STA_TYPE_DLS_PEER;
	rCmd.ucBssIdx = ucBssIndex;

	rStatus = kalIoctl(prGlueInfo,
		wonPeerRemoveAllOid,
		&rCmd,
		sizeof(struct CMD_PEER_WON_UPDATE),
		&u4BufLen);

	return rStatus;
}

static void wonDevFsmInit(struct ADAPTER *prAdapter,
	struct BSS_INFO *prWonBssInfo)
{
	struct WON_DEV_FSM_INFO *prWonDevFsmInfo =
		(struct WON_DEV_FSM_INFO *) NULL;

	prWonDevFsmInfo = prAdapter->rWifiVar.prWonDevFsmInfo;
	if (prWonDevFsmInfo->fgInitialied == TRUE) {
		DBGLOG(WON, INFO,
			"won dev %u already initialized.\n",
			prWonDevFsmInfo->ucBssIndex);
		prWonBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
			prWonDevFsmInfo->ucBssIndex);
		/* return; */
	}

	kalMemZero(prWonDevFsmInfo, sizeof(struct WON_DEV_FSM_INFO));

	prWonDevFsmInfo->eCurrentState = WON_DEV_STATE_IDLE;
	prWonDevFsmInfo->ucBssIndex = prWonBssInfo->ucBssIndex;
	prWonDevFsmInfo->fgInitialied = TRUE;
}

static void wonRoleFsmInit(struct ADAPTER *prAdapter,
	struct BSS_INFO *prWonBssInfo)
{
	struct WON_ROLE_FSM_INFO *prWonRoleFsmInfo =
		(struct WON_ROLE_FSM_INFO *) NULL;
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		if (WON_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, WON_DEFAULT_INDEX)
			!= NULL) {
			DBGLOG(WON, ERROR,
				"Error already init for role %d\n",
				WON_DEFAULT_INDEX);
			break;
		}

		prWonRoleFsmInfo = kalMemZAlloc(
			sizeof(struct WON_ROLE_FSM_INFO),
			VIR_MEM_TYPE);

		WON_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, WON_DEFAULT_INDEX) =
			prWonRoleFsmInfo;

		if (!prWonRoleFsmInfo) {
			DBGLOG(WON, ERROR,
				"Error allocating fsm Info Structure\n");
			u4Status = WLAN_STATUS_RESOURCES;
			break;
		}

		kalMemZero(prWonRoleFsmInfo, sizeof(struct WON_ROLE_FSM_INFO));

		prWonRoleFsmInfo->ucRoleIndex = WON_DEFAULT_INDEX;
		prWonRoleFsmInfo->eCurrentState = WON_ROLE_STATE_IDLE;
		prWonRoleFsmInfo->u4WonPacketFilter =
			PARAM_PACKET_FILTER_SUPPORTED;
		prWonRoleFsmInfo->ucBssIndex = prWonBssInfo->ucBssIndex;

		cnmTimerInitTimer(prAdapter,
			&(prWonRoleFsmInfo->rWonFsmTimeoutTimer),
			wonFsmRunEventTimeout,
			(uintptr_t)prWonRoleFsmInfo);

		u4Status = WLAN_STATUS_SUCCESS;
	} while (FALSE);

	if (u4Status != WLAN_STATUS_SUCCESS) {
		if (prWonRoleFsmInfo) {
			WON_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
				WON_DEFAULT_INDEX) = NULL;
			kalMemFree(prWonRoleFsmInfo, VIR_MEM_TYPE,
				sizeof(struct WON_ROLE_FSM_INFO));
		}
	}
}

static uint8_t wonFsmInit(
	struct ADAPTER *prAdapter,
	uint8_t aucMacAddr[])
{
	struct BSS_INFO *prWonBssInfo = (struct BSS_INFO *) NULL;
	struct GL_WON_INFO *prWonInfo = (struct GL_WON_INFO *)NULL;
	const struct NON_HT_PHY_ATTRIBUTE *prLegacyPhyAttr;
	uint8_t ucLegacyPhyTp;
	struct WIFI_VAR *prWifiVar;
	uint8_t ucMaxBandwidth = MAX_BW_UNKNOWN;

	if (prAdapter == NULL) {
		DBGLOG(WON, ERROR, "prAdapter is NULL\n");
		return MAX_BSSID_NUM;
	}

	prWonBssInfo = cnmGetBssInfoAndInit(prAdapter,
		NETWORK_TYPE_WON, FALSE);
	if (prWonBssInfo == NULL) {
		DBGLOG(WON, INFO, "No enough BSS INDEX\n");
		return MAX_BSSID_NUM;
	}

	prWonInfo = prAdapter->prGlueInfo->prWONInfo[WON_DEFAULT_INDEX];

	DBGLOG(WON, INFO, "WON BSSIFO INDEX %d %p\n",
	       prWonBssInfo->ucBssIndex, prWonBssInfo);
	COPY_MAC_ADDR(prWonBssInfo->aucOwnMacAddr,
		      aucMacAddr);

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO,
		"aucOwnMacAddr=" MACSTR ", aucMacAddr param=" MACSTR "\n",
		MAC2STR(prWonBssInfo->aucOwnMacAddr),
		MAC2STR(aucMacAddr));
#endif

	DBGLOG(WON, INFO,
		"Set won dev mac to " MACSTR "\n",
		MAC2STR(prWonBssInfo->aucOwnMacAddr));

	prWonBssInfo->eCurrentOPMode = OP_MODE_WON;

	prWifiVar = &prAdapter->rWifiVar;
	ucLegacyPhyTp = PHY_TYPE_OFDM_INDEX;
	prLegacyPhyAttr = &rNonHTPhyAttributes[ucLegacyPhyTp];

	prWonBssInfo->u4PrivateData = 0;
	prWonBssInfo->ucSSIDLen = 0;
	prWonBssInfo->fgIsQBSS = 1;
	prWonBssInfo->eConnectionState = MEDIA_STATE_DISCONNECTED;
	prWonBssInfo->ucOpRxNss = wlanGetSupportNss(
		prAdapter, prWonBssInfo->ucBssIndex);

	/* let secIsProtectedFrame to decide protection or
	 * not by STA record
	 */
	prWonBssInfo->fgIsProtection = FALSE;

	cnmWmmIndexDecision(prAdapter, prWonBssInfo);

#if (CFG_SUPPORT_802_11BE == 1)
	prWonBssInfo->ucPhyTypeSet =
		prWifiVar->ucAvailablePhyTypeSet &
		PHY_TYPE_SET_802_11ABGNACAXBE;
#elif (CFG_SUPPORT_802_11AX == 1)
	prWonBssInfo->ucPhyTypeSet =
		prWifiVar->ucAvailablePhyTypeSet &
		PHY_TYPE_SET_802_11ABGNACAX;
#elif (CFG_SUPPORT_802_11AC == 1)
	prWonBssInfo->ucPhyTypeSet =
		prWifiVar->ucAvailablePhyTypeSet &
		PHY_TYPE_SET_802_11ANAC;
#else
	prWonBssInfo->ucPhyTypeSet =
		prWifiVar->ucAvailablePhyTypeSet &
		PHY_TYPE_SET_802_11AN;
#endif

	prWonBssInfo->ucNonHTBasicPhyType = ucLegacyPhyTp;
	if (prLegacyPhyAttr
		    ->fgIsShortPreambleOptionImplemented &&
	    (prWifiVar->ePreambleType == PREAMBLE_TYPE_SHORT ||
	     prWifiVar->ePreambleType == PREAMBLE_TYPE_AUTO))
		prWonBssInfo->fgUseShortPreamble = TRUE;
	else
		prWonBssInfo->fgUseShortPreamble = FALSE;
	prWonBssInfo->fgUseShortSlotTime =
		prLegacyPhyAttr
			->fgIsShortSlotTimeOptionImplemented;

	prWonBssInfo->u2OperationalRateSet =
		prLegacyPhyAttr->u2SupportedRateSet;
	prWonBssInfo->u2BSSBasicRateSet = BASIC_RATE_SET_OFDM;
	/* Mask CCK 1M For Sco scenario except FDD mode */
	if (prAdapter->u4FddMode == FALSE)
		prWonBssInfo->u2BSSBasicRateSet &=
			~RATE_SET_BIT_1M;
	prWonBssInfo->u2VhtBasicMcsSet = 0;

	prWonBssInfo->fgErpProtectMode = FALSE;
	prWonBssInfo->eHtProtectMode = HT_PROTECT_MODE_NONE;
	prWonBssInfo->eGfOperationMode = GF_MODE_DISALLOWED;
	prWonBssInfo->eRifsOperationMode = RIFS_MODE_DISALLOWED;

	prWonBssInfo->ucPrimaryChannel =
		nicFreq2ChannelNum(prWonInfo->channel.freq * 1000);
	if (prWonBssInfo->ucPrimaryChannel > 0) {
		if ((prWonInfo->channel.freq >= 2412) &&
			(prWonInfo->channel.freq <= 2484))
			prWonBssInfo->eBand = BAND_2G4;
		else if ((prWonInfo->channel.freq >= 5180) &&
			(prWonInfo->channel.freq <= 5900))
			prWonBssInfo->eBand = BAND_5G;
#if (CFG_SUPPORT_WIFI_6G == 1)
		else if ((prWonInfo->channel.freq >= 5955) &&
			(prWonInfo->channel.freq <= 7115))
			prWonBssInfo->eBand = BAND_6G;
#endif
	}

	prWonBssInfo->eBssSCO = CHNL_EXT_SCN;
	prWonBssInfo->ucHtOpInfo1 = 0;
	prWonBssInfo->u2HtOpInfo2 = 0;
	prWonBssInfo->u2HtOpInfo3 = 0;

	ucMaxBandwidth = cnmGetBssMaxBw(prAdapter, prWonBssInfo->ucBssIndex);
	if (ucMaxBandwidth >= MAX_BW_NUM) {
		prWonBssInfo->ucVhtChannelWidth = CW_20_40MHZ;
	} else if (ucMaxBandwidth >= MAX_BW_40MHZ) {
		prWonBssInfo->ucVhtChannelWidth = ucMaxBandwidth - 1;
		prWonBssInfo->eBssSCO = nicGetSco(prAdapter,
			prWonBssInfo->eBand, prWonBssInfo->ucPrimaryChannel);
		prWonBssInfo->ucHtOpInfo1 |=
			HT_OP_INFO1_STA_CHNL_WIDTH;
		prWonBssInfo->fgAssoc40mBwAllowed = TRUE;
	} else {
		prWonBssInfo->ucVhtChannelWidth = CW_20_40MHZ;
	}

	prWonBssInfo->ucVhtChannelFrequencyS1 = nicGetS1(prWonBssInfo->eBand,
		prWonBssInfo->ucPrimaryChannel,
		prWonBssInfo->ucVhtChannelWidth);
	prWonBssInfo->ucVhtChannelFrequencyS2 = 0;

	/* TODO: fix nicGetS1 when BW20 */
	if (prWonBssInfo->ucPrimaryChannel == 149 &&
	    ucMaxBandwidth == MAX_BW_80MHZ)
		prWonBssInfo->ucVhtChannelFrequencyS1 = 155;
	else if (prWonBssInfo->ucPrimaryChannel == 6)
		prWonBssInfo->ucVhtChannelFrequencyS1 = 0;

	prWonBssInfo->u2HwDefaultFixedRateCode = RATE_OFDM_6M;
	rateGetDataRatesFromRateSet(
		prWonBssInfo->u2OperationalRateSet,
		prWonBssInfo->u2BSSBasicRateSet,
		prWonBssInfo->aucAllSupportedRates,
		&prWonBssInfo->ucAllSupportedRatesLen);

#if (CFG_SUPPORT_802_11AX == 1)
	/* Set DBRTS to 0x3FF as defalt */
	prWonBssInfo->ucHeOpParams[0] |=
		HE_OP_PARAM0_TXOP_DUR_RTS_THRESHOLD_MASK;
	prWonBssInfo->ucHeOpParams[1] |=
		HE_OP_PARAM1_TXOP_DUR_RTS_THRESHOLD_MASK;
#endif

#if (CFG_SUPPORT_WON_11BE_MLO)
	wonMldBssRegister(prAdapter,
		prWonBssInfo);
#endif

	wonDevFsmInit(prAdapter, prWonBssInfo);
	wonRoleFsmInit(prAdapter, prWonBssInfo);

	/* Activate WON BSS */
	if (!IS_BSS_ACTIVE(prWonBssInfo)) {
		nicUpdateBss(prAdapter, prWonBssInfo->ucBssIndex);

#if (CFG_SUPPORT_WON_DBDC == 1)
		/* Check if DBDC is required to be enabled first */
		CNM_DBDC_ADD_DECISION_INFO(rDbdcDecisionInfo,
			prWonBssInfo->ucBssIndex,
			prWonBssInfo->eBand,
			prWonBssInfo->ucPrimaryChannel,
			prWonBssInfo->ucWmmQueSet);

		cnmDbdcPreConnectionEnableDecision(
			prAdapter,
			&rDbdcDecisionInfo);
#endif

		/* DBDC decsion may change OpNss */
		cnmOpModeGetTRxNss(prAdapter,
			prWonBssInfo->ucBssIndex,
			&prWonBssInfo->ucOpRxNss,
			&prWonBssInfo->ucOpTxNss);

		nicActivateNetwork(prAdapter, prWonBssInfo->ucBssIndex);

		wonStartBssReqCh(prAdapter,
			WON_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
				WON_DEFAULT_INDEX),
			prWonBssInfo);
	}

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "dump won init bss info\n");
	bssDumpBssInfo(prAdapter, prWonBssInfo->ucBssIndex);
#endif

	return prWonBssInfo->ucBssIndex;
}

static void wonFsmUninit(struct ADAPTER *prAdapter)
{
	struct WON_DEV_FSM_INFO *prWonDevFsmInfo =
		(struct WON_DEV_FSM_INFO *) NULL;
	struct BSS_INFO *prWonBssInfo = (struct BSS_INFO *) NULL;
	struct WON_ROLE_FSM_INFO *prWonRoleFsmInfo =
		(struct WON_ROLE_FSM_INFO *) NULL;

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	do {
		ASSERT_BREAK(prAdapter != NULL);

		prWonDevFsmInfo = prAdapter->rWifiVar.prWonDevFsmInfo;
		prWonDevFsmInfo->fgInitialied = FALSE;

		ASSERT_BREAK(prWonDevFsmInfo != NULL);

		prWonBssInfo =
			prAdapter->aprBssInfo[prWonDevFsmInfo->ucBssIndex];

		prWonRoleFsmInfo = WON_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			WON_DEFAULT_INDEX);

		/* Abort device FSM */
		wonStateAbort_IDLE(prAdapter,
			prWonRoleFsmInfo,
			&(prWonRoleFsmInfo->rChnlReqInfo));

		SET_NET_PWR_STATE_IDLE(prAdapter, prWonBssInfo->ucBssIndex);

		/* Clear CmdQue */
		kalClearMgmtFramesByBssIdx(prAdapter->prGlueInfo,
			prWonBssInfo->ucBssIndex);
		/* Clear PendingCmdQue */
		wlanReleasePendingCMDbyBssIdx(prAdapter,
			prWonBssInfo->ucBssIndex);
		/* Clear PendingTxMsdu */
		nicFreePendingTxMsduInfo(prAdapter,
			prWonBssInfo->ucBssIndex, MSDU_REMOVE_BY_BSS_INDEX);

		/* Deactivate BSS. */
		/* Call cnmStaFreeAllStaByNetwork */
		nicDeactivateNetwork(prAdapter, prWonBssInfo->ucBssIndex);

		cnmFreeBssInfo(prAdapter, prWonBssInfo);

		WON_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			WON_DEFAULT_INDEX) = NULL;

		kalMemFree(prWonRoleFsmInfo, VIR_MEM_TYPE,
			sizeof(struct WON_ROLE_FSM_INFO));
	} while (FALSE);

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif
}

/* From gl_p2p.c*/
static int wonInit(struct net_device *prDev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPriv;

	if (!prDev)
		return -ENXIO;

#if CFG_SUPPORT_RX_GRO
	kalRxGroInit(prDev);
#endif /* CFG_SUPPORT_RX_GRO */

	prNetDevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(prDev);

	INIT_WORK(&(prNetDevPriv->workq),
		wonSetMulticastListWorkQueue);

	return 0;
}

static void wonUninit(struct net_device *prDev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPriv;

	prNetDevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(prDev);
	cancel_work_sync(&(prNetDevPriv->workq));
}

const struct net_device_ops won_netdev_ops = {
	.ndo_open = wonOpen,
	.ndo_stop = wonStop,
	.ndo_set_mac_address = wonSetMACAddress,
	.ndo_set_rx_mode = wonSetMulticastList,
	.ndo_get_stats = wonGetStats,
	.ndo_do_ioctl = wonDoIOCTL,
#if KERNEL_VERSION(5, 15, 0) <= CFG80211_VERSION_CODE
	.ndo_siocdevprivate = wonDoPrivIOCTL,
#endif
	.ndo_start_xmit = wonHardStartXmit,
	.ndo_select_queue = wlanSelectQueue,
	.ndo_init = wonInit,
	.ndo_uninit = wonUninit,
};

void wonFreeMemSafe(struct GLUE_INFO *prGlueInfo,
		void **pprMemInfo, uint32_t size)
{
	void *prTmpMemInfo = NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	prTmpMemInfo = *pprMemInfo;
	*pprMemInfo = NULL;
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	kalMemFree(prTmpMemInfo, VIR_MEM_TYPE, size);
}

u_int8_t wonAllocInfo(struct GLUE_INFO *prGlueInfo, uint8_t ucIdex)
{
	struct ADAPTER *prAdapter = NULL;
	struct WIFI_VAR *prWifiVar = NULL;

	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	prWifiVar = &(prAdapter->rWifiVar);

	ASSERT(prAdapter);
	ASSERT(prWifiVar);

	do {
		if (prGlueInfo->prWONInfo[ucIdex] == NULL) {
			/*alloc memory for won info */
			prGlueInfo->prWONInfo[ucIdex] =
				kalMemAlloc(sizeof(struct GL_WON_INFO),
					VIR_MEM_TYPE);

			if (prGlueInfo->prWONDevInfo == NULL) {
				prGlueInfo->prWONDevInfo =
					kalMemAlloc(
						sizeof(struct GL_WON_DEV_INFO),
						VIR_MEM_TYPE);
				if (prGlueInfo->prWONDevInfo) {
					kalMemZero(prGlueInfo->prWONDevInfo,
						sizeof(struct GL_WON_DEV_INFO));
				}
			}

			if (prAdapter->prWonInfo == NULL) {
				prAdapter->prWonInfo =
					kalMemAlloc(sizeof(struct WON_DEV_INFO),
						    VIR_MEM_TYPE);
				if (prAdapter->prWonInfo) {
					kalMemZero(prAdapter->prWonInfo,
						   sizeof(struct WON_DEV_INFO));
				}
			}

			if (prWifiVar->prWonDevFsmInfo == NULL) {
				/* Don't only create WON device for ucIdex 0.
				 * Avoid the exception that mtk_init_ap_role
				 * called without wondertap0.
				 */
				prWifiVar->prWonDevFsmInfo =
					kalMemAlloc(
						sizeof(struct WON_DEV_FSM_INFO),
						VIR_MEM_TYPE);
				if (prWifiVar->prWonDevFsmInfo) {
					kalMemZero(prWifiVar->prWonDevFsmInfo,
						sizeof(struct
							WON_DEV_FSM_INFO));
				}
			}

		} else {
			ASSERT(prAdapter->prWonInfo != NULL);
		}
		/*MUST set memory to 0 */
		kalMemZero(prGlueInfo->prWONInfo[ucIdex],
			sizeof(struct GL_WON_INFO));

		init_completion(&prGlueInfo->prWONInfo[ucIdex]->rDisconnComp);
	} while (FALSE);

	if (!prGlueInfo->prWONDevInfo)
		DBGLOG(WON, ERROR, "prWONDevInfo error\n");
	else
		DBGLOG(WON, TRACE, "prWONDevInfo ok\n");

	if (!prGlueInfo->prWONInfo[ucIdex])
		DBGLOG(WON, ERROR, "prWONInfo error\n");
	else
		DBGLOG(WON, TRACE, "prWONInfo ok\n");

	/* chk if alloc successful or not */
	if (prGlueInfo->prWONInfo[ucIdex] &&
		prGlueInfo->prWONDevInfo &&
		prAdapter->prWonInfo)
		return TRUE;

	DBGLOG(WON, ERROR, "fail\n");

	if (prGlueInfo->prWONDevInfo) {
		kalMemFree(prGlueInfo->prWONDevInfo,
			VIR_MEM_TYPE, sizeof(struct GL_WON_DEV_INFO));

		prGlueInfo->prWONDevInfo = NULL;
	}
	if (prGlueInfo->prWONInfo[ucIdex]) {
		kalMemFree(prGlueInfo->prWONInfo[ucIdex],
			VIR_MEM_TYPE, sizeof(struct GL_WON_INFO));

		prGlueInfo->prWONInfo[ucIdex] = NULL;
	}
	if (prAdapter->prWonInfo) {
		kalMemFree(prAdapter->prWonInfo,
			VIR_MEM_TYPE, sizeof(struct WON_DEV_INFO));

		prAdapter->prWonInfo = NULL;
	}
	return FALSE;

}

u_int8_t wonFreeInfo(struct GLUE_INFO *prGlueInfo, uint8_t ucIdx)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	struct GL_WON_INFO *prGlWonInfo;
	struct WIFI_VAR *prWifiVar;

	ASSERT(prGlueInfo);
	ASSERT(prAdapter);

	if (ucIdx >= KAL_WON_NUM) {
		DBGLOG(WON, ERROR, "ucIdx=%d is invalid\n", ucIdx);
		return FALSE;
	}

	/* Expect that prAdapter->prWonInfo must be existing. */
	if (prAdapter->prWonInfo == NULL) {
		DBGLOG(WON, ERROR, "prAdapter->prWonInfo is NULL\n");
		return FALSE;
	}

	prWifiVar = &prAdapter->rWifiVar;
	prGlWonInfo = prGlueInfo->prWONInfo[ucIdx];

	/* TODO: how can I sure that the specific WON device can be freed?
	 * The original check is that prGlueInfo->prAdapter->fgIsWONRegistered.
	 * For one wiphy feature, this func may be called without
	 * (fgIsWONRegistered == FALSE) condition.
	 */

	if (prGlWonInfo != NULL) {
		wonFreeMemSafe(prGlueInfo,
			(void **)&prGlWonInfo,
			sizeof(struct GL_WON_INFO));
		prGlueInfo->prWONInfo[ucIdx] = NULL;
		prAdapter->prWonInfo->u4DeviceNum--;
	}

	if (prAdapter->prWonInfo->u4DeviceNum == 0) {
		/* all prWONInfo are freed, and free the general part now */

		wonFreeMemSafe(prGlueInfo,
			(void **)&prAdapter->prWonInfo,
			sizeof(struct WON_DEV_INFO));

		if (prGlueInfo->prWONDevInfo) {
			wonFreeMemSafe(prGlueInfo,
				(void **)&prGlueInfo->prWONDevInfo,
				sizeof(struct GL_WON_DEV_INFO));
		}
		if (prAdapter->rWifiVar.prWonDevFsmInfo) {
			wonFreeMemSafe(prGlueInfo,
				(void **)&prWifiVar->prWonDevFsmInfo,
				sizeof(struct WON_DEV_FSM_INFO));
		}
	}

	return TRUE;
}

u_int8_t wonNetRegister(
	struct GLUE_INFO *prGlueInfo,
	uint8_t fgIsRtnlLockAcquired)
{
	struct ADAPTER *prAdapter = NULL;
	struct net_device *prDevHandler = NULL;
	struct net_device **pprWonDev = NULL;
	uint32_t i;
	int32_t i4RetReg;
	u_int8_t ret = TRUE, fgDoRegister = FALSE;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	pprWonDev = prGlueInfo->prWonDev;

	ASSERT(prAdapter);

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	if (prAdapter->rWONNetRegState == ENUM_NET_REG_STATE_UNREGISTERED &&
		prAdapter->rWONRegState == ENUM_WON_REG_STATE_REGISTERED) {
		prAdapter->rWONNetRegState = ENUM_NET_REG_STATE_REGISTERING;
		fgDoRegister = TRUE;
	}
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	if (!fgDoRegister) {
		DBGLOG(WON, WARN,
			"skip register, won_state=%d, net_state=%d\n",
			prAdapter->rWONRegState,
			prAdapter->rWONNetRegState);
		return TRUE;
	}

	for (i = 0; i < prGlueInfo->prAdapter->prWonInfo->u4DeviceNum; i++) {
		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		prDevHandler = prGlueInfo->prWONInfo[i] ?
			prGlueInfo->prWONInfo[i]->prDevHandler :
			NULL;

		/* Check NETREG_RELEASED for the case that free_netdev
		 * is called but not set to NULL yet.
		 */
		if (prDevHandler == NULL ||
		    prDevHandler->reg_state == NETREG_RELEASED) {
			prAdapter->rWONNetRegState =
				ENUM_NET_REG_STATE_UNREGISTERED;
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
			ret = FALSE;
			goto fail;
		}

		/* net device initialize */
		netif_carrier_off(prDevHandler);
		netif_tx_stop_all_queues(prDevHandler);

		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

		if (fgIsRtnlLockAcquired) {
#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE
			i4RetReg = cfg80211_register_netdevice(prDevHandler);
#else
			i4RetReg = register_netdevice(prDevHandler);
#endif
		} else {
			i4RetReg = register_netdev(prDevHandler);
		}

		prDevHandler->type = 0;

		DBGLOG(WON, INFO,
			"wonmac interface %d %s ifindex=%d reg=%d, f=%d\n",
			i, prDevHandler->name, prDevHandler->ifindex,
			i4RetReg,
			prDevHandler->flags);

		if (i4RetReg) {
			ret = FALSE;
			goto fail;
		} else {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
			pprWonDev[i] = prDevHandler;
			DBGLOG(WON, INFO,
				"[wonmac] prDevHandler = %p\n", prDevHandler);
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		}
	}

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	prAdapter->rWONNetRegState = ENUM_NET_REG_STATE_REGISTERED;
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	goto exit;
fail:
	for (i = 0;
	     i < prGlueInfo->prAdapter->prWonInfo->u4DeviceNum;
	     i++) {
		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		prDevHandler = prGlueInfo->prWONInfo[i] ?
			prGlueInfo->prWONInfo[i]->prDevHandler :
			NULL;
		pprWonDev[i] = NULL;
		prGlueInfo->prWONInfo[i]->prDevHandler = NULL;
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

		if (!prDevHandler)
			continue;

		if (prDevHandler->reg_state == NETREG_REGISTERED) {
			if (fgIsRtnlLockAcquired) {
#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE
				cfg80211_unregister_netdevice(prDevHandler);
#else
				unregister_netdevice(prDevHandler);
#endif
			} else {
				unregister_netdev(prDevHandler);
			}
#if KERNEL_VERSION(4, 11, 9) > CFG80211_VERSION_CODE
			free_netdev(prDevHandler);
#endif
		} else {
			free_netdev(prDevHandler);
		}
	}

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	prAdapter->rWONNetRegState = ENUM_NET_REG_STATE_UNREGISTERED;
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
exit:
	return ret;
}

u_int8_t wonNetUnregister(
	struct GLUE_INFO *prGlueInfo,
	uint8_t fgIsRtnlLockAcquired,
	u_int8_t fgIsWiphyLockHeld)
{
	u_int8_t fgDoUnregister = FALSE;
	uint8_t ucRoleIdx;
	struct ADAPTER *prAdapter = NULL;
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPriv = NULL;
	struct GL_WON_INFO *prWONInfo = NULL;
	struct BSS_INFO *prWonBssInfo = NULL;
	int iftype = 0;
	struct net_device *prRoleDev = NULL;
	struct GL_WON_DEV_INFO *prGlueWonDevInfo =
		(struct GL_WON_DEV_INFO *) NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	prAdapter = prGlueInfo->prAdapter;

	ASSERT(prGlueInfo);
	ASSERT(prAdapter);

	prGlueWonDevInfo = prGlueInfo->prWONDevInfo;
	if (prGlueWonDevInfo)
		kalMsleep(10);

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	if (prAdapter->rWONNetRegState == ENUM_NET_REG_STATE_REGISTERED &&
		prAdapter->rWONRegState == ENUM_WON_REG_STATE_REGISTERED) {
		prAdapter->rWONNetRegState = ENUM_NET_REG_STATE_UNREGISTERING;
		fgDoUnregister = TRUE;
	}
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	if (!fgDoUnregister) {
		DBGLOG(WON, WARN,
			"skip unregister, won_state=%d, net_state=%d\n",
			prAdapter->rWONRegState,
			prAdapter->rWONNetRegState);
		return TRUE;
	}

	for (ucRoleIdx = 0; ucRoleIdx < KAL_WON_NUM; ucRoleIdx++) {
		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		prWONInfo = prGlueInfo->prWONInfo[ucRoleIdx];
		if (prWONInfo == NULL) {
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
			continue;
		}

		if (prWONInfo->prDevHandler == NULL) {
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
			continue;
		}

		/* don't unregister the dev that share with the AIS */
		if (wlanIsAisDev(prWONInfo->prDevHandler)) {
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
			continue;
		}

		prRoleDev = prWONInfo->aprRoleHandler;
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		if (prRoleDev != NULL) {
			/* info cfg80211 disconnect */
			prNetDevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *)
				netdev_priv(prRoleDev);
			iftype = prRoleDev->ieee80211_ptr->iftype;
			prWonBssInfo = GET_BSS_INFO_BY_INDEX(
				prAdapter,
				prNetDevPriv->ucBssIdx);

			/* FIXME: The wonRoleFsmUninit may call the
			 * cfg80211_disconnected.
			 * wonRemove()->glUnregisterWON->wonRoleFsmUninit(),
			 * it may be too late.
			 */
			if ((prWonBssInfo != NULL) &&
			    (prWonBssInfo->eConnectionState ==
				MEDIA_STATE_CONNECTED)) {
				wonChangeMediaState(prAdapter,
					prWonBssInfo,
					MEDIA_STATE_DISCONNECTED);

#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 2, 0) <= CFG80211_VERSION_CODE)
				cfg80211_disconnected(prRoleDev, 0, NULL, 0,
							TRUE, GFP_KERNEL);
#else
				cfg80211_disconnected(prRoleDev, 0, NULL, 0,
							GFP_KERNEL);
#endif
			}

			if (prRoleDev != prWONInfo->prDevHandler) {
				if (netif_carrier_ok(prRoleDev))
					netif_carrier_off(prRoleDev);

				netif_tx_stop_all_queues(prRoleDev);
			}
		}

		if (netif_carrier_ok(prWONInfo->prDevHandler))
			netif_carrier_off(prWONInfo->prDevHandler);

		netif_tx_stop_all_queues(prWONInfo->prDevHandler);

		/* Here are the functions which need rtnl_lock */
		if ((prRoleDev) && (prWONInfo->prDevHandler != prRoleDev)) {
			DBGLOG(WON, INFO, "unregister won[%d]\n", ucRoleIdx);

			if (prRoleDev->reg_state == NETREG_REGISTERED) {
				/* Kernel may lock wiphy again if UP flag still
				 * raised, force unset to avoid deadlock.
				 */
				if (prRoleDev->flags & IFF_UP) {
					DBGLOG(WON, TRACE,
					       "unset role dev flag UP\n");
					if (!fgIsRtnlLockAcquired)
						rtnl_lock();
#if KERNEL_VERSION(5, 0, 0) <= CFG80211_VERSION_CODE
					dev_change_flags(prRoleDev,
					      prRoleDev->flags ^ IFF_UP, NULL);
#else
					dev_change_flags(prRoleDev,
					      prRoleDev->flags ^ IFF_UP);
#endif
					if (!fgIsRtnlLockAcquired)
						rtnl_unlock();
				}

				if (fgIsRtnlLockAcquired) {
#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE
					struct wireless_dev *ptr =
						prRoleDev->ieee80211_ptr;

					if (!fgIsWiphyLockHeld)
						wiphy_lock(ptr->wiphy);
					cfg80211_unregister_netdevice(
							prRoleDev);
					if (!fgIsWiphyLockHeld)
						wiphy_unlock(ptr->wiphy);
#else
					unregister_netdevice(prRoleDev);
#endif
				} else {
					unregister_netdev(prRoleDev);
				}
			}
			/* This ndev is created in mtk_won_cfg80211_add_iface(),
			 * and unregister_netdev will also free the ndev.
			 */
			prWONInfo->aprRoleHandler = NULL;
		}

		DBGLOG(WON, INFO, "unregister wondev[%d]\n", ucRoleIdx);
		if (prWONInfo->prDevHandler->reg_state == NETREG_REGISTERED) {
			struct net_device *prDev;

			prDev = prWONInfo->prDevHandler;
			prWONInfo->prDevHandler = NULL;
			if (prDev == prRoleDev) {
				DBGLOG(WON, INFO,
					"set won role as NULL too\n");
				prWONInfo->aprRoleHandler = NULL;
			}

			/* Kernel may lock wiphy again if UP flag still
			 * raised, force unset to avoid deadlock.
			 */
			if (prDev->flags & IFF_UP) {
				DBGLOG(WON, TRACE, "unset wondev flag UP\n");
				if (!fgIsRtnlLockAcquired)
					rtnl_lock();
#if KERNEL_VERSION(5, 0, 0) <= CFG80211_VERSION_CODE
				dev_change_flags(prDev, prDev->flags ^ IFF_UP,
						 NULL);
#else
				dev_change_flags(prDev, prDev->flags ^ IFF_UP);
#endif
				if (!fgIsRtnlLockAcquired)
					rtnl_unlock();
			}

			if (fgIsRtnlLockAcquired) {
#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE
				struct wireless_dev *ptr = prDev->ieee80211_ptr;

				if (!fgIsWiphyLockHeld)
					wiphy_lock(ptr->wiphy);
				cfg80211_unregister_netdevice(prDev);
				if (!fgIsWiphyLockHeld)
					wiphy_unlock(ptr->wiphy);
#else
				unregister_netdevice(prDev);
#endif
			} else {
				unregister_netdev(prDev);
			}

#if KERNEL_VERSION(4, 11, 9) > CFG80211_VERSION_CODE
			free_netdev(prDev);
#endif
		}
	}

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	prAdapter->rWONNetRegState = ENUM_NET_REG_STATE_UNREGISTERED;
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	return TRUE;
}

int glSetupWON(
	struct GLUE_INFO *prGlueInfo,
	struct wireless_dev *prWonWdev,
	struct net_device *prWonDev,
	uint8_t u4Idx,
	u_int8_t fgIsApMode,
	u_int8_t fgSkipRole,
	uint8_t aucIntfMac[])
{
	struct ADAPTER *prAdapter = NULL;
	struct GL_WON_INFO *prWONInfo = NULL;
	struct GL_HIF_INFO *prHif = NULL;
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPriv = NULL;
	/* uint8_t ucGroupMldId = MLD_GROUP_NONE; */
	/* enum nl80211_iftype type; */

	GLUE_SPIN_LOCK_DECLARATION();

	DBGLOG(WON, TRACE, "setup the won dev\n");

	if ((prGlueInfo == NULL) ||
	    (prWonWdev == NULL) ||
	    (prWonWdev->wiphy == NULL) ||
	    (prWonDev == NULL)) {
		DBGLOG(WON, ERROR, "parameter is NULL!!\n");
		return -1;
	}

	prHif = &prGlueInfo->rHifInfo;
	prAdapter = prGlueInfo->prAdapter;

	if ((prAdapter == NULL) ||
	    (prHif == NULL)) {
		DBGLOG(WON, ERROR, "prAdapter/prHif is NULL!!\n");
		return -1;
	}

	/* FIXME: check KAL_WON_NUM in trunk? */
	if (u4Idx >= KAL_WON_NUM) {
		DBGLOG(WON, ERROR, "u4Idx(%d) is out of range!!\n", u4Idx);
		return -1;
	}

	/*0. allocate woninfo */
	if (wonAllocInfo(prGlueInfo, u4Idx) != TRUE) {
		DBGLOG(WON, WARN, "Allocate memory for won FAILED\n");
		return -1;
	}

	prWONInfo = prGlueInfo->prWONInfo[u4Idx];

	/* fill wiphy parameters */
	prWONInfo->prWdev = prWonWdev;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	/* setup netdev */
	/* Point to shared glue structure */
	prNetDevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(prWonDev);
	prNetDevPriv->prGlueInfo = prGlueInfo;

	/* set ucIsWondertap for WON function device */
	prWonDev->type = ARPHRD_IEEE80211_RADIOTAP;
	prWonWdev->iftype = NL80211_IFTYPE_MONITOR; /* FIXME */
	prNetDevPriv->ucIsP2p = FALSE;
	prNetDevPriv->ucMddpSupport = FALSE;
	prNetDevPriv->ucIsWondertap = TRUE;

	/* register callback functions */
	/* prWonDev->needed_headroom = wlanGetTxNeededHeadRoom(prAdapter); */
	prWonDev->netdev_ops = &won_netdev_ops;

#if defined(_HIF_SDIO)
#if (MTK_WCN_HIF_SDIO == 0)
	SET_NETDEV_DEV(prWonDev, &(prHif->func->dev));
#endif
#endif

	prWonDev->ieee80211_ptr = prWonWdev;
	prWonWdev->netdev = prWonDev;

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	/* set HW checksum offload */
	if (prAdapter->fgIsSupportCsumOffload) {
		prWonDev->features |= NETIF_F_IP_CSUM |
				     NETIF_F_IPV6_CSUM |
				     NETIF_F_RXCSUM;
	}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	kalResetStats(prWonDev);

	/* finish */
	/* bind netdev pointer to netdev index */
	prWONInfo->prDevHandler = prWonDev;
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	prWONInfo->aprRoleHandler = prWONInfo->prDevHandler;

	DBGLOG(WON, INFO,
		"[won] prDevHandler = %p, aprRoleHandler = %p\n",
		prWONInfo->prDevHandler, prWONInfo->aprRoleHandler);

	/* not init BSS yet */
	prNetDevPriv->ucBssIdx = 0xff;

	if ((fgSkipRole == SKIP_ROLE_ALL) ||
		((fgSkipRole == SKIP_ROLE_EXCEPT_MAIN) && u4Idx))
		goto exit;

	prNetDevPriv->ucBssIdx = wonFsmInit(prAdapter, aucIntfMac);
	if (prNetDevPriv->ucBssIdx == MAX_BSSID_NUM)
		DBGLOG(WON, ERROR, "wonFsmInit failed.\n");
	/* Currently wpasupplicant can't support create interface. */
	/* so initial the corresponding data structure here. */
	wlanBindBssIdxToNetInterface(
		prGlueInfo,
		prNetDevPriv->ucBssIdx,
		(void *) prWONInfo->aprRoleHandler);

exit:

	return 0;
}

u_int8_t glWonCreateWirelessDevice(struct GLUE_INFO *prGlueInfo)
{
	struct wiphy *prWiphy = NULL;
	struct wireless_dev *prWdev = NULL;
	struct wireless_dev **pprWonRoleWdev = NULL;
	struct wireless_dev **pprWonWdev = NULL;
	uint8_t	i = 0;

	if (!prGlueInfo) {
		DBGLOG(WON, ERROR, "prGlueInfo is NULL\n");
		return FALSE;
	}

	prWiphy = wlanGetWiphy();

	if (!prWiphy) {
		DBGLOG(WON, ERROR, "unable to allocate wiphy for won\n");
		return FALSE;
	}

	pprWonWdev = prGlueInfo->prWonWdev;
	pprWonRoleWdev = prGlueInfo->prWonRoleWdev;

	for (i = 0 ; i < KAL_WON_NUM; i++) {
		if (!pprWonRoleWdev[i])
			break;
	}

	if (i >= KAL_WON_NUM) {
		DBGLOG(WON, WARN, "fail to register wiphy to driver\n");
		return FALSE;
	}

	prWdev = kzalloc(sizeof(struct wireless_dev), GFP_KERNEL);
	if (!prWdev) {
		DBGLOG(WON, ERROR, "allocate won wdev fail, no memory\n");
		return FALSE;
	}

	/* set priv as pointer to glue structure */
	prWdev->wiphy = prWiphy;

	pprWonRoleWdev[i] = prWdev;
	DBGLOG(WON, TRACE, "wiphy(%p) wdev(%p)\n",
			pprWonRoleWdev[i]->wiphy, prWdev);

	/* WONDev and WONRole[0] share the same Wdev */
	pprWonWdev[i] = pprWonRoleWdev[i];

	return TRUE;
}

u_int8_t glRegisterWON(
	struct GLUE_INFO *prGlueInfo,
	const char *prDevName,
	const char *prDevName2,
	uint8_t ucApMode)
{
	struct ADAPTER *prAdapter = NULL;
	/* TODO: fix correct MAC addr */
	uint8_t rMacAddr[PARAM_MAC_ADDR_LEN] = {
		0x8e, 0x75, 0xee, 0x82, 0xa3, 0x90};
	u_int8_t fgIsApMode = FALSE;
	uint8_t  ucRegisterNum = 1, i = 0;
	struct wireless_dev *prWonWdev = NULL;
	struct net_device *prWonDev = NULL;
	struct wireless_dev **pprWonWdev = NULL;
	struct wiphy *prWiphy = NULL;
	const char *prSetDevName;
	u_int8_t fgSkipRole = SKIP_ROLE_NONE;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	/* TODO: Move the MAC address setting to the appropriate place */
	COPY_MAC_ADDR(prAdapter->rWifiVar.aucWonAddress[i], rMacAddr);

	if (ucApMode == RUNNING_WON_NO_GROUP_MODE)
		fgSkipRole = SKIP_ROLE_EXCEPT_MAIN;
	else if (ucApMode == RUNNING_WON_DEV_MODE)
		fgSkipRole = SKIP_ROLE_ALL;

	if ((ucApMode == RUNNING_WON_MODE2) ||
	    (ucApMode == RUNNING_WON_AP_MODE) ||
	    (ucApMode == RUNNING_DUAL_WON_MODE) ||
	    (ucApMode == RUNNING_WON_DEV_MODE) ||
	    (ucApMode == RUNNING_WON_NO_GROUP_MODE)) {
		ucRegisterNum = KAL_WON_NUM;
	}

	do {
		if (ucApMode == RUNNING_WON_AP_MODE) {
			if (i == 0) {
				prSetDevName = prDevName;
				fgIsApMode = FALSE;
			} else {
				prSetDevName = prDevName2;
				fgIsApMode = TRUE;
			}
		} else {
			/* RUNNING_AP_MODE
			 * RUNNING_DUAL_AP_MODE
			 * RUNNING_WON_MODE
			 * RUNNING_DUAL_WON_MODE
			 * RUNNING_WON_DEV_MODE
			 * RUNNING_WON_NO_GROUP_MODE
			 */
			prSetDevName = prDevName;

			if (ucApMode == RUNNING_WON_MODE ||
				ucApMode == RUNNING_DUAL_WON_MODE ||
				ucApMode == RUNNING_WON_DEV_MODE ||
				ucApMode == RUNNING_WON_NO_GROUP_MODE)
				fgIsApMode = FALSE;
			else
				fgIsApMode = TRUE;
		}

		pprWonWdev = prGlueInfo->prWonWdev;

		if (!pprWonWdev[i])
			glWonCreateWirelessDevice(prGlueInfo);

		if (!pprWonWdev[i]) {
			DBGLOG(WON, ERROR, "pprWonWdev[%d] is NULL\n", i);
			return FALSE;
		}

		prWonWdev = pprWonWdev[i];

		/* Reset prWonWdev for the issue that the prWonWdev doesn't
		 * reset when the usb unplug/plug.
		 */
		prWiphy = prWonWdev->wiphy;
		memset(prWonWdev, 0, sizeof(struct wireless_dev));
		prWonWdev->wiphy = prWiphy;

		/* allocate netdev */
#if KERNEL_VERSION(3, 17, 0) <= CFG80211_VERSION_CODE
		prWonDev = alloc_netdev_mq(
					sizeof(struct NETDEV_PRIVATE_GLUE_INFO),
					prSetDevName, NET_NAME_PREDICTABLE,
					ether_setup, CFG_MAX_TXQ_NUM);
#else
		prWonDev = alloc_netdev_mq(
					sizeof(struct NETDEV_PRIVATE_GLUE_INFO),
					prSetDevName,
					ether_setup, CFG_MAX_TXQ_NUM);
#endif
		if (!prWonDev) {
			DBGLOG(WON, WARN, "unable to allocate ndev for won\n");
			goto err_alloc_netdev;
		}

		COPY_MAC_ADDR(rMacAddr,
			prAdapter->rWifiVar.aucWonAddress[i]);

		DBGLOG(WON, INFO,
			"Set won[%d] mac to " MACSTR " fgIsApMode(%d)\n",
			i, MAC2STR(rMacAddr), fgIsApMode);

#if KERNEL_VERSION(4, 11, 9) <= CFG80211_VERSION_CODE
		prWonDev->needs_free_netdev = true;
#endif

#if (KERNEL_VERSION(5, 16, 0) <= CFG80211_VERSION_CODE)
		eth_hw_addr_set(prWonDev, rMacAddr);
#else
		kalMemCopy(prWonDev->dev_addr, rMacAddr, ETH_ALEN);
#endif
		kalMemCopy(prWonDev->perm_addr, prWonDev->dev_addr, ETH_ALEN);

		if (glSetupWON(prGlueInfo, prWonWdev, prWonDev, i,
			       fgIsApMode, fgSkipRole, rMacAddr) != 0) {
			DBGLOG(WON, WARN, "glSetupWON[%u] FAILED\n", i);
			free_netdev(prWonDev);
			return FALSE;
		}

		i++;
		/* prWonInfo is alloc at glSetupWON()->wonAllocInfo() */
		prAdapter->prWonInfo->u4DeviceNum++;

	} while (i < ucRegisterNum);

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	/* set won net device register state */
	/* wonNetRegister() will check prAdapter->rWONNetRegState. */
	prAdapter->rWONNetRegState = ENUM_NET_REG_STATE_UNREGISTERED;
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	return TRUE;

err_alloc_netdev:
	return FALSE;
}

u_int8_t glUnregisterWON(struct GLUE_INFO *prGlueInfo, uint8_t ucIdx,
	uint8_t fgIsRtnlLockAcquired)
{
	uint8_t ucRoleIdx;
	struct ADAPTER *prAdapter;
	struct GL_WON_INFO *prWONInfo = NULL;
	struct wireless_dev **pprWonRoleWdev = NULL;
	uint8_t ucStart = 0;
	uint8_t ucEnd = 0;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	if (ucIdx == 0xff) {
		ucStart = 0;
		ucEnd = KAL_WON_NUM;
	} else if (ucIdx < KAL_WON_NUM) {
		ucStart = ucIdx;
		ucEnd = ucIdx + 1;
	} else {
		DBGLOG(WON, WARN, "The ucIdx (%d) is a wrong value\n", ucIdx);
		return FALSE;
	}

	prAdapter = prGlueInfo->prAdapter;
	pprWonRoleWdev = prGlueInfo->prWonRoleWdev;

	/* 4 <2> Uninit WON role FSM */
	for (ucRoleIdx = ucStart; ucRoleIdx < ucEnd; ucRoleIdx++) {
		if (WON_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx))
			wonFsmUninit(prAdapter);
	}

	/* 4 <3> Free Wiphy & netdev */
	for (ucRoleIdx = ucStart; ucRoleIdx < ucEnd; ucRoleIdx++) {
		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		prWONInfo = prGlueInfo->prWONInfo[ucRoleIdx];

		if (prWONInfo == NULL) {
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
			continue;
		}

		if ((prWONInfo->aprRoleHandler != NULL) &&
		    (prWONInfo->aprRoleHandler != prWONInfo->prDevHandler)) {
			/* This device is added by the WON, and use
			 * ndev->destructor to free. The wonDevFsmUninit() use
			 * prWONInfo->aprRoleHandler to do some check.
			 */
			prWONInfo->aprRoleHandler = NULL;
			DBGLOG(WON, INFO, "aprRoleHandler idx %d set NULL\n",
					ucRoleIdx);

			/* Expect that pprWonRoleWdev[ucRoleIdx] has been reset
			 * as pprWonWdev or NULL in wonNetUnregister
			 * (unregister_netdev).
			 */
		}

		if (prWONInfo->prDevHandler) {
			struct net_device *prDev;

			prDev = prWONInfo->prDevHandler;
			prWONInfo->prDevHandler = NULL;
			if (prDev == prWONInfo->aprRoleHandler) {
				DBGLOG(WON, INFO,
					"set won role as NULL too\n");
				prWONInfo->aprRoleHandler = NULL;
			}

			/* don't free the dev that share with the AIS */
			if (wlanIsAisDev(prDev))
				pprWonRoleWdev[ucRoleIdx] = NULL;
			else {
				if (prAdapter->rWONNetRegState ==
					ENUM_NET_REG_STATE_REGISTERED) {
					DBGLOG(WON, WARN,
						"Force unregister netdev\n");
					prAdapter->rWONNetRegState =
					    ENUM_NET_REG_STATE_UNREGISTERING;
					GLUE_RELEASE_SPIN_LOCK(prGlueInfo,
						SPIN_LOCK_NET_DEV);
					if (fgIsRtnlLockAcquired)
						unregister_netdevice(prDev);
					else
						unregister_netdev(prDev);
					GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo,
						SPIN_LOCK_NET_DEV);
					prAdapter->rWONNetRegState =
						ENUM_NET_REG_STATE_UNREGISTERED;

#if KERNEL_VERSION(4, 11, 9) > CFG80211_VERSION_CODE
					free_netdev(prDev);
#endif
				} else if (prAdapter->rWONNetRegState !=
					ENUM_NET_REG_STATE_UNREGISTERED) {
					DBGLOG(WON, WARN,
						"won dev[%u] not unregister done. net_state=%d\n",
						ucRoleIdx,
						prAdapter->rWONNetRegState);
				}
			}
		}
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

		/* 4 <4> Free WON internal memory */
		if (!wonFreeInfo(prGlueInfo, ucRoleIdx)) {
			DBGLOG(WON, ERROR, "FreeInfo FAILED\n");
			return FALSE;
		}

	}

	return TRUE;
}

static int wonOpen(struct net_device *prDev)
{
	struct GLUE_INFO *prGlueInfo = NULL;
#if CFG_SUPPORT_WED_PROXY
	uint32_t u4BufLen = 0;
#endif

	ASSERT(prDev);
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	ASSERT(prGlueInfo);

#if CFG_SUPPORT_WED_PROXY
	kalIoctlByBssIdx(prGlueInfo, wlanoidWedAttachWarp, prDev,
		sizeof(struct net_device *), &u4BufLen, wlanGetBssIdx(prDev));
#endif

#if CFG_TX_GSO
	kalTxGsoInit(prDev);
#endif /* CFG_TX_GSO */

#if CFG_SW_TSO
	kalTxTsoSwInit(prDev);
#endif /* CFG_SW_TSO */

	/* 2. carrier on & start TX queue */
	/*DFS todo 20161220_DFS*/
#if (CFG_SUPPORT_DFS_MASTER == 1)
	if (prDev->ieee80211_ptr->iftype != NL80211_IFTYPE_AP) {
		/* netif_carrier_on(prDev); */
		netif_tx_start_all_queues(prDev);
	}
#else
	/* netif_carrier_on(prDev); */
	netif_tx_start_all_queues(prDev);
#endif

#ifdef CONFIG_WIRELESS_EXT
	prDev->wireless_handlers = &wext_handler_def;
#endif

	return 0;		/* success */
}

static int wonStop(struct net_device *prDev)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct GL_WON_DEV_INFO *prWonGlueDevInfo = NULL;
#if CFG_SUPPORT_WED_PROXY
	uint32_t u4BufLen = 0;
#endif

	ASSERT(prDev);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	/* XXX: The wonStop may be triggered after the wlanRemove.	*/
	/*      And prGlueInfo->prWONDevInfo is freed in wonFreeInfo.	*/
	if (!prAdapter || !prAdapter->fgIsWONRegistered)
		return -EFAULT;

	prWonGlueDevInfo = prGlueInfo->prWONDevInfo;
	ASSERT(prWonGlueDevInfo);

	/* 0. Do the scan done and set parameter to abort if the scan pending */
	/*DBGLOG(WON, INFO, "wonStop and ucRoleIdx = %u\n", ucRoleIdx);*/

	/* 1. stop TX queue */
	netif_tx_stop_all_queues(prDev);

	/* 3. stop queue and turn off carrier */
	/*prGlueInfo->prWONInfo[0]->eState = MEDIA_STATE_DISCONNECTED;*/

	netif_tx_stop_all_queues(prDev);
	if (netif_carrier_ok(prDev))
		netif_carrier_off(prDev);

#ifdef CONFIG_WIRELESS_EXT
	prDev->wireless_handlers = NULL;
#endif

#if CFG_SUPPORT_WED_PROXY
	if (kalIsHalted(prGlueInfo) == FALSE)
		kalIoctlByBssIdx(prGlueInfo, wlanoidWedDetachWarp, prDev,
			sizeof(struct net_device *), &u4BufLen,
			wlanGetBssIdx(prDev));
	else
		wlanoidWedDetachWarp(prGlueInfo->prAdapter, prDev,
			sizeof(struct net_device *), &u4BufLen);
#endif

	return 0;
}

/*---------------------------------------------------------------------------*/
/*!
 * \brief A method of struct net_device,
 *        to get the network interface statistical
 *        information.
 *
 * Whenever an application needs to get statistics for the interface,
 * this method is called.
 * This happens, for example, when ifconfig or netstat -i is run.
 *
 * \param[in] prDev      Pointer to struct net_device.
 *
 * \return net_device_stats buffer pointer.
 */
/*---------------------------------------------------------------------------*/
struct net_device_stats *wonGetStats(struct net_device *prDev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate;

	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			netdev_priv(prDev);
	kalMemCopy(&prNetDevPrivate->stats, &prDev->stats,
			sizeof(struct net_device_stats));
#ifdef CFG_SUPPORT_WON_TODO
#if CFG_MTK_MDDP_SUPPORT
	mddpGetMdStats(prDev);
#endif
#endif

	return (struct net_device_stats *) &prNetDevPrivate->stats;
}

static void wonSetMulticastList(struct net_device *prDev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPriv;
	struct GLUE_INFO *prGlueInfo;

	if (!prDev)
		return;

	prNetDevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(prDev);
	prGlueInfo = prNetDevPriv->prGlueInfo;
	if (!prGlueInfo || !prGlueInfo->u4ReadyFlag) {
		DBGLOG(WON, WARN, "driver is not ready\n");
		return;
	}

	schedule_work(&(prNetDevPriv->workq));
}

static void wonSetMulticastListWorkQueue(struct work_struct *work)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPriv = CONTAINER_OF(work,
		struct NETDEV_PRIVATE_GLUE_INFO, workq);
	struct GLUE_INFO *prGlueInfo = prNetDevPriv->prGlueInfo;
	struct net_device *prDev;
	uint32_t u4SetInfoLen = 0, u4McCount;

	prDev = wlanGetNetDev(prGlueInfo, prNetDevPriv->ucBssIdx);
	if (!prDev) {
		DBGLOG(WON, ERROR,
			"prDev for Bss%d not exist.\n",
			prNetDevPriv->ucBssIdx);
		return;
	}

	DBGLOG(WON, TRACE, "Bss[%d] set multicast list, flags=0x%x\n",
		prNetDevPriv->ucBssIdx,
		prDev->flags);

	if (prDev->flags & IFF_PROMISC)
		prGlueInfo->prWONDevInfo->u4PacketFilter
			|= PARAM_PACKET_FILTER_PROMISCUOUS;

	if (prDev->flags & IFF_BROADCAST)
		prGlueInfo->prWONDevInfo->u4PacketFilter
			|= PARAM_PACKET_FILTER_BROADCAST;

	u4McCount = netdev_mc_count(prDev);
	if (prDev->flags & IFF_MULTICAST) {
		if ((prDev->flags & IFF_ALLMULTI)
			|| (u4McCount > MAX_NUM_GROUP_ADDR))
			prGlueInfo->prWONDevInfo->u4PacketFilter
				|= PARAM_PACKET_FILTER_ALL_MULTICAST;
		else
			prGlueInfo->prWONDevInfo->u4PacketFilter
				|= PARAM_PACKET_FILTER_MULTICAST;
	}

	if (prGlueInfo->prWONDevInfo->u4PacketFilter
		& PARAM_PACKET_FILTER_MULTICAST) {
		/* Prepare multicast address list */
		struct PARAM_MULTICAST_LIST rMcAddrList;
		struct netdev_hw_addr *ha;
		uint32_t i = 0;

		/* Avoid race condition with kernel net subsystem */
		netif_addr_lock_bh(prDev);
		kalMemZero(&rMcAddrList, sizeof(rMcAddrList));

		netdev_for_each_mc_addr(ha, prDev) {
			/* If ha is null, it will break the loop. */
			/* Check mc count before accessing to ha to
			 * prevent from kernel crash.
			 */
			if (i == u4McCount || !ha)
				break;
			if (i < MAX_NUM_GROUP_ADDR) {
				COPY_MAC_ADDR(
					&rMcAddrList.aucMcAddrList[i],
					GET_ADDR(ha));
				i++;
			}
		}

		rMcAddrList.ucBssIdx = prNetDevPriv->ucBssIdx;
		rMcAddrList.ucAddrNum = i;
		rMcAddrList.fgIsOid = TRUE;

		netif_addr_unlock_bh(prDev);

		if (i >= MAX_NUM_GROUP_ADDR)
			return;

		kalIoctlByBssIdx(prGlueInfo,
				 wlanoidSetMulticastList,
				 &rMcAddrList,
				 sizeof(struct PARAM_MULTICAST_LIST),
				 &u4SetInfoLen,
				 prNetDevPriv->ucBssIdx);
	}
}

void wonTxDataMgmt(struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr)
{
	struct MSG_WON_TX *prMsgWonTx = NULL;
	uint32_t ucStaRecIdx = STA_REC_INDEX_NOT_FOUND;
	struct STA_RECORD *prStaRec = NULL;
	uint8_t ucBssIndex;
	struct MSDU_INFO *prMsduInfo;
	uint16_t u2EstimatedFrameLen;

	prMsgWonTx = (struct MSG_WON_TX *) prMsgHdr;
	prMsduInfo = prMsgWonTx->prMsduInfo;

	if (prMsgWonTx->eIsDataMgmtFrame == WON_MSG_TX_DATA) {
		ucBssIndex = prMsgWonTx->ucBssIndex;

		wonPeerAdd(prAdapter, ucBssIndex, prMsgWonTx->addr1, FALSE);
		/* free */
		if (prMsduInfo) {
			nicTxFreeMsduInfoPacket(prAdapter, prMsduInfo);
			nicTxReturnMsduInfo(prAdapter, prMsduInfo);
		}
	} else if (prMsgWonTx->eIsDataMgmtFrame == WON_MSG_TX_MGMT) {
		ucBssIndex = prMsgWonTx->ucBssIndex;
		u2EstimatedFrameLen = prMsgWonTx->u2EstimatedFrameLen;

		if (IS_BMCAST_MAC_ADDR(prMsgWonTx->addr1))
			ucStaRecIdx = STA_REC_INDEX_BMCAST;
		else {
#if CFG_WONDERTAP_AUTO_ADD_STA
			/* Workaround to add sta record here */
			wonPeerAdd(prAdapter,
				ucBssIndex, prMsgWonTx->addr1, FALSE);
#endif
			prStaRec = cnmGetStaRecByAddress(prAdapter,
				ucBssIndex, prMsgWonTx->addr1);
			if (prStaRec)
				ucStaRecIdx = prStaRec->ucIndex;
		}

		TX_SET_MMPDU(prAdapter,
			prMsduInfo,
			ucBssIndex,
			ucStaRecIdx,
			WLAN_MAC_MGMT_HEADER_LEN,
			u2EstimatedFrameLen,
			NULL, /* TODO ? */
			MSDU_RATE_MODE_AUTO);

		nicTxConfigPktControlFlag(prMsduInfo,
			MSDU_CONTROL_FLAG_FORCE_TX |
			MSDU_CONTROL_FLAG_MGNT_2_CMD_QUE,
			TRUE);
		nicTxSetPktRetryLimit(prMsduInfo, 3);
		nicTxSetPktLifeTime(prAdapter, prMsduInfo, 0);
		nicTxEnqueueMsdu(prAdapter, prMsduInfo);
	}

	cnmMemFree(prAdapter, prMsgHdr);
}

static netdev_tx_t __wonHardStartXmit(struct GLUE_INFO *prGlueInfo,
	struct sk_buff *prSkb,
	struct net_device *prDev,
	uint8_t ucBssIndex)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *)NULL;
	struct ieee80211_hdr *hdr;
	struct MSG_WON_TX *prMsgWonTx = NULL;
	uint16_t u2RadioTapLen;

	u2RadioTapLen = ieee80211_get_radiotap_len(prSkb->data);
	skb_pull(prSkb, u2RadioTapLen);

	hdr = (struct ieee80211_hdr *) prSkb->data;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (ieee80211_is_data(hdr->frame_control)) {
#if CFG_WONDERTAP_TX_RX_DEBUG
		DBGLOG(WON, INFO, "DataFrame addr1: " MACSTR "\n",
			MAC2STR(hdr->addr1));
		DBGLOG(WON, INFO, "DataFrame addr2: " MACSTR "\n",
			MAC2STR(hdr->addr2));
		DBGLOG(WON, INFO, "DataFrame addr3: " MACSTR "\n",
			MAC2STR(hdr->addr3));
#endif
#if CFG_WONDERTAP_AUTO_ADD_STA
		/* Workaround to add sta record here */
		if (!IS_BMCAST_MAC_ADDR(hdr->addr1)) {
			prMsgWonTx = (struct MSG_WON_TX *) cnmMemAlloc(
						prGlueInfo->prAdapter,
						RAM_TYPE_MSG,
						sizeof(struct MSG_WON_TX));
			if (prMsgWonTx == NULL) {
				DBGLOG(WON, ERROR,
					"can't alloc data prMsgWonTx\n");
				return NETDEV_TX_OK;
			}

			prMsgWonTx->rMsgHdr.eMsgId = MID_WON_DATA_MGMT_TX;
			prMsgWonTx->eIsDataMgmtFrame = WON_MSG_TX_DATA;
			COPY_MAC_ADDR(prMsgWonTx->addr1, hdr->addr1);
			prMsgWonTx->ucBssIndex = ucBssIndex;

			mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0,
				(struct MSG_HDR *) prMsgWonTx,
				MSG_SEND_METHOD_BUF);
		}
#endif
		GLUE_SET_PKT_FLAG(prSkb, ENUM_PKT_802_11);
		kalHardStartXmit(prSkb, prDev, prGlueInfo, ucBssIndex);

		if (prBssInfo &&
			(prBssInfo->eConnectionState == MEDIA_STATE_CONNECTED ||
			 prBssInfo->rStaRecOfClientList.u4NumElem > 0)) {
			kalPerMonStart(prGlueInfo);
		}
	} else if (ieee80211_is_mgmt(hdr->frame_control)) {
		uint16_t u2EstimatedFrameLen = prSkb->len;
		struct MSDU_INFO *prMsduInfo = cnmMgtPktAlloc(
			prAdapter, u2EstimatedFrameLen);
		if (!prMsduInfo) {
			DBGLOG(WON, ERROR, "MSDUAlloc fail=%u\n",
				u2EstimatedFrameLen);
			return NETDEV_TX_OK;
		}
		kalMemCopy(prMsduInfo->prPacket,
			prSkb->data,
			u2EstimatedFrameLen);
#if CFG_WONDERTAP_TX_RX_DEBUG
		DBGLOG(WON, INFO,
			"Len:%u, FC:0x%x, MgmtTxDest: " MACSTR "\n",
			u2EstimatedFrameLen,
			hdr->frame_control,
			MAC2STR(hdr->addr1));
#endif

		prMsgWonTx = (struct MSG_WON_TX *) cnmMemAlloc(
					prGlueInfo->prAdapter, RAM_TYPE_MSG,
					sizeof(struct MSG_WON_TX));
		if (prMsgWonTx == NULL) {
			DBGLOG(WON, ERROR, "can't alloc mgmt prMsgWonTx\n");
			return NETDEV_TX_OK;
		}

		prMsgWonTx->rMsgHdr.eMsgId = MID_WON_DATA_MGMT_TX;
		prMsgWonTx->eIsDataMgmtFrame = WON_MSG_TX_MGMT;
		COPY_MAC_ADDR(prMsgWonTx->addr1, hdr->addr1);
		prMsgWonTx->ucBssIndex = ucBssIndex;
		prMsgWonTx->prMsduInfo = prMsduInfo;
		prMsgWonTx->u2EstimatedFrameLen = u2EstimatedFrameLen;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0,
			(struct MSG_HDR *) prMsgWonTx,
			MSG_SEND_METHOD_BUF);
		dev_kfree_skb(prSkb);
	}

	return NETDEV_TX_OK;
}

netdev_tx_t wonHardStartXmit(struct sk_buff *prSkb,
	struct net_device *prDev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate =
		(struct NETDEV_PRIVATE_GLUE_INFO *) NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint8_t ucBssIndex;

	ASSERT(prSkb);
	ASSERT(prDev);

	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(prDev);
	prGlueInfo = prNetDevPrivate->prGlueInfo;
	ucBssIndex = prNetDevPrivate->ucBssIdx;

	if (prGlueInfo == NULL) {
		DBGLOG(WON, WARN, "prGlueInfo is NULL\n");
		dev_kfree_skb(prSkb);
		return NETDEV_TX_OK;
	}

#if CFG_CHIP_RESET_SUPPORT
	if (!wlanIsDriverReady(prGlueInfo,
		(WLAN_DRV_READY_CHECK_RESET | WLAN_DRV_READY_CHECK_WLAN_ON))) {
		DBGLOG(WON, WARN,
		"u4ReadyFlag:%u, kalIsResetting():%d, dropping the packet\n",
		prGlueInfo->u4ReadyFlag, kalIsResetting());
		dev_kfree_skb(prSkb);
		return NETDEV_TX_OK;
	}
#endif

	kalResetPacket(prGlueInfo, (void *) prSkb);

	return __wonHardStartXmit(prGlueInfo, prSkb, prDev, ucBssIndex);
}

void wonChGrant(struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr)
{
	struct MSG_CH_GRANT *prMsgChGrant = (struct MSG_CH_GRANT *) NULL;

	prMsgChGrant = (struct MSG_CH_GRANT *) prMsgHdr;
	nicUpdateBssEx(prAdapter, prMsgChGrant->ucBssIndex, FALSE);
}

int wonDoIOCTL(struct net_device *prDev, struct ifreq *prIfReq, int i4Cmd)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int ret = 0;

	ASSERT(prDev && prIfReq);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	if (!prGlueInfo) {
		DBGLOG(WON, ERROR, "prGlueInfo is NULL\n");
		return -EFAULT;
	}

	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(WON, ERROR, "Adapter is not ready\n");
		return -EINVAL;
	}

	if (i4Cmd == SIOCGIWPRIV) {
		ret = wext_support_ioctl(prDev, prIfReq, i4Cmd);
	} else if ((i4Cmd >= SIOCIWFIRSTPRIV) && (i4Cmd < SIOCIWLASTPRIV)) {
		/* 0x8BE0 ~ 0x8BFF, private ioctl region */
		ret = priv_support_ioctl(prDev, prIfReq, i4Cmd);
	} else if (i4Cmd == SIOCDEVPRIVATE + 1) {
#ifdef CFG_ANDROID_AOSP_PRIV_CMD
		ret = android_private_support_driver_cmd(prDev, prIfReq, i4Cmd);
#else
		ret = priv_support_driver_cmd(prDev, prIfReq, i4Cmd);
#endif /* CFG_ANDROID_AOSP_PRIV_CMD */
	} else {
		DBGLOG(WON, WARN, "Unexpected ioctl command: 0x%04x\n", i4Cmd);
		ret = -1;
	}

	return ret;
}

#if KERNEL_VERSION(5, 15, 0) <= CFG80211_VERSION_CODE
int wonDoPrivIOCTL(struct net_device *prDev, struct ifreq *prIfReq,
		void __user *prData, int i4Cmd)
{
	return wonDoIOCTL(prDev, prIfReq, i4Cmd);
}
#endif

int wonSetMACAddress(struct net_device *prDev, void *addr)
{
	struct ADAPTER *prAdapter = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct sockaddr *sa = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	uint8_t ucRoleIdx = 0, ucBssIdx = 0;
	u_int8_t fgIsNetDevFound = FALSE;
	struct NETDEV_PRIVATE_GLUE_INFO *prDevPrivate =
		(struct NETDEV_PRIVATE_GLUE_INFO *) NULL;
#if (KERNEL_VERSION(5, 16, 0) <= CFG80211_VERSION_CODE)
	u8 _addr[ETH_ALEN];
#endif

	if (!prDev || !addr) {
		DBGLOG(WON, ERROR, "Set macaddr with ndev(%d) and addr(%d)\n",
		       (prDev == NULL) ? 0 : 1, (addr == NULL) ? 0 : 1);
		return -EINVAL;
	}

	sa = (struct sockaddr *)addr;

	prDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(prDev);
	if (!prDevPrivate) {
		DBGLOG(WON, ERROR, "Null prDevPrivate\n");
		return WONDERTAP_STATUS_FAIL;
	}

	ucBssIdx = prDevPrivate->ucBssIdx;

	prGlueInfo = prDevPrivate->prGlueInfo;
	if (!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHECK_ALL)) {
		DBGLOG(WON, WARN, "driver is not ready\n");
		return WONDERTAP_STATUS_FAIL;
	}
	prAdapter = prGlueInfo->prAdapter;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);
	if (!prBssInfo) {
		DBGLOG(WON, ERROR, "bss is not active\n");
		goto skip_role;
	}

	COPY_MAC_ADDR(prBssInfo->aucOwnMacAddr, sa->sa_data);
	COPY_MAC_ADDR(
		prAdapter->rWifiVar.aucWonAddress[ucRoleIdx],
		sa->sa_data);

	fgIsNetDevFound = TRUE;
	DBGLOG(WON, INFO,
		"[%u][%u] Set wonmac to " MACSTR ".\n",
		ucBssIdx, ucRoleIdx,
		MAC2STR(prBssInfo->aucOwnMacAddr));

skip_role:

	if (fgIsNetDevFound) {
#if (KERNEL_VERSION(5, 16, 0) <= CFG80211_VERSION_CODE)
		ether_addr_copy(_addr, sa->sa_data);
		eth_hw_addr_set(prDev, _addr);
#else
		COPY_MAC_ADDR(prDev->dev_addr, sa->sa_data);
#endif
	} else {
		DBGLOG(WON, WARN,
			"Unmatch net_device %s, new " MACSTR " not set.\n",
			prDev->name, MAC2STR(sa->sa_data));
	}

	return WLAN_STATUS_SUCCESS;
}

/* From gl_p2p_init.c */
u_int8_t wonLaunch(struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter = NULL;
	enum ENUM_WON_REG_STATE eWONRegState;
	enum ENUM_NET_REG_STATE eWONNetRegState;

	GLUE_SPIN_LOCK_DECLARATION();

	prAdapter = prGlueInfo->prAdapter;

	ASSERT(prGlueInfo);
	ASSERT(prAdapter);

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	if (prAdapter->rWONRegState != ENUM_WON_REG_STATE_UNREGISTERED) {
		eWONRegState = prAdapter->rWONRegState;
		eWONNetRegState = prAdapter->rWONNetRegState;
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		DBGLOG(WON, INFO, "skip launch, won_state=%d, net_state=%d\n",
			eWONRegState, eWONNetRegState);
		return FALSE;
	}

	prAdapter->rWONRegState = ENUM_WON_REG_STATE_REGISTERING;
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	if (!glRegisterWON(prGlueInfo, ifname, ifname2, mode)) {
		DBGLOG(WON, ERROR, "Launch failed\n");
		prAdapter->rWONRegState = ENUM_WON_REG_STATE_UNREGISTERED;
		return FALSE;
	}

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	prAdapter->fgIsWONRegistered = TRUE;
	prAdapter->fgIsWONRunning = FALSE;
	prAdapter->rWONRegState = ENUM_WON_REG_STATE_REGISTERED;
	DBGLOG(WON, INFO, "wonblock, disable roaming\n");
	prAdapter->rWifiVar.fgDisRoaming = FALSE;
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	DBGLOG(WON, TRACE, "Launch success, fgIsWONRegistered TRUE\n");

	wondertap_module_init();

	return TRUE;
}

uint8_t wonGetMode(void)
{
	return mode;
}

void wonSetMode(uint8_t ucMode)
{
	mode = ucMode;
	ifname = WON_INF_NAME;
	ifname2 = WON_INF_NAME;
}

u_int8_t wonRemove(
	struct GLUE_INFO *prGlueInfo,
	uint8_t fgIsRtnlLockAcquired)
{
	struct ADAPTER *prAdapter = NULL;
	u_int32_t wait = 0;

	GLUE_SPIN_LOCK_DECLARATION();

	prAdapter = prGlueInfo->prAdapter;

	ASSERT(prGlueInfo);
	ASSERT(prAdapter);

	wondertap_module_exit();

	/* We must guarantee that all won net devices are unregistered with
	 * kernel before the net devices are freed. Otherwise, when wonLaunch
	 * is invoked next time, we will get kernel exception because the old
	 * won net devices registered to kernel were volatile.
	 */
retry:
	wait = 0;
	while (wait < 2000) {
		/* won net devices are unregistered */
		if (prAdapter->rWONRegState == ENUM_WON_REG_STATE_REGISTERED &&
			prAdapter->rWONNetRegState ==
				ENUM_NET_REG_STATE_UNREGISTERED)
			break;

		/* won net devices are not unregistered yet */
		if (prAdapter->rWONRegState == ENUM_WON_REG_STATE_REGISTERED &&
			prAdapter->rWONNetRegState ==
				ENUM_NET_REG_STATE_REGISTERED) {
			wonNetUnregister(prGlueInfo, fgIsRtnlLockAcquired,
					 FALSE);
			break;
		}

		kalMsleep(100);
		wait += 100;
	}

	if (wait >= 2000) {
		DBGLOG(WON, INFO, "skip remove, won_state=%d, net_state=%d\n",
			prAdapter->rWONRegState,
			prAdapter->rWONNetRegState);
		return FALSE;
	}

	/* Make sure that won is in registered state and won net is in
	 * unregistered state before continuing the removal procedure.
	 */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	if (prAdapter->rWONRegState == ENUM_WON_REG_STATE_REGISTERED &&
		prAdapter->rWONNetRegState == ENUM_NET_REG_STATE_UNREGISTERED)
		prAdapter->rWONRegState = ENUM_WON_REG_STATE_UNREGISTERING;
	else {
		/* Someone has changed won net register state. Try again. */
		DBGLOG(WON, INFO, "retry remove, won_state=%d, net_state=%d\n",
			prAdapter->rWONRegState,
			prAdapter->rWONNetRegState);
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		goto retry;
	}
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	glUnregisterWON(prGlueInfo, 0xff, fgIsRtnlLockAcquired);

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	prAdapter->rWONRegState = ENUM_WON_REG_STATE_UNREGISTERED;
	prAdapter->fgIsWONRegistered = FALSE;
	prAdapter->fgIsWONRunning = FALSE;
	DBGLOG(WON, INFO, "wonblock, enable roaming\n");
	prAdapter->rWifiVar.fgDisRoaming = FALSE;
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	return TRUE;
}


/* From gl_init.c */
void reset_won_mode(struct GLUE_INFO *prGlueInfo,
	uint8_t fgIsRtnlLockAcquired, u_int8_t fgIsWiphyLockHeld)
{
	struct PARAM_CUSTOM_WON_SET_WITH_LOCK_STRUCT rSetWON;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	if (!prGlueInfo)
		return;

	rSetWON.u4Enable = 0;
	rSetWON.u4Mode = 0;
	rSetWON.fgIsRtnlLockAcquired = fgIsRtnlLockAcquired;

	wonNetUnregister(prGlueInfo,
		fgIsRtnlLockAcquired,
		fgIsWiphyLockHeld);

	rWlanStatus = kalIoctl(prGlueInfo,
		wlanoidSetWonMode,
		(void *) &rSetWON,
		sizeof(struct PARAM_CUSTOM_WON_SET_WITH_LOCK_STRUCT),
		&u4BufLen);

	if (rWlanStatus != WLAN_STATUS_SUCCESS)
		wonRemove(prGlueInfo, fgIsRtnlLockAcquired);

	DBGLOG(WON, INFO,
			"ret = 0x%08x\n", (uint32_t) rWlanStatus);
}

int set_won_mode_handler(struct net_device *netdev,
			 struct PARAM_CUSTOM_WON_SET_WITH_LOCK_STRUCT wonmode)
{
	struct GLUE_INFO *prGlueInfo =
		*((struct GLUE_INFO **)netdev_priv(netdev));
	struct PARAM_CUSTOM_WON_SET_WITH_LOCK_STRUCT rSetWON = {0};
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	if (!prGlueInfo)
		return -1;

#if (CFG_MTK_ANDROID_WMT)
	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(WON, ERROR, "adapter is not ready\n");
		return -1;
	}
#endif /*CFG_MTK_ANDROID_WMT*/

	/* Remember original ifindex for reset case */
	if (kalIsResetting() &&
	    prGlueInfo->prAdapter->rWifiVar.u4RegWonIfAtProbe) {
		DBGLOG(WON, INFO, "Resetting won mode at probe\n");
		return 0;
	}

	/* Resetting won mode if registered to avoid launch KE */
	if (wonmode.u4Enable
		&& prGlueInfo->prAdapter->fgIsWONRegistered
		&& !kalIsResetting()) {
		DBGLOG(WON, WARN, "Resetting won mode\n");
		reset_won_mode(prGlueInfo,
			wonmode.fgIsRtnlLockAcquired,
			wonmode.fgIsWiphyLockHeld);
	}

	rSetWON.u4Enable = wonmode.u4Enable;
	rSetWON.u4Mode = wonmode.u4Mode;
	rSetWON.fgIsRtnlLockAcquired = wonmode.fgIsRtnlLockAcquired;

	if ((!rSetWON.u4Enable) && (kalIsResetting() == FALSE))
		wonNetUnregister(prGlueInfo,
			wonmode.fgIsRtnlLockAcquired,
			wonmode.fgIsWiphyLockHeld);

	rWlanStatus = kalIoctl(prGlueInfo, wlanoidSetWonMode,
		(void *) &rSetWON,
		sizeof(struct PARAM_CUSTOM_WON_SET_WITH_LOCK_STRUCT),
		&u4BufLen);

	DBGLOG(WON, INFO,
		"Mode%d: enable=%u, ret = 0x%08x, won reg = %d, resetting = %d\n",
		rSetWON.u4Mode,
		rSetWON.u4Enable,
		(uint32_t) rWlanStatus,
		prGlueInfo->prAdapter->fgIsWONRegistered,
		kalIsResetting());


	/* Need to check fgIsWONRegistered, in case of whole chip reset.
	 * in this case, kalIOCTL return success always,
	 * and prGlueInfo->prWONInfo[0] may be NULL
	 */
	if ((rSetWON.u4Enable)
	    && (prGlueInfo->prAdapter->fgIsWONRegistered)
	    && (kalIsResetting() == FALSE))
		wonNetRegister(prGlueInfo, wonmode.fgIsRtnlLockAcquired);

	return 0;
}

int set_won_mode_handler_wrapper(struct net_device *netdev,
		struct PARAM_CUSTOM_WON_SET_STRUCT wonmode)
{
	struct PARAM_CUSTOM_WON_SET_WITH_LOCK_STRUCT rWonmodeWithLock;
	int ret;

	DBGLOG(WON, TRACE, "set won enable[%d], mode[%d]\n",
		wonmode.u4Enable, wonmode.u4Mode);

	rWonmodeWithLock.u4Enable = wonmode.u4Enable;
	rWonmodeWithLock.u4Mode = wonmode.u4Mode;

	rWonmodeWithLock.fgIsRtnlLockAcquired = TRUE;
	rWonmodeWithLock.fgIsWiphyLockHeld = FALSE;
	rtnl_lock();
	ret = set_won_mode_handler(netdev, rWonmodeWithLock);
	rtnl_unlock();

	return ret;
}

void wlanOnWondertapReg(
	struct GLUE_INFO *prGlueInfo,
	struct ADAPTER *prAdapter,
	struct wireless_dev *prWdev,
	uint8_t fgIsRtnlLockAcquired)
{
	struct PARAM_CUSTOM_WON_SET_WITH_LOCK_STRUCT rSet;

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Enter\n");
#endif

	if (!prAdapter->rWifiVar.u4RegWonIfAtProbe) {
		DBGLOG(WON, INFO, "No Need\n");
		return;
	}

	g_prGlueInfo = prGlueInfo;

	rSet.u4Enable = 1;
	rSet.u4Mode = prAdapter->rWifiVar.ucRegWonMode;
	rSet.fgIsRtnlLockAcquired = fgIsRtnlLockAcquired;
	rSet.fgIsWiphyLockHeld = FALSE;

	if (set_won_mode_handler(
		prWdev->netdev, rSet) == 0)
		DBGLOG(WON, INFO, "Success to register\n");
	else
		DBGLOG(WON, ERROR, "Fail to register\n");

#if CFG_WONDERTAP_TRACE
	DBGLOG(WON, INFO, "Exit\n");
#endif
}

uint32_t
wlanoidSetWonMode(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen)
{
	uint32_t status = WLAN_STATUS_SUCCESS;
	struct PARAM_CUSTOM_WON_SET_WITH_LOCK_STRUCT rSet;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen =
		sizeof(struct PARAM_CUSTOM_WON_SET_WITH_LOCK_STRUCT);
	if (u4SetBufferLen <
		sizeof(struct
		PARAM_CUSTOM_WON_SET_WITH_LOCK_STRUCT)) {
		DBGLOG(WON, WARN, "Invalid length %u\n", u4SetBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	kalMemCopy(&rSet, pvSetBuffer, *pu4SetInfoLen);

	if (rSet.u4Enable) {
		wonSetMode(rSet.u4Mode);

		if (wonLaunch(prAdapter->prGlueInfo)) {
			/* ToDo:: ASSERT */
			ASSERT(prAdapter->fgIsWONRegistered);
		} else {
			DBGLOG(WON, ERROR, "Launch Failed\n");
			status = WLAN_STATUS_FAILURE;
		}

	} else {
		if (prAdapter->fgIsWONRegistered)
			wonRemove(
				prAdapter->prGlueInfo,
				rSet.fgIsRtnlLockAcquired);
	}

	return status;
}

void wlanDestroyWonWdev(struct GLUE_INFO *prGlueInfo)
{
	/* There is only one wiphy, avoid that double free the wiphy */
	struct wiphy *wiphy = NULL;
	struct wireless_dev **pprWonRoleWdev = NULL;
	struct wireless_dev **pprWonWdev = NULL;
	int i = 0;

	if (!prGlueInfo)
		return;

	pprWonRoleWdev = prGlueInfo->prWonRoleWdev;
	pprWonWdev = prGlueInfo->prWonWdev;

	/* free WON wdev */
	for (i = 0; i < KAL_WON_NUM; i++) {
		if (!pprWonRoleWdev || pprWonRoleWdev[i] == NULL)
			continue;
		if (wlanIsAisDev(pprWonRoleWdev[i]->netdev)) {
			/* This is AIS/AP Interface */
			pprWonRoleWdev[i] = NULL;
			continue;
		}

		/* Do wiphy_unregister here. Take care the case that the
		 * pprWonRoleWdev[i] is created by the cfg80211 add iface ops,
		 * And the base WON dev is in the pprWonWdev.
		 * Expect that new created pprWonRoleWdev[i] is freed in
		 * unregister_netdev/mtk_vif_destructor. And pprWonRoleWdev[i]
		 * is reset as pprWonWdev in mtk_vif_destructor.
		 */
		if (pprWonRoleWdev[i] == pprWonWdev[i])
			pprWonWdev[i] = NULL;

		wiphy = pprWonRoleWdev[i]->wiphy;

		kfree(pprWonRoleWdev[i]);
		pprWonRoleWdev[i] = NULL;
	}

	/* This case is that pprWOnWdev isn't equal to pprWonRoleWdev[0]
	 * . The pprWonRoleWdev[0] is created in the won cfg80211 add
	 * iface ops. The two wdev use the same wiphy. Don't double
	 * free the same wiphy.
	 * This part isn't expect occur. Because wonNetUnregister should
	 * unregister_netdev the new created wdev, and pprWonRoleWdev[0]
	 * is reset as pprWOnWdev.
	 */
	for (i = 0; i < KAL_WON_NUM; i++) {
		if (pprWonWdev && pprWonWdev[i] != NULL) {
			wiphy = pprWonWdev[i]->wiphy;
			kfree(pprWonWdev[i]);
			pprWonWdev[i] = NULL;
		}
	}

	g_prGlueInfo = NULL;
}

#endif /* CFG_SUPPORT_WONDERTAP */
