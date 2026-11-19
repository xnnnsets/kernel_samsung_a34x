/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 MediaTek Inc.
 */

/*! \file   gl_wondertap.h
 *    \brief  List the external reference to OS for nan GLUE Layer.
 *
 *    In this file we define the data structure - GLUE_INFO_T to store
 *    those objects
 *    we acquired from OS - e.g. TIMER, SPINLOCK, NET DEVICE ... . And all the
 *    external reference (header file, extern func() ..) to OS for GLUE Layer
 *    should also list down here.
 */
#include <linux/auxiliary_bus.h>

#ifndef _GL_WONDERTAP_H
#define _GL_WONDERTAP_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */
#if CFG_SUPPORT_WONDERTAP

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   V A R I A B L E
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define CFG_SUPPORT_WONDERTAP_UT (1)

#define CFG_WONDERTAP_TRACE (1)
#define CFG_WONDERTAP_FIX_RATE (1)
#define CFG_WONDERTAP_AUTO_ADD_STA (1)

#define WON_INF_NAME "wondertap%d"

#define WONDERAP_VERSION 20251129

#define CAPS_RATE_ADAPT 1
/* [v1.2] STA + WONDERTAP: DBDC */
/* [v2.0] STA + WONDERTAP: MCC */
#define CAPS_STA_COEXIST 1
#define CAPS_SAP_COEXIST 0
#define CAPS_P2P_COEXIST 0
#define CAPS_NAN_COEXIST 0
#define CAPS_RANGING_COEXIST 0
#define CAPS_AMSDU_AGG 0
#define CAPS_AMPDU_AGG 0
#define CAPS_DYNAMIC_FREQ 1
#define CAPS_DYNAMIC_RATE 0
#define CAPS_CUSTOM_MGMT_RETRY 0
#define CAPS_CUSTOM_DATA_RETRY 0
#define CAPS_FRAME_FILTER 0

#define WONDERTAP_STATUS_OK (0)
#define WONDERTAP_STATUS_FAIL (1)

#define RUNNING_WON_MODE 0
#define RUNNING_WON_MODE1 1
#define RUNNING_WON_MODE2 2
#define RUNNING_WON_AP_MODE 3
#define RUNNING_DUAL_WON_MODE 4
#define RUNNING_WON_DEV_MODE 5
#define RUNNING_WON_NO_GROUP_MODE 6
#define RUNNING_WON_MODE_NUM 7

enum ENUM_WON_REG_STATE {
	ENUM_WON_REG_STATE_UNREGISTERED,
	ENUM_WON_REG_STATE_REGISTERING,
	ENUM_WON_REG_STATE_REGISTERED,
	ENUM_WON_REG_STATE_UNREGISTERING,
	ENUM_WON_REG_STATE_NUM
};

enum ENUM_WON_DEV_STATE {
	WON_DEV_STATE_IDLE = 0,
	WON_DEV_STATE_SCAN,
	WON_DEV_STATE_REQING_CHANNEL,
	WON_DEV_STATE_CHNL_ON_HAND,
	WON_DEV_STATE_OFF_CHNL_TX,
	WON_DEV_STATE_LISTEN_OFFLOAD,
	/* Requesting Channel to Send Specific Frame. */
	WON_DEV_STATE_NUM
};

enum ENUM_WON_ROLE_STATE {
	WON_ROLE_STATE_IDLE = 0,
	WON_ROLE_STATE_SCAN,
	WON_ROLE_STATE_REQING_CHANNEL,
	WON_ROLE_STATE_CHNL_ON_HAND,
	WON_ROLE_STATE_OFF_CHNL_TX,
	WON_ROLE_STATE_LISTEN_OFFLOAD,
	WON_ROLE_STATE_NORMAL_TR,
	WON_ROLE_STATE_NUM
};

enum ENUM_WON_MSG {
	WON_MSG_TX_DATA = 0,
	WON_MSG_TX_MGMT,
	WON_MSG_NUM
};

/** @struct MSG_WON_TX
 *  (MSG) This struct represents a message for won mgmt tx.
 */
struct MSG_WON_TX {
	/** Header structure for the message */
	struct MSG_HDR rMsgHdr;	/* Must be the first member */

