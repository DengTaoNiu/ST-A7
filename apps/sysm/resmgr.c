/**
 * @file resmgr.c
 * @author wx (wenxiong@iot-semi.com)
 * @brief 资源管理实现文件，《参考资源配置sdk v1.0.docx》
 * @version 0.1
 * @date 2021-08-23
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "resmgr.h"
#include "debug.h"

static resmgr_config_t resmgr;
/**
 * @brief 记录资源状态，节点上线时，根据资源状态，
 *        判断节点是否需要接收该段资源
 */
static res_state_t res_state[MAX_RES_CFG_ID];


/**
 * @brief Set the resmgr object
 * 
 * @param ptr 指向配置数据，来自flash或上位机
 * @return int 0
 */

int set_resmgr(void *ptr)
{
    int i;
    node_config_t *pncfg;
    res_config_t *prescfg;
    res_state_t *pstate;

    pncfg = resmgr.node_cfg_tbl;
    prescfg = resmgr.res_cfg_tbl;
    pstate = res_state;

    memset((void *)&resmgr, 0, sizeof(resmgr_config_t));
    // LOG_OUT(LOG_INFO, "*********memset resmsgr,size %d,%lx.\n", sizeof(resmgr_config_t), (void *)&resmgr);

    if (NULL != ptr)
    {
        memcpy((void *)&resmgr, ptr, sizeof(resmgr_config_t));
    }

    return 0;
}

/**
 * @brief 返回当前配置的首地址，方便配置保存
 * 
 * @return void* 
 */
void *get_resmgr()
{
    int i;
    node_config_t *pncfg;
    res_config_t *prescfg;
    pncfg = resmgr.node_cfg_tbl;
    prescfg = resmgr.res_cfg_tbl;

    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state != CONFIG_ENABLE_E)
        {
            memset((void *)&pncfg[i], 0, sizeof(node_config_t));
        }
    }

    for (i = 0; i < MAX_RES_CFG_ID; ++i)
    {
        if((prescfg[i].cfg_state != CONFIG_ENABLE_E) && (prescfg[i].cfg_state != CONFIG_DISABLE_E))
        {
            memset((void *)&prescfg[i], 0, sizeof(res_config_t));
        }
        
    }
    return (void *)&resmgr;
}

/**
 * @brief Get the trs res config object
 * 
 * @param mac 节点唯一标识
 * @param pres 返回节点的发送资源配置表
 * @return int  0找到，1未找到
 */

int get_trs_res_config(uint8_t *mac, hcb_fixres_t *pres)
{
    int i;
    int j;
    int tresid;
    int rescnt;
    node_config_t *pncfg;
    res_config_t *prescfg;

    pncfg = resmgr.node_cfg_tbl;
    prescfg = resmgr.res_cfg_tbl;
    pres->rescnt = 0;
    memset((void *)pres, 0, sizeof(hcb_fixres_t));

    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            continue;
        }
        if (0 == memcmp(mac, pncfg[i].mac, MAC_LEN))
        {
            rescnt = 0;
            for (j = 0; j < MAX_TRS_RES_CNT; ++j)
            {
                tresid = pncfg[i].trs_res_id[j];
                if (tresid == UNUSED_RES_ID)
                {
                    continue;
                }
                /*该资源配置无效*/
                if (prescfg[tresid].cfg_state != CONFIG_ENABLE_E)
                {
                    continue;
                }
                pres->res.trs[rescnt].resid = prescfg[tresid].resid;
                pres->res.trs[rescnt].chan = prescfg[tresid].chan;
                pres->res.trs[rescnt].freq = prescfg[tresid].freq;
                pres->res.trs[rescnt].len = prescfg[tresid].len;
                pres->res.trs[rescnt].pos = prescfg[tresid].pos;
                pres->res.trs[rescnt].send2all = prescfg[tresid].send2all;
                ++rescnt;
            }
            pres->rescnt = rescnt;
            break;
        }
    }
    if (pres->rescnt > 0)
    {
        return 0;
    }
    LOG_OUT(LOG_DBG, "find no trs res.\n");
    return 1;
}
/**
 * @brief Get the rcv res config object
 * 
 * @param mac 节点唯一标识
 * @param pres 返回节点的接收资源配置表中已经生效和资源id
 * @param res_in_use 正在使用的资源id列表
 * @return int 0找到，1未找到
 */

