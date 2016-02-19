/*******************************************************************************
    OpenAirInterface
    Copyright(c) 1999 - 2014 Eurecom

    OpenAirInterface is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.


    OpenAirInterface is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OpenAirInterface.The full GNU General Public License is
    included in this distribution in the file called "COPYING". If not,
    see <http://www.gnu.org/licenses/>.

  Contact Information
  OpenAirInterface Admin: openair_admin@eurecom.fr
  OpenAirInterface Tech : openair_tech@eurecom.fr
  OpenAirInterface Dev  : openair4g-devel@lists.eurecom.fr

  Address      : Eurecom, Campus SophiaTech, 450 Route des Chappes, CS 50193 - 06904 Biot Sophia Antipolis cedex, FRANCE

*******************************************************************************/


/*! \file RRC/LITE/defs.h
* \brief RRC struct definitions and function prototypes
* \author Navid Nikaein and Raymond Knopp
* \date 2010 - 2014
* \version 1.0
* \company Eurecom
* \email: navid.nikaein@eurecom.fr, raymond.knopp@eurecom.fr
*/

#ifndef __OPENAIR_RRC_DEFS_H__
#define __OPENAIR_RRC_DEFS_H__

#ifdef USER_MODE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "collection/tree.h"
#include "rrc_types.h"
#include "PHY/defs.h"
#include "COMMON/platform_constants.h"
#include "COMMON/platform_types.h"

#include "COMMON/mac_rrc_primitives.h"
#include "LAYER2/MAC/defs.h"

//#include "COMMON/openair_defs.h"
#ifndef USER_MODE
#include <rtai.h>
#endif

#include "SystemInformationBlockType1.h"
#include "SystemInformation.h"
#include "RRCConnectionReconfiguration.h"
#include "RRCConnectionReconfigurationComplete.h"
#include "RRCConnectionSetup.h"
#include "RRCConnectionSetupComplete.h"
#include "RRCConnectionRequest.h"
#include "RRCConnectionReestablishmentRequest.h"
#include "BCCH-DL-SCH-Message.h"
#include "BCCH-BCH-Message.h"
#ifdef Rel10
#include "MCCH-Message.h"
#include "MBSFNAreaConfiguration-r9.h"
#include "SCellToAddMod-r10.h"
#endif
#include "AS-Config.h"
#include "AS-Context.h"
#include "UE-EUTRA-Capability.h"
#include "MeasResults.h"

// This corrects something generated by asn1c which is different between Rel8 and Rel10
#ifndef Rel10
#define SystemInformation_r8_IEs__sib_TypeAndInfo__Member SystemInformation_r8_IEs_sib_TypeAndInfo_Member
#define SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib2 SystemInformation_r8_IEs_sib_TypeAndInfo_Member_PR_sib2
#define SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib3 SystemInformation_r8_IEs_sib_TypeAndInfo_Member_PR_sib3
#define SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib4 SystemInformation_r8_IEs_sib_TypeAndInfo_Member_PR_sib4
#define SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib5 SystemInformation_r8_IEs_sib_TypeAndInfo_Member_PR_sib5
#define SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib6 SystemInformation_r8_IEs_sib_TypeAndInfo_Member_PR_sib6
#define SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib7 SystemInformation_r8_IEs_sib_TypeAndInfo_Member_PR_sib7
#define SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib8 SystemInformation_r8_IEs_sib_TypeAndInfo_Member_PR_sib8
#define SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib9 SystemInformation_r8_IEs_sib_TypeAndInfo_Member_PR_sib9
#define SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib10 SystemInformation_r8_IEs_sib_TypeAndInfo_Member_PR_sib10
#define SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib11 SystemInformation_r8_IEs_sib_TypeAndInfo_Member_PR_sib11
#endif
/*
#ifdef Rel10
#define SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib12_v920 SystemInformation_r8_IEs_sib_TypeAndInfo_Member_PR_sib12_v920
#define SystemInformation_r8_IEs__sib_TypeAndInfo__Member_PR_sib13_v920 SystemInformation_r8_IEs_sib_TypeAndInfo_Member_PR_sib13_v920
#endif
*/
//#include "L3_rrc_defs.h"
#ifndef NO_RRM
#include "L3_rrc_interface.h"
#include "rrc_rrm_msg.h"
#include "rrc_rrm_interface.h"
#endif

