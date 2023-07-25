/**
 * @file resmgr.h
 * @author wx (wenxiong@iot-semi.com)
 * @brief 资源管理文件，《参考资源配置sdk v1.0.docx》
 * @version 0.1
 * @date 2021-08-23
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef __RES_MGR_H_
#define __RES_MGR_H_

#include <stdint.h>

#define LOG_INFO 1
#define LOG_DBG 1
#define LOG_ERR 1

#define LOG_OUT(loglevel, ...)                       \
    do                                               \
    {                                                \
        if (loglevel)                                \
        {                                            \
            printf("%s,%d\t: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                     \
        }                                            \
    } while (0);

/**
 * @brief flash中当前能保存的最大节点配置数量
 * 
 */
#define MAX_CONFIG_NODE_CNT 64
/**
 * @brief flash中当前能保存的最大的资源配置数量
 * 
 */
#define MAX_RES_CFG_ID 254
/**
 * @brief 单个节点，当前最大的发送资源数量
 * 
 */
#define MAX_TRS_RES_CNT 8
/**
 * @brief 单个节点，当前最大的接收资源数量
 * 
 */
#define MAX_RCV_RES_CNT 16
#define MAX_NODE_RECV_CNT MAX_RCV_RES_CNT
/**
 * @brief 此资源id，表示接收所有的资源
 * 
 */

#define BROADCAST_RES_ID 0xff
/**
 * @brief 不可用的资源id
 * 
 */
#define UNUSED_RES_ID 0

/**
 * @brief 节点mac长度限制 
 * 
 */
#define MAC_LEN 6

typedef enum
{
    CONFIG_UNVALID_E = 0, //配置无效处理
    CONFIG_DISABLE_E, //配置不使能处理(不生效)
    CONFIG_ENABLE_E, //配置使能处理(生效)
} CONFIG_STATE_E;

/**
 * @brief 单个资源配置结构体
 * 
 */
#pragma pack(push, 1)
typedef struct _tag_res_config
{
    /*资源唯一标识，从1 开始，最大254，255用于广播接收，0表示该段资源不可用*/
    uint8_t resid; 
    /*资源通道类型，可用于转发优先级*/
    uint8_t chan;
    /*资源分配频率，1，2，4，8*/
    uint8_t freq;
    /*该段资源长度*/
    uint8_t len;
    /*配置状态*/
    uint8_t cfg_state;
    /*广播，所有节点接收该资源*/
    uint8_t send2all;
    /*使用 位置*/
    uint8_t pos;
} res_config_t;

typedef struct _tag_pre_res_config
{
    uint8_t num;
    uint8_t chan;
    uint8_t freq;
    uint8_t len;
    uint32_t magic;
    uint8_t state; //yjn
} pre_res_config_t;
/**
 * @brief 节点资源关联结构体
 * 
 */

typedef struct _tag_node_config
{
    /*节点唯一标识符*/
    uint8_t mac[MAC_LEN];
    /*配置状态*/
    uint8_t cfg_state;
    /*广播支持，接收所有资源*/
    uint8_t rcvall;
    /*保存当前节点需要发送的资源id*/
    uint8_t trs_res_id[MAX_TRS_RES_CNT];
    /*保存当前节点需要接收的资源id*/
    uint8_t rcv_res_id[MAX_RCV_RES_CNT];
} node_config_t;
/**
 * @brief 资源管理配置结构体
 * 
 */

typedef struct _tag_resmgr_config
{
    /*资源配置表,resid 和数组下标一一对应*/
    res_config_t res_cfg_tbl[MAX_RES_CFG_ID];
    /*节点资源关联表*/
    node_config_t node_cfg_tbl[MAX_CONFIG_NODE_CNT];
} resmgr_config_t;

#pragma pack(pop)

typedef struct
{
    uint8_t resid;
    /*资源通道类型，可用于转发优先级*/
    uint8_t chan;
    /*资源分配频率，1，2，4，8*/
    uint8_t freq;
    /*起始位置*/
    uint8_t pos;
    /*该段资源长度*/
    uint8_t len;
    /*所有节点都需要接收该资源*/
    uint8_t send2all;
} trs_res_data_t;

typedef struct
{
    uint8_t resid;
    /*资源通道类型，可用于转发优先级*/
    uint8_t snode;
    /*资源分配频率，1，2，4，8*/
    uint8_t freq;
    /*起始位置*/
    uint8_t pos;
    /*该段资源长度*/
    uint8_t len;
} rcv_res_data_t;

typedef struct
{
    uint8_t resid;
    uint8_t freq;
    uint8_t pos;
    uint8_t len;
    uint8_t send2all;
} del_res_data_t;

typedef struct
{
    uint8_t rescnt;
    /*发送方的nodeid，用以判断，后续资源是接收还是发送*/
    uint8_t snode;
    /*收发资源列表*/
    union
    {
        trs_res_data_t trs[MAX_TRS_RES_CNT];
        rcv_res_data_t rcv[MAX_NODE_RECV_CNT];
        del_res_data_t del[MAX_TRS_RES_CNT];
    } res;

} hcb_fixres_t;

/**
 * @brief 运行时，资源状态记录
 * 
 */
typedef struct
{
    uint8_t snode;
    uint8_t pos;
    uint8_t freq;
    uint8_t state;
} res_state_t;

int set_resmgr(void *ptr);
void *get_resmgr(void);

int get_trs_res_config(uint8_t *mac, hcb_fixres_t *pres);
int get_rcv_res_config(uint8_t *mac, hcb_fixres_t *pres);
int get_del_res_config(uint8_t *mac, hcb_fixres_t *pres);
int get_node_res_id(uint8_t *mac, node_config_t *pcfg);

int active_res(uint8_t resid, uint8_t snode, uint8_t freq, uint8_t pos);
int deactive_res(uint8_t resid);

int add_res_config(uint8_t resid, res_config_t *res);
int del_res_config(uint8_t resid);

/*未实现add_node_config，在add_rcv_res及add_trs_res中默认实现*/
int add_node_config(uint8_t *mac);
int del_node_config(uint8_t *mac);
int add_trs_res(uint8_t resid, uint8_t *mac, uint8_t send2all);
int add_rcv_res(uint8_t resid, uint8_t *mac);
int del_rcv_res(uint8_t resid, uint8_t *mac);
int del_trs_res(uint8_t resid, uint8_t *mac);

int debug_cmd_res_config(void *parg, int argc, const char **argv);
int debug_cmd_res_config_clear(void *parg, int argc, const char **argv);

#endif