int get_rcv_res_config(uint8_t *mac, hcb_fixres_t *pres)
{
    int i;
    int j;
    int rescnt;
    int tresid;
    int rcvall;
    node_config_t *pncfg;
    res_config_t *prescfg;

    pncfg = resmgr.node_cfg_tbl;
    prescfg = resmgr.res_cfg_tbl;
    memset((void *)pres, 0, sizeof(hcb_fixres_t));

    rescnt = 0;
    rcvall = 0;
    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            continue;
        }

        if (0 == memcmp(mac, pncfg[i].mac, MAC_LEN))
        {
            rcvall = pncfg[i].rcvall;
            /*recv all 广播资源*/
            if (pncfg[i].rcvall != 0)
            {
                /*添加活跃在线资源*/
                for (j = 1; j < MAX_RES_CFG_ID; ++j)
                {
                    tresid = j;
                    if (rescnt >= MAX_NODE_RECV_CNT)
                    {
                        break;
                    }
                    if (res_state[tresid].state == 0)
                    {
                        continue;
                    }
                    pres->res.rcv[rescnt].resid = prescfg[tresid].resid;
                    pres->res.rcv[rescnt].len = prescfg[tresid].len;
                    pres->res.rcv[rescnt].pos = res_state[tresid].pos;
                    pres->res.rcv[rescnt].freq = res_state[tresid].freq;
                    pres->res.rcv[rescnt].snode = res_state[tresid].snode;
                    ++rescnt;
                }
            }
            else
            {
                for (j = 0; j < MAX_RCV_RES_CNT; ++j)
                {
                    tresid = pncfg[i].rcv_res_id[j];
                    if (tresid == UNUSED_RES_ID)
                    {
                        continue;
                    }
                    /**
                    * @brief 如果该资源所属节点还未上线，不下发
                    */
                    if (res_state[tresid].state == 0)
                    {
                        continue;
                    }
                    pres->res.rcv[rescnt].resid = prescfg[tresid].resid;
                    pres->res.rcv[rescnt].len = prescfg[tresid].len;
                    pres->res.rcv[rescnt].pos = res_state[tresid].pos;
                    pres->res.rcv[rescnt].freq = res_state[tresid].freq;
                    pres->res.rcv[rescnt].snode = res_state[tresid].snode;
                    ++rescnt;
                }
            }
            break;
        }
    }
    /*监听已经添加则不再添加，避免重复添加*/
    if (rcvall == 0)
    {
        /*添加send to all 广播资源*/
        for (i = 1; i < MAX_RES_CFG_ID; ++i)
        {
            if (rescnt >= MAX_NODE_RECV_CNT)
            {
                break;
            }
            tresid = i;
            if (res_state[tresid].state == 0)
            {
                continue;
            }
            if (prescfg[tresid].send2all == 0)
            {
                continue;
            }
            pres->res.rcv[rescnt].resid = prescfg[tresid].resid;
            pres->res.rcv[rescnt].len = prescfg[tresid].len;
            pres->res.rcv[rescnt].pos = res_state[tresid].pos;
            pres->res.rcv[rescnt].freq = res_state[tresid].freq;
            pres->res.rcv[rescnt].snode = res_state[tresid].snode;
            ++rescnt;
        }
    }

    pres->rescnt = rescnt;
    /**
     * @brief 过滤自己的发送资源？ 
     * 先添加接收资源，后添加发送资源，自己的发送资源还没有激活，
     * 所以不用考虑过滤自己的发送资源
     * 
     */
    if (pres->rescnt > 0)
    {
        return 0;
    }
    LOG_OUT(LOG_DBG, "find no rcv res.\n");
    return 1;
}
/**
 * @brief 获取需要删除的资源，节点下线时处理
 * 
 * @param mac 节点配置mac，唯一标识
 * @param pres 返回结果
 * @return int 
 */