	/** Index of the BSS */
	uint8_t ucBssIndex;

	enum ENUM_WON_MSG eIsDataMgmtFrame;

	uint8_t addr1[ETH_ALEN];
	struct MSDU_INFO *prMsduInfo;
	uint16_t u2EstimatedFrameLen;

};

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

#if CFG_SUPPORT_WONDERTAP_UT
/*
 * @brief Per-packet transmission descriptor.
 *
 * This structure is intended to be stored in the `sk_buff->cb` control buffer
 * to provide per-packet transmission instructions to the underlying driver.
 */
struct wonderap_txd {
	/** @brief True if the frame is a unicast transmission. */
	bool is_unicast;
	/** @brief The 802.11 frame type (e.g., IEEE80211_FTYPE_DATA). */
	u8 frame_type;
	/** @brief The Traffic Identifier (TID) for QoS. */
	u8 tid;
};
/* limit the wonder_txd size less than skb->cb */
static_assert(sizeof(struct wonderap_txd) <= 48,
	"wonder_txd too large for skb->cb");

/** @brief Supported hardware/software features. */
struct wondertap_capability {
	u32 version;
	union {
		/* @brief All capability flags as a single 32-bit word. */
		u32 raw_bits;
		/* @brief Access to individual capability bits. */
		struct {
			/* @brief Dynamic rate adaptation is supported. */
			u32 rate_adaptation: 1;
			/* @brief STA (Station) coexistence is supported. */
			u32 sta_coexist: 1;
			/* @brief SAP (Soft AP) coexistence is supported. */
			u32 sap_coexist: 1;
			/* @brief P2P (Wi-Fi Direct) coexistence is
			 * supported.
			 */
			u32 p2p_coexist: 1;
			/* @brief NAN (Neighbor Awareness Networking)
			 * coexistence is supported.
			 */
			u32 nan_coexist: 1;
			/* @brief Ranging coexistence is supported. */
			u32 ranging_coexist: 1;
			/* @brief A-MSDU aggregation is supported. */
			u32 amsdu_aggregation: 1;
			/* @brief A-MPDU aggregation is supported. */
			u32 ampdu_aggregation: 1;
			/* @brief Dynamic frequency/channel changes are
			 * supported.
			 */
			u32 dynamic_freq: 1;
			/* @brief Dynamic setting a fixed TX rate is
			 * supported.
			 */
			u32 dynamic_fixed_tx_rate: 1;
			/* @brief Setting custom management frame retry
			 * limits is supported.
			 */
			u32 custom_mgmt_retry_limit: 1;
			/* @brief Setting custom data frame retry limits
			 * is supported.
			 */
			u32 custom_data_retry_limit: 1;
			/* @brief Frame type filtering is supported. */
			u32 frame_type_filter: 1;
			/* @brief Reserved for future use. Must be 0. */
			u32 reserved: 19;
		} bits;
	};
};

/** @brief Defines the PHY preamble/protocol for the TX rate. */
enum wondertap_rate_preamble {
	/* 802.11a/g rates (non-HT) */
	WONDERTAP_RATE_PREAMBLE_LEGACY = 0,
	/* 802.11n High Throughput */
	WONDERTAP_RATE_PREAMBLE_HT = 1,
	/* 802.11ac Very High Throughput */
	WONDERTAP_RATE_PREAMBLE_VHT = 2,
	/* 802.11ax High Efficiency */
	WONDERTAP_RATE_PREAMBLE_HE = 3,
	/* 802.11be Extremely High Throughput */
	WONDERTAP_RATE_PREAMBLE_EHT = 4,
};

/** @brief Defines the channel bandwidth. */
enum wondertap_rate_bw {
	WONDERTAP_RATE_BW_20 = 0,
	WONDERTAP_RATE_BW_40 = 1,
	WONDERTAP_RATE_BW_80 = 2,
	WONDERTAP_RATE_BW_160 = 3,
	WONDERTAP_RATE_BW_320 = 4,
};

/** @brief Channel and bandwidth configuration. */
struct wondertap_set_freq_params {
	u32 freq;
	enum wondertap_rate_bw bandwidth;
};