#if defined(ENABLE_ITTI)
# include "intertask_interface.h"
#endif

#if defined(ENABLE_USE_MME)
# include "commonDef.h"
#endif

#if ENABLE_RAL
# include "collection/hashtable/obj_hashtable.h"
#endif

//--------
typedef unsigned int uid_t;
#define UID_LINEAR_ALLOCATOR_BITMAP_SIZE (((NUMBER_OF_UE_MAX/8)/sizeof(unsigned int)) + 1)
typedef struct uid_linear_allocator_s {
  unsigned int   bitmap[UID_LINEAR_ALLOCATOR_BITMAP_SIZE];
} uid_allocator_t;

//--------



#define PROTOCOL_RRC_CTXT_UE_FMT           PROTOCOL_CTXT_FMT
#define PROTOCOL_RRC_CTXT_UE_ARGS(CTXT_Pp) PROTOCOL_CTXT_ARGS(CTXT_Pp)

#define PROTOCOL_RRC_CTXT_FMT           PROTOCOL_CTXT_FMT
#define PROTOCOL_RRC_CTXT_ARGS(CTXT_Pp) PROTOCOL_CTXT_ARGS(CTXT_Pp)
/** @defgroup _rrc RRC 
 * @ingroup _oai2
 * @{
 */

#if ENABLE_RAL
typedef struct rrc_ral_threshold_key_s {
  ral_link_param_type_t   link_param_type;
  ral_threshold_t         threshold;
} rrc_ral_threshold_key_t;
#endif

//#define NUM_PRECONFIGURED_LCHAN (NB_CH_CX*2)  //BCCH, CCCH

#define UE_MODULE_INVALID ((module_id_t) ~0) // FIXME attention! depends on type uint8_t!!!
#define UE_INDEX_INVALID  ((module_id_t) ~0) // FIXME attention! depends on type uint8_t!!! used to be -1

typedef enum UE_STATE_e {
  RRC_INACTIVE=0,
  RRC_IDLE,
  RRC_SI_RECEIVED,
  RRC_CONNECTED,
  RRC_RECONFIGURED,
  RRC_HO_EXECUTION
} UE_STATE_t;

typedef enum HO_STATE_e {
  HO_IDLE=0,
  HO_MEASURMENT,
  HO_PREPARE,
  HO_CMD, // initiated by the src eNB
  HO_COMPLETE // initiated by the target eNB
} HO_STATE_t;

//#define NUMBER_OF_UE_MAX MAX_MOBILES_PER_RG
#define RRM_FREE(p)       if ( (p) != NULL) { free(p) ; p=NULL ; }
#define RRM_MALLOC(t,n)   (t *) malloc16( sizeof(t) * n )
#define RRM_CALLOC(t,n)   (t *) malloc16( sizeof(t) * n)
#define RRM_CALLOC2(t,s)  (t *) malloc16( s )

#define MAX_MEAS_OBJ 6
#define MAX_MEAS_CONFIG 6
#define MAX_MEAS_ID 6

#define PAYLOAD_SIZE_MAX 1024
#define RRC_BUF_SIZE 255
#define UNDEF_SECURITY_MODE 0xff
#define NO_SECURITY_MODE 0x33

#define CBA_OFFSET        0xfff4
// #define NUM_MAX_CBA_GROUP 4 // in the platform_constants