int get_del_res_config(uint8_t *mac, hcb_fixres_t *pres)
{
    int i;
    int j;
    int tresid;
    int rescnt;
    node_config_t *pncfg;
    res_config_t *prescfg;
    res_state_t *pstate;

    pncfg = resmgr.node_cfg_tbl;
    prescfg = resmgr.res_cfg_tbl;
    pstate = res_state;

    memset((void *)pres, 0, sizeof(hcb_fixres_t));

    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            continue;
        }
        if (0 == memcmp(mac, pncfg[i].mac, MAC_LEN))
        {
            rescnt = 0;
            for (j = 0; j < MAX_TRS_RES_CNT; ++j)
            {
                tresid = pncfg[i].trs_res_id[j];
                if (tresid == UNUSED_RES_ID)
                {
                    continue;
                }
                /**/
                if (pstate[tresid].state != 0)
                {
                    pres->res.del[rescnt].freq = pstate[tresid].freq;
                    pres->res.del[rescnt].pos = pstate[tresid].pos;
                    pres->res.del[rescnt].len = prescfg[tresid].len;
                    pres->res.del[rescnt].resid = tresid;
                    pres->res.del[rescnt].send2all = prescfg[tresid].send2all;
                    ++rescnt;
                }
            }
            pres->rescnt = rescnt;
            break;
        }
    }
    if (pres->rescnt > 0)
    {
        return 0;
    }
    LOG_OUT(LOG_DBG, "find no del res.\n");
    return 1;
}
/**
 * @brief Get the node res id object
 *        获取节点资源配置id，在节点上线的时候发给它
 * @param mac 节点唯一标识
 * @param pcfg 节点发送和接收 相关资源 id
 * @return int  0找到，1未找到
 */

int get_node_res_id(uint8_t *mac, node_config_t *pcfg)
{
    int i;
    node_config_t *pncfg;
    pncfg = resmgr.node_cfg_tbl;
    memset((void *)pcfg, 0, sizeof(node_config_t));
    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            continue;
        }
        if (0 == memcmp(mac, pncfg[i].mac, MAC_LEN))
        {
            memcpy((void *)pcfg, (void *)&pncfg[i], sizeof(node_config_t));
            return 0;
        }
    }
    LOG_OUT(LOG_DBG, "find no res id config.\n");
    return 1;
}

/**
 * @brief 激活资源，该资源已经在发送，可以配置到资源接收里面
 * 
 * @param resid 
 * @param freq 
 * @param pos 
 * @return int 
 */

int active_res(uint8_t resid, uint8_t snode, uint8_t freq, uint8_t pos)
{
    res_config_t *prescfg;
    if (resid == 0)
    {
        LOG_OUT(LOG_ERR, "resid must big than 0\n");
        return 1;
    }
    prescfg = resmgr.res_cfg_tbl;
    if (prescfg[resid].cfg_state != CONFIG_ENABLE_E)
    {
        return 1;
    }
    res_state[resid].snode = snode;
    res_state[resid].state = 1;
    res_state[resid].freq = freq;
    res_state[resid].pos = pos;
    return 0;
}
/**
 * @brief 该资源失效
 * 
 * @param resid 
 * @return int 
 */

int deactive_res(uint8_t resid)
{
    if (resid == 0)
    {
        LOG_OUT(LOG_ERR, "resid must big than 0\n");
        return 1;
    }
    res_state[resid].state = UNUSED_RES_ID;
    return 0;
}
/**
 * @brief 添加资源配置
 * 
 * @param resid 资源id
 * @param res 资源内容
 * @return int 0成功，1失败
 */

int add_res_config(uint8_t resid, res_config_t *res)
{
    int i;
    int j;
    int ret;
    if (resid == 0)
    {
        LOG_OUT(LOG_ERR, "resid must big than 0\n");
        return 1;
    }

    res_config_t *prescfg;
    prescfg = resmgr.res_cfg_tbl;
    res->send2all = 0;
    memcpy((void *)&prescfg[resid], res, sizeof(res_config_t));
    prescfg[resid].cfg_state = CONFIG_ENABLE_E;
    prescfg[resid].resid = resid;
    
    /*Troubleshoot the serial port configuration resource display error*/
    if (prescfg[resid].pos == 0)
    {
        if(resid == 1)
        {
            prescfg[resid].pos = 4;
        }else{
            prescfg[resid].pos = prescfg[resid - 1].pos + prescfg[resid - 1].len;
        }
    }
    
    return 0;
}
/**
 * @brief 删除没有接收资源，也没有发送资源的节点配置
 * 
 * @return int 
 */

int clear_node_config(void)
{
    int i;
    int j;
    node_config_t *pncfg;
    res_config_t *prescfg;
    /*删除既无接收资源，也无发送资源的节点配置*/
    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            continue;
        }
        if (pncfg[i].rcvall != 0)
        {
            continue;
        }
        for (j = 0; j < MAX_TRS_RES_CNT; ++j)
        {
            if (pncfg[i].trs_res_id[j] != UNUSED_RES_ID)
            {
                break;
            }
        }
        if (j < MAX_TRS_RES_CNT)
        {
            continue;
        }
        for (j = 0; j < MAX_RCV_RES_CNT; ++j)
        {
            if (pncfg[i].rcv_res_id[j] != UNUSED_RES_ID)
            {
                break;
            }
        }
        if (j < MAX_RCV_RES_CNT)
        {
            continue;
        }
        pncfg[i].cfg_state = CONFIG_UNVALID_E;
        LOG_OUT(LOG_INFO, "del node cfg %d\n", i);
    }
    return 0;
}
/**
 * @brief 删除资源配置
 * 
 * @param resid 需要删除的资源配置的id
 * @return int 
 */