/** @brief Defines the Guard Interval (GI). */
enum wondertap_rate_gi {
	/* Driver uses default (e.g., Long GI or 0.8us) */
	WONDERTAP_RATE_GI_DEFAULT = 0,
	/* Short GI for HT/VHT */
	WONDERTAP_RATE_GI_SHORT = 1,
	/* Specific HE GI values */
	WONDERTAP_RATE_GI_0_8_US = 2,
	WONDERTAP_RATE_GI_1_6_US = 3,
	WONDERTAP_RATE_GI_3_2_US = 4,
};

/**
 * @brief Parameters to configure a specific TX rate.
 *
 * This structure is used to define a complete transmission rate,
 * including its PHY characteristics and special features.
 */
struct wondertap_fixed_tx_rate_params {
	/** * @brief The preamble/PHY type for this rate. */
	enum wondertap_rate_preamble preamble;
	/** * @brief The channel bandwidth for this rate. */
	enum wondertap_rate_bw bw;
	/** * @brief The Guard Interval (GI) for this rate. */
	enum wondertap_rate_gi gi;
	/**
	 * @brief The number of spatial streams (NSS).
	 * Typically 1-4 for client devices. 0 is invalid.
	 */
	u8 nss;
	/**
	 * @brief The Modulation and Coding Scheme (MCS) index.
	 * - For HT (802.11n): 0-7 (up to 31 for 4 streams).
	 * - For VHT (802.11ac): 0-9.
	 * - For HE (802.11ax): 0-11.
	 * - For Legacy: This field is interpreted as the legacy rate index
	 * (e.g., index for 54 Mbps, 48 Mbps, etc.). Ignored by some drivers.
	 */
	u8 mcs;
	/** @brief Reserved for future use. */
	u8 reserved[2];
};

/**
 * @brief Enumeration for the different types of hardware filters supported.
 *
 * This enum is used in the `wondertap_set_filter` function to specify which
 * filter is being configured.
 */
enum wondertap_filter_type {
	/**
	 * @brief Configures a filter based on the 802.11 frame's type
	 *        and subtype.
	 */
	WONDERTAP_FILTER_TYPE_FRAME,
};

/**
 * @brief Parameters for configuring the frame type/subtype filter.
 */
struct wondertap_frame_filter_params {
	/**
	 * @brief Set to 'true' to enable the filter, 'false' to disable it.
	 */
	bool enabled;
	/**
	 * @brief The 802.11 frame type to match.
	 * This field is ignored if 'enabled' is false.
	 *
	 * The frame type defines the major category of the frame.
	 * Use standard Linux kernel definitions from <linux/ieee80211.h>:
	 * - IEEE80211_FTYPE_MGMT (0x0000): Management frames
	 * - IEEE80211_FTYPE_CTRL (0x0004): Control frames
	 * - IEEE80211_FTYPE_DATA (0x0008): Data frames
	 */
	u16 frame_type;
	/**
	 * @brief The 802.11 frame subtype to match.
	 * This field is ignored if 'enabled' is false.
	 *
	 * The subtype specifies the frame's exact purpose within its category.
	 * Use standard Linux kernel definitions from <linux/ieee80211.h>:
	 * - e.g., IEEE80211_STYPE_BEACON (0x0080)
	 * - e.g., IEEE80211_STYPE_PROBE_REQ (0x0040)
	 * - e.g., IEEE80211_STYPE_QOS_DATA (0x0080)
	 */
	u32 frame_subtype;
};

#define WONDERTAP_VHT_NSS_MAX 8
#define WONDERTAP_HE_NSS_MAX 8
#define WONDERTAP_EHT_NSS_MAX 8
#define WONDERTAP_HT_NSS_MAX 4

/**
 * @brief Defines the PHY technology for a transmission rate.
 *
 * This enumeration is used to categorize different generations of Wi-Fi
 * technologies, primarily within the `wondertap_tx_rate_mask_params`
 * structure to specify which rate masks are being configured.
 */