typedef struct UE_RRC_INFO_s {
  UE_STATE_t State;
  uint8_t SIB1systemInfoValueTag;
  uint32_t SIStatus;
  uint32_t SIcnt;
#ifdef Rel10
  uint8_t MCCHStatus[8]; // MAX_MBSFN_AREA
#endif
  uint8_t SIwindowsize; //!< Corresponds to the SIB1 si-WindowLength parameter. The unit is ms. Possible values are (final): 1,2,5,10,15,20,40
  uint8_t handoverTarget;
  HO_STATE_t ho_state;
  uint16_t SIperiod; //!< Corresponds to the SIB1 si-Periodicity parameter (multiplied by 10). Possible values are (final): 80,160,320,640,1280,2560,5120
  unsigned short UE_index;
  uint32_t T300_active;
  uint32_t T300_cnt;
  uint32_t T304_active;
  uint32_t T304_cnt;
  uint32_t T310_active;
  uint32_t T310_cnt;
  uint32_t N310_cnt;
  uint32_t N311_cnt;
  rnti_t   rnti;
} __attribute__ ((__packed__)) UE_RRC_INFO;

typedef struct UE_S_TMSI_s {
  boolean_t  presence;
  mme_code_t mme_code;
  m_tmsi_t   m_tmsi;
} __attribute__ ((__packed__)) UE_S_TMSI;

#if defined(ENABLE_ITTI)
typedef enum e_rab_satus_e {
  E_RAB_STATUS_NEW,
  E_RAB_STATUS_DONE,
  E_RAB_STATUS_FAILED,
} e_rab_status_t;

typedef struct e_rab_param_s {
  e_rab_t param;
  uint8_t status;
} __attribute__ ((__packed__)) e_rab_param_t;
#endif




/* Intermediate structure for Handover management. Associated per-UE in eNB_RRC_INST */
typedef struct HANDOVER_INFO_s {
  uint8_t ho_prepare;
  uint8_t ho_complete;
  uint8_t modid_s; //module_idP of serving cell
  uint8_t modid_t; //module_idP of target cell
  uint8_t ueid_s; //UE index in serving cell
  uint8_t ueid_t; //UE index in target cell
  AS_Config_t as_config; /* these two parameters are taken from 36.331 section 10.2.2: HandoverPreparationInformation-r8-IEs */
  AS_Context_t as_context; /* They are mandatory for HO */
  uint8_t buf[RRC_BUF_SIZE];  /* ASN.1 encoded handoverCommandMessage */
  int size;   /* size of above message in bytes */
} HANDOVER_INFO;

#define RRC_HEADER_SIZE_MAX 64
#define RRC_BUFFER_SIZE_MAX 1024
typedef struct {
  char Payload[RRC_BUFFER_SIZE_MAX];
  char Header[RRC_HEADER_SIZE_MAX];
  char payload_size;
} RRC_BUFFER;
#define RRC_BUFFER_SIZE sizeof(RRC_BUFFER)

typedef struct RB_INFO_s {
  uint16_t Rb_id;  //=Lchan_id
  LCHAN_DESC Lchan_desc[2];
  MAC_MEAS_REQ_ENTRY *Meas_entry;
} RB_INFO;

typedef struct SRB_INFO_s {
  uint16_t Srb_id;  //=Lchan_id
  RRC_BUFFER Rx_buffer;
  RRC_BUFFER Tx_buffer;
  LCHAN_DESC Lchan_desc[2];
  unsigned int Trans_id;
  uint8_t Active;
} SRB_INFO;

typedef struct RB_INFO_TABLE_ENTRY_s {
  RB_INFO Rb_info;
  uint8_t Active;
  uint32_t Next_check_frame;
  uint8_t Status;
} RB_INFO_TABLE_ENTRY;

typedef struct SRB_INFO_TABLE_ENTRY_s {
  SRB_INFO Srb_info;
  uint8_t Active;
  uint8_t Status;
  uint32_t Next_check_frame;
} SRB_INFO_TABLE_ENTRY;

typedef struct MEAS_REPORT_LIST_s {
  MeasId_t measId;
  //CellsTriggeredList  cellsTriggeredList;//OPTIONAL
  uint32_t numberOfReportsSent;
} MEAS_REPORT_LIST;