int del_res_config(uint8_t resid)
{
    int i;
    int j;
    node_config_t *pncfg;
    res_config_t *prescfg;

    if (resid == 0)
    {
        LOG_OUT(LOG_ERR, "resid must big than 0\n");
        return 1;
    }

    pncfg = resmgr.node_cfg_tbl;
    prescfg = resmgr.res_cfg_tbl;

    prescfg[resid].cfg_state = CONFIG_UNVALID_E;
    /*删除节点中的收发资源id*/
    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            continue;
        }
        for (j = 0; j < MAX_TRS_RES_CNT; ++j)
        {
            if (pncfg[i].trs_res_id[j] == resid)
            {
                pncfg[i].trs_res_id[j] = UNUSED_RES_ID;
            }
        }
        for (j = 0; j < MAX_RCV_RES_CNT; ++j)
        {
            if (pncfg[i].rcv_res_id[j] == resid)
            {
                pncfg[i].rcv_res_id[j] = UNUSED_RES_ID;
            }
        }
    }
    clear_node_config();
    return 0;
}

/**
 * @brief 为节点添加发送资源，并让该资源配置生效
 * 
 * @param resid 资源配置id
 * @param mac  节点唯一标识 
 * @return int  0成功，非0失败
 */
int add_trs_res(uint8_t resid, uint8_t *mac, uint8_t send2all)
{
    int i;
    int j;
    node_config_t *pncfg;
    res_config_t *prescfg;
    if (resid == 0)
    {
        LOG_OUT(LOG_ERR, "resid must big than 0\n");
        return 1;
    }
    /*先检查该资源配置是否有效*/
    if (prescfg[resid].cfg_state != CONFIG_ENABLE_E)
    {
        LOG_OUT(LOG_ERR, "resid %d config unvalid.\n", resid);
        return 1;
    }

    pncfg = resmgr.node_cfg_tbl;
    prescfg = resmgr.res_cfg_tbl;
    if (prescfg[resid].cfg_state == CONFIG_UNVALID_E)
    {
        LOG_OUT(LOG_DBG, "res %d, not valid,can not be used.\n", resid);
        return 1;
    }

    if (send2all != 0)
    {
        prescfg[resid].send2all = 1;
    }
    else
    {
        prescfg[resid].send2all = 0;
    }

    /*清除该资源的发送资源，避免两个节点添加了同一个发送资源*/
    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            continue;
        }
        for (j = 0; j < MAX_TRS_RES_CNT; ++j)
        {
            if (pncfg[i].trs_res_id[j] == resid)
            {
                pncfg[i].trs_res_id[j] = UNUSED_RES_ID;
            }
        }
    }
    /*添加发送资源 resid*/
    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            continue;
        }
        if (0 == memcmp(mac, pncfg[i].mac, MAC_LEN))
        {
            /*清除该节点的接收资源，避免同时发送和接收同一个资源*/
            for (j = 0; j < MAX_RCV_RES_CNT; ++j)
            {
                if (pncfg[i].rcv_res_id[j] == resid)
                {
                    pncfg[i].rcv_res_id[j] = UNUSED_RES_ID;
                }
            }
            /*遍历是否已经添加过该发送资源*/
            for (j = 0; j < MAX_TRS_RES_CNT; ++j)
            {
                if (pncfg[i].trs_res_id[j] == resid)
                {
                    return 0;
                }
            }
            /*添加发送资源id*/
            for (j = 0; j < MAX_TRS_RES_CNT; ++j)
            {
                if (pncfg[i].trs_res_id[j] == UNUSED_RES_ID)
                {
                    pncfg[i].trs_res_id[j] = resid;
                    return 0;
                }
            }
            LOG_OUT(LOG_DBG, "recv res id cnt big than MAX_TRS_RES_CNT");
            return 1;
        }
    }
    /*节点资源配置不存在，添加一个新的*/
    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            memset((void *)&pncfg[i], 0, sizeof(node_config_t));
            memcpy(pncfg[i].mac, mac, MAC_LEN);
            pncfg[i].trs_res_id[0] = resid;
            pncfg[i].cfg_state = CONFIG_ENABLE_E;
            return 0;
        }
    }
    LOG_OUT(LOG_DBG, "node config cnt big than MAX_CONFIG_NODE_CNT");
    return 2;
}