enum wondertap_tx_rate {
	/** @brief Legacy rates (802.11a/b/g). */
	WONDERTAP_RATE_LEGACY,
	/** @brief High Throughput rates (802.11n). */
	WONDERTAP_RATE_HT,
	/** @brief Very High Throughput rates (802.11ac). */
	WONDERTAP_RATE_VHT,
	/** @brief High Efficiency rates (802.11ax). */
	WONDERTAP_RATE_HE,
	/** @brief Extremely High Throughput rates (802.11be). */
	WONDERTAP_RATE_EHT,
	/** @brief The total number of rate categories. Not a valid
	 *         rate type.
	 */
	WONDERTAP_RATE_MAX,
};

/**
 * @brief Defines which TX rate masks are active in a `wondertap_tx_rate_mask`
 * configuration.
 */
enum wondertap_tx_rate_mask_enable {
	/**
	 * @brief If set, the legacy_rates bitmap is valid and should be
	 * applied.
	 */
	WONDERTAP_RATEMASK_EN_LEGACY = BIT(WONDERTAP_RATE_LEGACY),
	/**
	 * @brief If set, the ht_mcs bitmap is valid and should be applied.
	 */
	WONDERTAP_RATEMASK_EN_HT = BIT(WONDERTAP_RATE_HT),
	/**
	 * @brief If set, the vht_mcs bitmap is valid and should be applied.
	 */
	WONDERTAP_RATEMASK_EN_VHT = BIT(WONDERTAP_RATE_VHT),
	/**
	 * @brief If set, the he_mcs bitmap is valid and should be applied.
	 */
	WONDERTAP_RATEMASK_EN_HE = BIT(WONDERTAP_RATE_HE),
	/**
	 * @brief If set, the eht_mcs bitmap is valid and should be applied.
	 */
	WONDERTAP_RATEMASK_EN_EHT = BIT(WONDERTAP_RATE_EHT),
};

/**
 * @brief A unified structure to define
 * the permitted transmission rates for
 * the rate control algorithm.
 */
struct wondertap_tx_rate_mask_params {
	/**
	 * @brief A bitmask from `enum wondertap_tx_rate_mask_enable` that
	 * specifies which of the rate masks in this structure are valid and
	 * should be applied by the driver.
	 */
	u32 enable_mask;
	/**
	 * @brief A bitmap of permitted legacy (802.11a/g) rates.
	 * The bits correspond to the driver's internal legacy rate indices.
	 * This field is only valid if WONDERTAP_RATEMASK_EN_LEGACY is set.
	 */
	u32 legacy_rates;
	u16 ht_mcs[WONDERTAP_HT_NSS_MAX];
	u16 vht_mcs[WONDERTAP_VHT_NSS_MAX];
	u16 he_mcs[WONDERTAP_HE_NSS_MAX];
	u16 eht_mcs[WONDERTAP_EHT_NSS_MAX];
};

/** @brief Wonder interface initialization parameters. */
struct wondertap_init_params {
	/**
	 * @brief The initial channel and frequency for the interface.
	 */
	struct wondertap_set_freq_params channel;
	/**
	 * @brief The default fixed transmission rate.
	 */
	struct wondertap_fixed_tx_rate_params tx_rate;
	/**
	 * @brief The MAC address for wondertap0 interface.
	 */
	u8 mac_addr[ETH_ALEN];
	/** * @brief The BSSID to filter. */
	u8 bssid[ETH_ALEN];
	/**
	 * @brief Max retransmission attempts for management frames.
	 *
	 * This value controls the retry behavior for the packet at the hardware
	 * level. The interpretation is as follows:
	 * - 0: The frame will be transmitted once with no retries.
	 * - 1-254: The frame will be re-transmitted up to this many times if no
	 * acknowledgment is received.
	 * - 255: The hardware will use its an unlimited number of retries.
	 */
	u8 mgmt_retry_limit;
	/**
	 * @brief Max retransmission attempts for data frames.
	 *
	 * This value controls the retry behavior for the packet at the hardware
	 * level. The interpretation is as follows:
	 * - 0: The frame will be transmitted once with no retries.
	 * - 1-254: The frame will be re-transmitted up to this many times if no
	 * acknowledgment is received.
	 * - 255: The hardware will use its an unlimited number of retries.
	 */
	u8 data_retry_limit;
	/** * @brief Aggregation feature control */
	u8 amsdu_enable:1;
	u8 ampdu_enable:1;
	/**
	 * @brief Reserved for future use and alignment.
	 */
	u8 reserved1:6;
	u8 reserved2;
	/**
	 * @brief The two-letter ISO 3166 country code (e.g., "US", "TW").
	 *
	 * @note Includes the null terminator (\0), hence the size of 3.
	 */
	char country_code[3];
	u8 reserved3;
};

