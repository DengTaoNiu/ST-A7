/**
 * @file atbui.h
 * @author your name (you@domain.com)
 * @brief 此文件是sdk的上位机交互文件，具体参考《sdk上位机软件初稿.docx》
 * @version 0.1
 * @date 2021-11-09
 *
 * @copyright Copyright (c) 2021
 *
 */
#ifndef _ATB_UI_H
#define _ATB_UI_H

#include <stdio.h>
#include <stdint.h>
#include "resmgr.h"

#define MAC_LEN 6
#define NAME_LEN 32
#define VER_LEN 16

#define NODE_CFG_CNT 64

typedef enum
{
    SET_ATB_CFG = 0X10,
    SET_ATB_CFG_REPLY,
    GET_ATB_CFG,
    GET_ATB_CFG_REPLY,
    SET_NODE_NAME,
    SET_NODE_NAME_REPLY,
    GET_NODE_INFO,
    GET_NODE_INFO_REPLY,
    GET_RES_CONFIG,
    GET_RES_CONFIG_REPLY,
    SET_RES_CONFIG,
    SET_RES_CONFIG_REPLY,
    GET_NODE_BASIC_INFO, 
    GET_NODE_BASIC_INFO_REPLY,
    GET_ATB_SIG,
    GET_ATB_SIG_REPLY,
    GET_BPS,
    GET_BPS_REPLY,
    GET_DEBUG,
    GET_DEBUG_REPLY,
    SET_DEBUG,
    SET_DEBUG_REPLY,

    OTA_START,
    OTA_START_REPLY,
    OTA_DATA,
    OTA_DATA_REPLY,
    OTA_END,
    OTA_END_REPLY,

    GET_DEVICE,
    GET_DEVICE_REPLY,
    SET_DEVICE,
    SET_DEVICE_REPLY,
} E_MSG_TYPE;

#pragma pack(push, 1)

typedef struct
{
    uint8_t msp;
    uint8_t dmdb;
    uint8_t umdb;
    uint8_t power;
    uint8_t txpga;
    uint8_t bandsel;
    uint8_t role;
    uint8_t dynband;
    uint32_t crc;
} atbcfg_t;

typedef enum
{
    E_CFG_RS4850,
    E_CFG_RS4851,
    E_CFG_RS4852,
    E_CFG_CAN0,
    E_CFG_CAN1,
    E_CFG_CAN2,
    E_CFG_IOM,
    E_CFG_NODE,
    E_CFG_REBOOT = 36,
    E_CFG_LED_ON,
    E_CFG_LED_OFF,
} E_CFG_TYPE; 

typedef struct
{
    uint32_t baudrate;
    uint32_t databit;
    uint32_t stopbit;
    uint32_t paritybit;
} rs485cfg_t;

typedef enum
{
    E_IOM_SWITCH = 1,
    E_IOM_VOL,
    E_IOM_CUR,
    E_IOM_RESIS,
} E_IOM_TYPE;

typedef enum
{
    E_RES_NI1000 = 1,
    E_RES_PT1000,
} E_RESIS_TYPE;

typedef struct
{
    uint16_t iomtype;
    uint16_t restype;
    uint16_t min;
    uint16_t max;
    int32_t zero;
} uiuocfg_t;

typedef struct
{
    uint8_t name[NAME_LEN];
    uint32_t addr;
    /**
     * @brief 是否监听autbus
     *
     */
    int32_t listenautbus;
} nodeusercfg_t;

typedef struct
{
    uint32_t baudrate;
} cancfg_t;
typedef struct
{
    rs485cfg_t rs485cfg[3];
    cancfg_t cancfg[3];
    uiuocfg_t uiuocfg[12];
    nodeusercfg_t nodecfg;
    uint32_t crc;
} devcfg_t;

/**
 * @brief 节点信息，此结构体下位机上传给上位机的数据结构
 *
 */
typedef struct
{
    /*节点运行时间，cn保存节点上线时间，这个时间是系统当前时间与节点上线时间差值，尽量减少tn实时发送的带宽消耗*/
    uint64_t runtime;
    uint8_t mac[MAC_LEN];
    uint8_t name[NAME_LEN];
    /*acpu软件版本号*/
    uint8_t aver[VER_LEN];
    /*bcpu软件版本号*/
    uint8_t bver[VER_LEN];
    uint8_t nodetype[VER_LEN];
    uint8_t nodeaddr;
    uint8_t listen;
    uint8_t role;
    uint8_t nid;
    uint8_t state;
    uint8_t lostcnt;
    uint8_t sigerr;
    uint8_t report;
} nodeinfo_t;

typedef struct
{
    uint32_t nodecnt;
    nodeinfo_t pnodecfg[0];
} atb_nodeinfo_t;

/**
 * @brief 获取最基本的节点信息，此结构体非特殊情况不改变
 *       为实现兼容性而设置
 *
 */
typedef struct
{
    uint64_t runtime;
    uint8_t mac[MAC_LEN];
    uint8_t name[NAME_LEN];
    /*acpu软件版本号*/
    uint8_t aver[VER_LEN];
    /*bcpu软件版本号*/
    uint8_t bver[VER_LEN];
    uint8_t nodetype[VER_LEN];
    uint8_t nodeaddr;
    uint8_t role;
    uint8_t nid;
    uint8_t state;

} basic_nodeinfo_t;