/**
 * @brief 为节点添加接收资源
 * 
 * @param resid 资源id
 * @param mac 节点唯一标识 
 * @return int 0成功，非0失败
 */
int add_rcv_res(uint8_t resid, uint8_t *mac)
{
    int i;
    int j;
    node_config_t *pncfg;
    res_config_t *prescfg;
    pncfg = resmgr.node_cfg_tbl;
    prescfg = resmgr.res_cfg_tbl;

    if (resid == 0)
    {
        LOG_OUT(LOG_ERR, "resid must big than 0\n");
        return 1;
    }

    /*先检查该资源配置是否有效*/
    if ((prescfg[resid].cfg_state != CONFIG_ENABLE_E) && (resid != BROADCAST_RES_ID))
    {
        LOG_OUT(LOG_ERR, "resid %d config unvalid.\n", resid);
        return 1;
    }

    /*添加接收资源 resid*/
    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            continue;
        }
        if (0 == memcmp(mac, pncfg[i].mac, MAC_LEN))
        {
            /*接收所有资源*/
            if (BROADCAST_RES_ID == resid)
            {
                pncfg[i].rcvall = 1;
                return 0;
            }

            /*节点不接收自己的发送资源*/
            for (j = 0; j < MAX_TRS_RES_CNT; ++j)
            {
                if (pncfg[i].trs_res_id[j] == resid)
                {
                    return 0;
                }
            }
            /*遍历是否已经添加，避免重复添加*/
            for (j = 0; j < MAX_RCV_RES_CNT; ++j)
            {
                if (pncfg[i].rcv_res_id[j] == resid)
                {
                    return 0;
                }
            }
            /*添加*/
            for (j = 0; j < MAX_RCV_RES_CNT; ++j)
            {
                if (pncfg[i].rcv_res_id[j] == UNUSED_RES_ID)
                {
                    pncfg[i].rcv_res_id[j] = resid;
                    return 0;
                }
            }
            LOG_OUT(LOG_DBG, "recv res id cnt big than MAX_RCV_RES_CNT");
            return 1;
        }
    }
    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            memset((void *)&pncfg[i], 0, sizeof(node_config_t));
            memcpy(pncfg[i].mac, mac, MAC_LEN);
            if (resid == 0xff)
            {
                pncfg[i].rcvall = 1;
            }
            else
            {
                pncfg[i].rcv_res_id[0] = resid;
            }

            pncfg[i].cfg_state = CONFIG_ENABLE_E;
            return 0;
        }
    }
    LOG_OUT(LOG_DBG, "node config cnt big than MAX_CONFIG_NODE_CNT");

    return 2;
}

/**
 * @brief 从节点配置中移除某发送资源
 * 
 * @param resid 
 * @param mac 
 * @return int 
 */

int del_trs_res(uint8_t resid, uint8_t *mac)
{
    int i;
    int j;
    node_config_t *pncfg;
    if (resid == 0)
    {
        LOG_OUT(LOG_ERR, "resid must big than 0\n");
        return 1;
    }
    pncfg = resmgr.node_cfg_tbl;
    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            continue;
        }
        if (0 == memcmp(mac, pncfg[i].mac, MAC_LEN))
        {
            for (j = 0; j < MAX_TRS_RES_CNT; ++j)
            {
                if (pncfg[i].trs_res_id[j] == resid)
                {
                    pncfg[i].trs_res_id[j] = UNUSED_RES_ID;
                    clear_node_config();
                    return 0;
                }
            }
            LOG_OUT(LOG_DBG, "res id not in trs res id table.");
            return 1;
        }
    }
    LOG_OUT(LOG_DBG, "do not find node res config.");
    return 1;
}

/**
 * @brief 从节点配置中移除某接收资源
 * 
 * @param resid 
 * @param mac 
 * @return int 
 */