/** @brief Deinitialization parameters passed
 * from the core to the vendor driver.
 */
struct wondertap_deinit_params {
	/**
	 * @brief The two-letter ISO 3166 country code (e.g., "US", "TW").
	 * @note Includes the null terminator (\0), hence the size of 3.
	 */
	char country_code[3];
	u8 reserved1;
};

/** @brief Vendor Driver Operations structure that must be implemented. */
struct wondertap_ops {
/**
 * @brief Initializes the wondertap0 interface.
 *
 * This function serves as the entry point to initialize the wondertap0.
 * It finds the registered vendor operations and invokes the vendor-specific
 * init() callback to allocate and prepare the wondertap0 interface.
 *
 * @handle: A pointer to a void pointer (`void **`) that will be populated with
 * the opaque handle of the newly created driver instance upon successful
 * return.
 * @params: A pointer to the initialization parameters required by the vendor
 * driver.
 *
 * Return: 0 on success, or a negative errno code on failure.
 */
	int (*init)(void **handle,
		const struct wondertap_init_params *params);
/**
 * @brief Deinitializes the wondertap0 interface.
 *
 * This function tears down a driver instance, calling the vendor-specific
 * deinit() callback to free all allocated resources and power down the
 * wondertap0 interface.
 *
 * @handle: The opaque driver instance handle that was obtained from a
 * successful call to wondertap_init().
 * @params: A pointer to the deinitialization parameters required by the vendor
 * driver.
 */
	void (*deinit)(void *handle,
		const struct wondertap_deinit_params *params);
/**
 * @brief Sets the hardware operating channel and bandwidth.
 *
 * This function calls the vendor-specific set_freq() callback to configure
 * the radio to operate on a specific channel with a given bandwidth.
 *
 * @handle: The opaque driver instance handle.
 * @params: A pointer to the channel and bandwidth configuration.
 *
 * Return: 0 on success, or a negative errno code on failure.
 */
	int (*set_freq)(void *handle,
		const struct wondertap_set_freq_params *params);
/**
 * @brief Configures a specific hardware packet filter.
 *
 * This is a versatile function that dispatches the configuration to the
 * appropriate hardware filter based on the @filter_type. The caller must
 * provide a pointer to a parameter structure that corresponds to the
 * specified filter type.
 *
 * @param handle The opaque driver instance handle.
 * @param filter_type The type of filter to configure, as defined in
 * `enum wondertap_filter_type`.
 * @param params A void pointer to the filter-specific param
 * - For WONDERTAP_FILTER_TYPE_FRAME: This must be a pointer to
 * `struct wondertap_frame_filter_params`.
 *
 * @return: 0 on success, or a negative errno code on failure.
 */
	int (*set_filter)(void *handle,
		enum wondertap_filter_type type,
		const void *params);
/**
 * @brief Sets a fixed transmission rate for the hardware.
 *
 * This function calls the vendor-specific set_tx_rate()
 * callback to force
 * the hardware to use a specific, fixed rate for transmissions.
 *
 * @handle: The opaque driver instance handle.
 * @params: A pointer to the desired transmission
 *          rate parameters (MCS, NSS, GI, etc.).
 *
 * Return: 0 on success, or a negative errno code on failure.
 */
	int (*set_fixed_tx_rate)(void *handle,
		const struct wondertap_fixed_tx_rate_params *params);
/**
 * @brief Configures a mask of permitted transmission rates for
 * the automatic rate control algorithm.
 *
 * @param handle The opaque driver instance handle.
 * @param params A pointer to the rate mask structure defining the
 *               permitted rates.
 *
 * Return: 0 on success, or a negative errno code on failure.
 */
	int (*set_tx_rate_mask)(void *handle,
		const struct wondertap_tx_rate_mask_params *params);
/**
 * @brief Retrieves supported vendor features.
 *
 * This function calls the vendor-specific get_capability()
 * callback to query
 * the hardware and driver for its capabilities.
 *
 * @handle: The opaque driver instance handle.
 * @features: A pointer to a struct wondertap_features
 * that will be populated
 * with the feature flags supported by the vendor.
 *
 * Return: 0 on success, or a negative errno code on failure.
 */
	int (*get_capabilities)(void *handle,
		struct wondertap_capability *capabilities);
};