typedef struct HANDOVER_INFO_UE_s {
  PhysCellId_t targetCellId;
  uint8_t measFlag;
} HANDOVER_INFO_UE;

typedef struct eNB_RRC_UE_s {
  uint8_t                            primaryCC_id;
#ifdef Rel10
  SCellToAddMod_r10_t                sCell_config[2];
#endif
  SRB_ToAddModList_t*                SRB_configList;
  DRB_ToAddModList_t*                DRB_configList;
  uint8_t                            DRB_active[8];
  struct PhysicalConfigDedicated*    physicalConfigDedicated;
  struct SPS_Config*                 sps_Config;
  MeasObjectToAddMod_t*              MeasObj[MAX_MEAS_OBJ];
  struct ReportConfigToAddMod*       ReportConfig[MAX_MEAS_CONFIG];
  struct QuantityConfig*             QuantityConfig;
  struct MeasIdToAddMod*             MeasId[MAX_MEAS_ID];
  MAC_MainConfig_t*                  mac_MainConfig;
  MeasGapConfig_t*                   measGapConfig;
  SRB_INFO                           SI;
  SRB_INFO                           Srb0;
  SRB_INFO_TABLE_ENTRY               Srb1;
  SRB_INFO_TABLE_ENTRY               Srb2;
  MeasConfig_t*                      measConfig;
  HANDOVER_INFO*                     handover_info;

#if defined(ENABLE_SECURITY)
  /* KeNB as derived from KASME received from EPC */
  uint8_t kenb[32];
#endif
  /* Used integrity/ciphering algorithms */
  e_SecurityAlgorithmConfig__cipheringAlgorithm     ciphering_algorithm;
  e_SecurityAlgorithmConfig__integrityProtAlgorithm integrity_algorithm;

  uint8_t                            Status;
  rnti_t                             rnti;
  uint64_t                           random_ue_identity;

#if defined(ENABLE_ITTI)
  /* Information from UE RRC ConnectionRequest */
  UE_S_TMSI                          Initialue_identity_s_TMSI;
  EstablishmentCause_t               establishment_cause;

  /* Information from UE RRC ConnectionReestablishmentRequest */
  ReestablishmentCause_t             reestablishment_cause;

  /* UE id for initial connection to S1AP */
  uint16_t                           ue_initial_id;

  /* Information from S1AP initial_context_setup_req */
  uint32_t                           eNB_ue_s1ap_id :24;

  security_capabilities_t            security_capabilities;

  /* Number of e_rab to be setup in the list */
  uint8_t                            nb_of_e_rabs;
  /* list of e_rab to be setup by RRC layers */
  e_rab_param_t                      e_rab[S1AP_MAX_E_RAB];

  // LG: For GTPV1 TUNNELS
  uint32_t                           enb_gtp_teid[S1AP_MAX_E_RAB];
  transport_layer_addr_t             enb_gtp_addrs[S1AP_MAX_E_RAB];
  rb_id_t                            enb_gtp_ebi[S1AP_MAX_E_RAB];
#endif
  uint32_t                           ul_failure_timer;
} eNB_RRC_UE_t;

typedef uid_t ue_uid_t;

typedef struct rrc_eNB_ue_context_s {
  /* Tree related data */
  RB_ENTRY(rrc_eNB_ue_context_s) entries;

  /* Uniquely identifies the UE between MME and eNB within the eNB.
   * This id is encoded on 24bits.
   */
  rnti_t         ue_id_rnti;

  // another key for protocol layers but should not be used as a key for RB tree
  ue_uid_t       local_uid;

  /* UE id for initial connection to S1AP */
  struct eNB_RRC_UE_s   ue_context;
} rrc_eNB_ue_context_t;