typedef struct
{
    uint32_t nodecnt;
    basic_nodeinfo_t pnodecfg[0];
} basicnode_t; 

typedef struct
{
    uint64_t cb_total;
    uint64_t cberr_dn;
    uint64_t cberr_up;
    uint16_t amdn;
    uint16_t amup;
    uint16_t nhddn;
    uint16_t nhdup;
    uint8_t nhdvdn;
    uint8_t nhdvup;
    uint8_t snrdn;
    uint8_t snrup;
    int8_t agc;
    uint8_t nodeid;
} atbsignal_t;

#pragma pack(pop)

typedef enum
{
    E_UI_CMD_NONE = 3,
    /*获取信道质量*/
    E_GET_ATB_SIG,
    /*实时更新发送增益*/
    E_UPDATE_TXGAIN,
    /*设置调试数据*/
    E_SET_DEBUG_DATA,
    /*获取流量统计数据*/
    E_GET_BPS_DATA,
    /*查看调试数据*/
    E_GET_DEBUG_DATA,
    /*查看设备参数*/
    E_SET_DEVICE_DATA,
    /*修改设备参数*/
    E_GET_DEVICE_DATA,
    /*ota升级*/
    E_OTA_START,
    E_OTA_DATA,
    E_OTA_END,
    E_SET_DYN,
} E_UI_CMD;

#define APP_FLASH_UPGRADE_ADDR 0X100000
#define APP_FLASH_UPGRADE_SIZE (1024 * 256)

#define UI_BUF_SIZE 1200
#define DEBUG_BUF_SIZE 256
#define DEV_SET_SIZE 256

/* 1M + 832K . */
// #define CFG_DEBUG_OFFSET 0x1C0000
// #define CFG_RES_OFFSET 0x1D0000
// #define CFG_ATB_OFFSET 0x1E0000
// #define CFG_NODE_OFFSET 0x1F0000
// #define CFG_DEVICE_OFFSET 0x200000
// #define PRE_RES_OFFSET      0x2A0000
#if (INCLUDE_NandFlash == 0)
    /* 1M + 768K */
    #define CFG_DEBUG_OFFSET    0x1C0000
    #define CFG_RES_OFFSET      0x1E0000
    #define CFG_ATB_OFFSET      0x200000
    #define CFG_NODE_OFFSET     0x220000
    #define CFG_DEVICE_OFFSET   0x240000
    #define CFG_MAC_OFFSET      0x260000
    #define CFG_SYSINFO_OFFSET  0x280000
    #define PRE_RES_OFFSET      0x2A0000
#else
    /* 64M */
    #define CFG_DEBUG_OFFSET    0x4000000
    #define CFG_RES_OFFSET      0x4040000
    #define CFG_ATB_OFFSET      0x4080000
    #define CFG_NODE_OFFSET     0x40c0000
    #define CFG_DEVICE_OFFSET   0x4100000
    #define CFG_MAC_OFFSET      0x4140000
    #define CFG_SYSINFO_OFFSET  0x4180000
    #define PRE_RES_OFFSET      0x41C0000
#endif
#define FLASH_CRC 0x11223344
#define FLASH_DEV_CRC 0x43241234
#define PRE_RES_MAGIC 0x55666655
typedef struct _tag_ui_operate
{
    /**
     * @brief 参考 E_UI_CMD
     *
     */
    uint8_t atbui_cmd;
    uint8_t atb_sig_nodeid;
    uint8_t update_txgain;
    uint8_t dbgget_nodeid;
    uint8_t dbgset_nodeid;
    uint8_t devget_nodeid;
    uint8_t devset_nodeid;
    uint8_t uiforward_nodeid;
    uint8_t ota_flag;
    uint8_t ui_dat_buf[UI_BUF_SIZE];
    uint8_t ota_reply_cmd;
    uint8_t ota_reply_len;
    uint8_t ota_reply_buf[DEV_SET_SIZE];
    uint16_t ota_data_sn;
    uint8_t reboot_delay;
    uint8_t dev_set_buf[DEV_SET_SIZE];
} atbui_t;
int ui_eth_send(int efd, uint8_t *pdat, uint16_t slen);
void atb_ui_process(int efd, int ffd);
void atb_ui_tn_process(int efd, int ffd);
int ui_get_nodes(atb_nodeinfo_t *pinfo); 
int flash_save_atbcfg(int ffd);
int flash_save_nodecfg(int ffd);
int flash_save_rescfg(int ffd);
int flash_save_debugcfg(int ffd, void *ptr, uint32_t size);
int flash_load_rescfg(int ffd);
int flash_load_atbcfg(int ffd);
int flash_load_debugcfg(int ffd, void *ptr, uint32_t size);
int flash_save_devcfg(int ffd, void *ptr, uint32_t size);
void atb_ui_rpc_recv(uint8_t *ptr, int rlen, int ffd);
int flash_save_pre_res(int ffd);
int flash_load_pre_res(int ffd);
int flash_erase_pre_res(int ffd);
int flash_load_maccfg(int ffd);
int flash_save_maccfg(int ffd, void *ptr, uint32_t size);
uint8_t *get_maccfg();
atbcfg_t *get_atbcfg();
extern devcfg_t devcfg;
extern atbui_t atbui_cmd;
extern pre_res_config_t pre_res;
extern int dyn;
#endif