/**
 * @brief Enumeration of WonderTap interface versions.
 *
 * This enum defines the supported versions of the WonderTap interface.
 */
enum wondertap_ver {
	WONDER_VERSION_1_0,
	WONDER_VERSION_1_1,
	WONDER_VERSION_1_2,
	WONDER_VERSION_1_3,
	WONDER_VERSION_1_4,
	WONDER_VERSION_1_4_1,
	WONDER_VERSION_1_5,
	WONDER_VERSION_MAX,
};

struct wondertap_aux_dev {
	/** @brief The base auxiliary device structure. */
	struct auxiliary_device adev;
	/** @brief The version of the WonderTap interface being used. */
	enum wondertap_ver ver;
	/** @brief Pointer to the vendor-specific operations table. */
	const struct wondertap_ops *wonder_ops;
};
#endif /* CFG_SUPPORT_WONDERTAP_UT */

#define wonChangeMediaState(_prAdapter, _prWonBssInfo, _eNewMediaState) \
	(_prWonBssInfo->eConnectionState = (_eNewMediaState))

struct PARAM_CUSTOM_WON_SET_STRUCT {
	uint32_t u4Enable;
	uint32_t u4Mode;
};

struct PARAM_CUSTOM_WON_SET_WITH_LOCK_STRUCT {
	uint32_t u4Enable;
	uint32_t u4Mode;
	uint8_t fgIsRtnlLockAcquired;
	u_int8_t fgIsWiphyLockHeld;
};

struct GL_WON_DEV_INFO {
	uint64_t u8Cookie;
	uint32_t u4OsMgmtFrameFilter;
	uint32_t u4PacketFilter;
};

struct WON_DEV_INFO {
	uint32_t u4DeviceNum;
};

struct WON_CHNL_REQ_INFO {
	u_int8_t fgIsChannelRequested;
	uint8_t ucSeqNumOfChReq;
	uint64_t u8Cookie;
	uint8_t ucReqChnlNum;
	enum ENUM_BAND eBand;
	enum ENUM_CHNL_EXT eChnlSco;
	uint8_t ucOriChnlNum;
	enum ENUM_CHANNEL_WIDTH eChannelWidth;	/*VHT operation ie */
	uint8_t ucCenterFreqS1;
	uint8_t ucCenterFreqS2;
	enum ENUM_BAND eOriBand;
	enum ENUM_CHNL_EXT eOriChnlSco;
	uint32_t u4MaxInterval;
	enum ENUM_CH_REQ_TYPE eChnlReqType;
	uint8_t ucChReqNum;
};

struct WON_DEV_FSM_INFO {
	uint8_t ucBssIndex;
	/* State related. */
	enum ENUM_WON_DEV_STATE eCurrentState;

	uint8_t ucReqChannelNum;
	enum ENUM_BAND eReqBand;

	/* Packet filter for WON module. */
	uint32_t u4WonPacketFilter;

	u_int8_t fgInitialied;
};

struct WON_ROLE_FSM_INFO {
	uint8_t ucRoleIndex;

	uint8_t ucBssIndex;

	enum ENUM_WON_ROLE_STATE eCurrentState;

	struct WON_CHNL_REQ_INFO rChnlReqInfo;

	/* Packet filter for WON module. */
	uint32_t u4WonPacketFilter;

	struct TIMER rWonFsmTimeoutTimer;
};