typedef struct {
  uint8_t                           *SIB1;
  uint8_t                           sizeof_SIB1;
  uint8_t                           *SIB23;
  uint8_t                           sizeof_SIB23;
  uint16_t                          physCellId;
  BCCH_BCH_Message_t                mib;
  BCCH_DL_SCH_Message_t             siblock1;
  BCCH_DL_SCH_Message_t             systemInformation;
  //  SystemInformation_t               systemInformation;
  SystemInformationBlockType1_t     *sib1;
  SystemInformationBlockType2_t     *sib2;
  SystemInformationBlockType3_t     *sib3;
#ifdef Rel10
  SystemInformationBlockType13_r9_t *sib13;
  uint8_t                           MBMS_flag;
  uint8_t                           num_mbsfn_sync_area;
  uint8_t                           **MCCH_MESSAGE; //  MAX_MBSFN_AREA
  uint8_t                           sizeof_MCCH_MESSAGE[8];// MAX_MBSFN_AREA
  MCCH_Message_t            mcch;
  MBSFNAreaConfiguration_r9_t       *mcch_message;
  SRB_INFO                          MCCH_MESS[8];// MAX_MBSFN_AREA
#endif
#ifdef CBA
  uint8_t                        num_active_cba_groups;
  uint16_t                       cba_rnti[NUM_MAX_CBA_GROUP];
#endif
  SRB_INFO                          SI;
  SRB_INFO                          Srb0;
} rcc_eNB_carrier_data_t;

typedef struct eNB_RRC_INST_s {
  rcc_eNB_carrier_data_t          carrier[MAX_NUM_CCs];
  uid_allocator_t                    uid_allocator; // for rrc_ue_head
  RB_HEAD(rrc_ue_tree_s, rrc_eNB_ue_context_s)     rrc_ue_head; // ue_context tree key search by rnti
  uint8_t                           HO_flag;
  uint8_t                            Nb_ue;
#if ENABLE_RAL
  obj_hash_table_t                  *ral_meas_thresholds;
#endif
  hash_table_t                      *initial_id2_s1ap_ids; // key is    content is rrc_ue_s1ap_ids_t
  hash_table_t                      *s1ap_id2_s1ap_ids   ; // key is    content is rrc_ue_s1ap_ids_t

#ifdef LOCALIZATION
  /// localization type, 0: power based, 1: time based
  uint8_t loc_type;
  /// epoch timestamp in millisecond, RRC
  int32_t reference_timestamp_ms;
  /// aggregate physical states every n millisecond
  int32_t aggregation_period_ms;
  /// localization list for aggregated measurements from PHY
  struct list loc_list;
#endif
} eNB_RRC_INST;

#define MAX_UE_CAPABILITY_SIZE 255
typedef struct OAI_UECapability_s {
  uint8_t sdu[MAX_UE_CAPABILITY_SIZE];
  uint8_t sdu_size;
  UE_EUTRA_Capability_t *UE_EUTRA_Capability;
} OAI_UECapability_t;

