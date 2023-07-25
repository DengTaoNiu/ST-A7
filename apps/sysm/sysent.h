
#pragma once
#include "resmgr.h"
#include "debug.h"
#include "atbui.h"

typedef enum
{
    RES_NODE_ONLINE = 5,
    RES_NODE_CFG,
} TN_RES_STATE_E;

typedef struct _tn_report_
{
    /**
     * @brief 节点id
     * 
     */
    uint8_t nodeid;
    /**
     * @brief 信道错包增加
     *
     */
    uint8_t sigerr;
    /**
     * @brief 节点位置上报
     *
     */
    uint8_t report;
} tnreport_t;

typedef struct _tag_sysm_context
{
    comn_context cmctx;

    /**/
    int dfd; /* /dpctrl  */
    int qfd; /* /dprrpc  */
    int efd; /* /eth/if */
    int ffd; /* /flash   */


    int msgresfd;
    int msguifd;
    uint8_t state;
    uint8_t sigerr; 
    uint8_t report; 
    /**/
    uint8_t nodeid;
    uint8_t nodeaddr; 
    uint8_t mac[MAC_LEN];
    uint8_t name[NAME_LEN];
    /*收发资源id列表*/
    uint8_t rcvall;
    uint8_t trs_res_id[MAX_TRS_RES_CNT];
    uint8_t rcv_res_id[MAX_RCV_RES_CNT];
    uint8_t bver[16];
    uint8_t devtype[16]; 
    uint64_t systime;
} sysm_context_t;

/* cmd, resource add */
int sysres_cmd_add_resource(void *parg, int argc, const char **argv);
int sysres_cmd_commit(void *parg, int argc, const char **argv);

int dbg_cmd_rrpc_send(void *parg, int argc, const char **argv);
int dbg_cmd_rrpc_ctrl(void *parg, int argc, const char **argv);

int test_dump_rrpc(sysm_context_t *pctx, int qfd);