int del_rcv_res(uint8_t resid, uint8_t *mac)
{
    int i;
    int j;
    node_config_t *pncfg;
    if (resid == 0)
    {
        LOG_OUT(LOG_ERR, "resid must big than 0\n");
        return 1;
    }
    pncfg = resmgr.node_cfg_tbl;
    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            continue;
        }
        if (0 == memcmp(mac, pncfg[i].mac, MAC_LEN))
        {
            for (j = 0; j < MAX_RCV_RES_CNT; ++j)
            {
                if (pncfg[i].rcv_res_id[j] == resid)
                {
                    pncfg[i].rcv_res_id[j] = UNUSED_RES_ID;
                    clear_node_config();
                    return 0;
                }
            }
            LOG_OUT(LOG_DBG, "res id not in rcv res id table.");
            return 1;
        }
    }
    LOG_OUT(LOG_DBG, "do not find node res config.");
    return 1;
}

/**
 * @brief 删除某节点配置
 * 
 * @param mac 
 * @return int 
 */
int del_node_config(uint8_t *mac)
{
    int i;
    int j;
    node_config_t *pncfg;
    pncfg = resmgr.node_cfg_tbl;
    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state == CONFIG_UNVALID_E)
        {
            continue;
        }
        if (0 == memcmp(mac, pncfg[i].mac, MAC_LEN))
        {
            pncfg[i].cfg_state = CONFIG_UNVALID_E;
            for (j = 0; j < MAX_TRS_RES_CNT; j++)
            {
                if (pncfg[i].trs_res_id[j] != UNUSED_RES_ID)
                {
                    del_res_config(pncfg[i].trs_res_id[j]);
                }
            }
            return 0;
        }
    }

    LOG_OUT(LOG_DBG, "do not find node res config.");
    return 1;
}

int debug_cmd_res_config(void *parg, int argc, const char **argv)
{
    int i;
    int j;
    node_config_t *pncfg;
    res_config_t *prescfg;
    res_state_t *pstate;

    pncfg = resmgr.node_cfg_tbl;
    prescfg = resmgr.res_cfg_tbl;
    pstate = res_state;

    printf("resid\tchan\tfreq\tlen\tsend2all\tpos\tcfgstate\n");
    for (i = 1; i < MAX_RES_CFG_ID; i++)
    {
        if (prescfg[i].cfg_state != CONFIG_UNVALID_E)
        {
            printf("%d\t%d\t%d\t%d\t%d\t\t%d\t%d\n", prescfg[i].resid, prescfg[i].chan, prescfg[i].freq, prescfg[i].len, prescfg[i].send2all, prescfg[i].pos, prescfg[i].cfg_state);
        }
    }
    printf("\n\n\n-----------------------------------------------------------------------\n\n\n");
    printf("mac\t\trecvall\tcfgstate\ttrs res id\t\t\trecv res id\n");
    for (i = 0; i < MAX_CONFIG_NODE_CNT; ++i)
    {
        if (pncfg[i].cfg_state != CONFIG_UNVALID_E)
        {
            printf("%02X%02X%02X%02X%02X%02X\t", pncfg[i].mac[0], pncfg[i].mac[1], pncfg[i].mac[2], pncfg[i].mac[3], pncfg[i].mac[4], pncfg[i].mac[5]);
            printf("%d\t%d\t\t", pncfg[i].rcvall, pncfg[i].cfg_state);
            for (j = 0; j < MAX_TRS_RES_CNT; ++j)
            {
                printf("%02x ", pncfg[i].trs_res_id[j]);
            }
            printf("\t");
            for (j = 0; j < MAX_RCV_RES_CNT; ++j)
            {
                printf("%02x ", pncfg[i].rcv_res_id[j]);
            }
            printf("\n");
        }
    }
    printf("\n\n\n-----------------------------------------------------------------------\n\n\n");
    printf("resid\tsnode\tfreq\tpos\tactivestate\n");
    for (i = 1; i < MAX_RES_CFG_ID; i++)
    {
        if (prescfg[i].cfg_state != CONFIG_UNVALID_E)
        {
            printf("%d\t%d\t%d\t%d\t%d\n", i, pstate[i].snode, pstate[i].freq, pstate[i].pos, pstate[i].state);
        }
    }
    return 0;
usage:
    printf("usage: %s \n", argv[0]);
    return 0;
}
/**
 * @brief 清除当前配置，但并不保存，saveres保存
 * 
 * @param parg 
 * @param argc 
 * @param argv 
 * @return int 
 */
int debug_cmd_res_config_clear(void *parg, int argc, const char **argv)
{
    memset((void *)&resmgr, 0, sizeof(resmgr_config_t));
    return 0;
}