typedef struct UE_RRC_INST_s {
  Rrc_State_t     RrcState;
  Rrc_Sub_State_t RrcSubState;
# if defined(ENABLE_USE_MME)
  plmn_t          plmnID;
  Byte_t          rat;
  as_nas_info_t   initialNasMsg;
# endif
  OAI_UECapability_t *UECap;
  uint8_t *UECapability;
  uint8_t UECapability_size;
  UE_RRC_INFO Info[NB_SIG_CNX_UE];
  SRB_INFO Srb0[NB_SIG_CNX_UE];
  SRB_INFO_TABLE_ENTRY Srb1[NB_CNX_UE];
  SRB_INFO_TABLE_ENTRY Srb2[NB_CNX_UE];
  HANDOVER_INFO_UE HandoverInfoUe;
  uint8_t *SIB1[NB_CNX_UE];
  uint8_t sizeof_SIB1[NB_CNX_UE];
  uint8_t *SI[NB_CNX_UE];
  uint8_t sizeof_SI[NB_CNX_UE];
  uint8_t SIB1Status[NB_CNX_UE];
  uint8_t SIStatus[NB_CNX_UE];
  SystemInformationBlockType1_t *sib1[NB_CNX_UE];
  SystemInformation_t *si[NB_CNX_UE]; //!< Temporary storage for an SI message. Decoding happens in decode_SI().
  SystemInformationBlockType2_t *sib2[NB_CNX_UE];
  SystemInformationBlockType3_t *sib3[NB_CNX_UE];
  SystemInformationBlockType4_t *sib4[NB_CNX_UE];
  SystemInformationBlockType5_t *sib5[NB_CNX_UE];
  SystemInformationBlockType6_t *sib6[NB_CNX_UE];
  SystemInformationBlockType7_t *sib7[NB_CNX_UE];
  SystemInformationBlockType8_t *sib8[NB_CNX_UE];
  SystemInformationBlockType9_t *sib9[NB_CNX_UE];
  SystemInformationBlockType10_t *sib10[NB_CNX_UE];
  SystemInformationBlockType11_t *sib11[NB_CNX_UE];

#ifdef Rel10
  uint8_t                           MBMS_flag;
  uint8_t *MCCH_MESSAGE[NB_CNX_UE];
  uint8_t sizeof_MCCH_MESSAGE[NB_CNX_UE];
  uint8_t MCCH_MESSAGEStatus[NB_CNX_UE];
  MBSFNAreaConfiguration_r9_t       *mcch_message[NB_CNX_UE];
  SystemInformationBlockType12_r9_t *sib12[NB_CNX_UE];
  SystemInformationBlockType13_r9_t *sib13[NB_CNX_UE];
#endif
#ifdef CBA
  uint8_t                         num_active_cba_groups;
  uint16_t                        cba_rnti[NUM_MAX_CBA_GROUP];
#endif
  uint8_t                         num_srb;
  struct SRB_ToAddMod             *SRB1_config[NB_CNX_UE];
  struct SRB_ToAddMod             *SRB2_config[NB_CNX_UE];
  struct DRB_ToAddMod             *DRB_config[NB_CNX_UE][8];
  MeasObjectToAddMod_t            *MeasObj[NB_CNX_UE][MAX_MEAS_OBJ];
  struct ReportConfigToAddMod     *ReportConfig[NB_CNX_UE][MAX_MEAS_CONFIG];
  struct QuantityConfig           *QuantityConfig[NB_CNX_UE];
  struct MeasIdToAddMod           *MeasId[NB_CNX_UE][MAX_MEAS_ID];
  MEAS_REPORT_LIST      *measReportList[NB_CNX_UE][MAX_MEAS_ID];
  uint32_t           measTimer[NB_CNX_UE][MAX_MEAS_ID][6]; // 6 neighboring cells
  RSRP_Range_t                    s_measure;
  struct MeasConfig__speedStatePars *speedStatePars;
  struct PhysicalConfigDedicated  *physicalConfigDedicated[NB_CNX_UE];
  struct SPS_Config               *sps_Config[NB_CNX_UE];
  MAC_MainConfig_t                *mac_MainConfig[NB_CNX_UE];
  MeasGapConfig_t                 *measGapConfig[NB_CNX_UE];
  double                          filter_coeff_rsrp; // [7] ???
  double                          filter_coeff_rsrq; // [7] ???
  float                           rsrp_db[7];
  float                           rsrq_db[7];
  float                           rsrp_db_filtered[7];
  float                           rsrq_db_filtered[7];
#if ENABLE_RAL
  obj_hash_table_t               *ral_meas_thresholds;
  ral_transaction_id_t            scan_transaction_id;
#endif
#if defined(ENABLE_SECURITY)
  /* KeNB as computed from parameters within USIM card */
  uint8_t kenb[32];
#endif

  /* Used integrity/ciphering algorithms */
  e_SecurityAlgorithmConfig__cipheringAlgorithm     ciphering_algorithm;
  e_SecurityAlgorithmConfig__integrityProtAlgorithm integrity_algorithm;
} UE_RRC_INST;

#include "proto.h"

#endif
/** @} */