struct GL_WON_INFO {
	struct net_device *prDevHandler;
	struct net_device *aprRoleHandler;
	struct wireless_dev *prWdev;
	int32_t i4Generation;
	uint8_t ucRole;
	uint32_t u4CipherPairwise;
	u_int8_t fgChannelSwitchReq;
	uint32_t u4LinkId;
	struct completion rDisconnComp;

	/* Connection info */
	struct wondertap_set_freq_params channel;
	struct wondertap_fixed_tx_rate_params tx_rate;
};

struct CMD_PEER_WON_UPDATE {
	uint8_t ucBssIdx;
	enum ENUM_STA_TYPE eStaType;
	uint8_t aucPeerMac[6];
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

#define WON_ROLE_INDEX_2_ROLE_FSM_INFO(_prAdapter, _RoleIndex) \
	((_prAdapter)->rWifiVar.aprWonRoleFsmInfo[_RoleIndex])

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */


/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

#if CFG_SUPPORT_WONDERTAP_UT
/**
 * @brief Register a vendor's wondertap operations.
 *
 * @ops: A pointer to the vendor's statically defined wondertap_ops structure.
 * This pointer must remain valid until wondertap_unregister_ops() is
 * called.
 *
 * Return:
 * * 0 on success.
 * @note Only one vendor implementation can be registered at a time.
 */
extern int wondertap_register_ops(struct wondertap_ops *ops);
/**
 * @brief Unregister a vendor's wondertap operations.
 *
 * @ops: The exact same pointer to the wondertap_ops structure that was
 * previously passed to wondertap_register_ops(). The unregistration
 * will only proceed if this pointer matches the currently active one.
 */
extern int wondertap_unregister_ops(struct wondertap_ops *ops);
#endif /* CFG_SUPPORT_WONDERTAP_UT */

#if 0
extern void wondertap_rx_frame(struct net_device *dev,
	const u8 *data, size_t len);
extern void wondertap_trigger_reset(struct net_device *dev);
#endif

void wlanOnWondertapReg(
	struct GLUE_INFO *prGlueInfo,
	struct ADAPTER *prAdapter,
	struct wireless_dev *prWdev,
	uint8_t fgIsRtnlLockAcquired);

uint32_t
wlanoidSetWonMode(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

u_int8_t glWonCreateWirelessDevice(struct GLUE_INFO *prGlueInfo);

u_int8_t wonNetUnregister(
	struct GLUE_INFO *prGlueInfo,
	uint8_t fgIsRtnlLockAcquired,
	u_int8_t fgIsWiphyLockHeld);

u_int8_t wonRemove(
	struct GLUE_INFO *prGlueInfo,
	uint8_t fgIsRtnlLockAcquired);

void wlanDestroyWonWdev(struct GLUE_INFO *prGlueInfo);

uint32_t wondertapSetCountryCode(
	struct ADAPTER *prAdapter,
	const uint8_t *country_code);

uint32_t wondertapSetFixRateByBss(
	struct ADAPTER *prAdapter,
	u_int8_t fgIsOid,
	uint8_t ucBssIndex);

int32_t procRemoveFsWondertap(
	struct proc_dir_entry *prProcRoot);

int32_t procCreateFsWondertap(
	struct GLUE_INFO *prGlueInfo,
	struct proc_dir_entry *prProcRoot);

void wonFsmStateTransition(struct ADAPTER *prAdapter,
		struct WON_ROLE_FSM_INFO *prWonRoleFsmInfo,
		enum ENUM_WON_ROLE_STATE eNextState);

uint32_t wonPeerRemoveAll(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

uint32_t wonPeerAdd(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t *mac,
	uint8_t fgIsOid);

void
scanWonProcessBeaconAndProbeResp(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb,
	uint32_t *prStatus,
	struct BSS_DESC *prBssDesc,
	struct WLAN_BEACON_FRAME *prWlanBeaconFrame);

uint32_t wonRxIndicateOnePkt(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb, uint16_t ucBssIndex);

void wonRxProcessActionFrame(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb);

void wonChGrant(struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr);
void wonTxDataMgmt(struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr);

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

#endif /* CFG_SUPPORT_WONDERTAP */
#endif /* _GL_WONDERTAP_H */
