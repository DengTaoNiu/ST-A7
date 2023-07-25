/**
 * @file atbui.c
 * @author your name (you@domain.com)
 * @brief 此文件是sdk的上位机交互文件，具体参考《sdk上位机软件初稿.docx》
 * @version 0.1
 * @date 2021-11-09
 *
 * @copyright Copyright (c) 2021
 *
 */
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "dlist.h"
#include "hcb_comn.h"
#include "pkt_api.h"
#include "sys_api.h"
#include "debug.h"
#include "phb1.h"
#include "resmgr.h"
#include "atbui.h"
#include "sysent.h"
#include "sysres.h"

#define BUF_SIZE 4096

const uint8_t srcmac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
const uint8_t dstmac[6] = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11};

static atbcfg_t atbcfg;

devcfg_t devcfg;
pre_res_config_t pre_res;
static uint8_t maccfg[MAC_LEN];
typedef struct
{
    uint32_t ridx;
    uint8_t sbuf[BUF_SIZE];
    uint8_t rbuf[BUF_SIZE];
} atb2eth_t;

static atb2eth_t rxtxbuf;

atbui_t atbui_cmd;
int dyn = 0;
/**
 * @brief  多包发送函数，有效数据1024+(dstmac,srcmac,type,len)字节为一包
 *
 * @param efd
 * @param pdat
 * @param slen
 * @return int
 */
int ui_eth_send(int efd, uint8_t *pdat, uint16_t slen)
{
    int iret;
    int cnt;
    int i;
    pbuf_t *ipkt;
    uint32_t crc;
    uint16_t tlen;
    uint8_t *ptr;
    tlen = slen;

    /*追加校验*/
    if (tlen > (BUF_SIZE - 4))
    {
        return 1;
    }
    crc = 0;
    for (i = 0; i < tlen; i++)
    {
        crc += pdat[i] & 0xff;
    }
    // LOG_OUT(LOG_DBG, "send crc 0x%lx\n", crc);
    memcpy((void *)(pdat + tlen), (void *)&crc, 4);
    tlen += 4;

    cnt = tlen / 1024;
    if ((tlen % 1024) != 0)
    {
        cnt += 1;
    }
    for (i = 0; i < cnt; ++i)
    {
        ipkt = pkt_alloc(0);
        if (ipkt == NULL)
        {
            LOG_OUT(LOG_ERR, "reply err,alloc fail.\n");
            return 1;
        }
        ptr = pkt_append(ipkt, 6);
        memcpy(ptr, dstmac, 6);
        ptr = pkt_append(ipkt, 6);
        memcpy(ptr, srcmac, 6);
        ptr = pkt_append(ipkt, 2);
        /*最后一包,当cnt=1时，既是第一包也是最后一包*/
        if (i == (cnt - 1))
        {
            *(uint16_t *)ptr = 0x1234;
            ptr = pkt_append(ipkt, 2);
            memcpy(ptr, &tlen, 2);
            ptr = pkt_append(ipkt, tlen);
            memcpy(ptr, pdat, tlen);
        }
        /*第一包*/
        else if (i == 0)
        {
            *(uint16_t *)ptr = 0x1235;
            ptr = pkt_append(ipkt, 2);
            *(uint16_t *)ptr = 1024;
            ptr = pkt_append(ipkt, 1024);
            memcpy(ptr, pdat, 1024);
            tlen -= 1024;
            pdat += 1024;
        }
        else
        {
            *(uint16_t *)ptr = 0x1236;
            ptr = pkt_append(ipkt, 2);
            *(uint16_t *)ptr = 1024;
            ptr = pkt_append(ipkt, 1024);
            memcpy(ptr, pdat, 1024);
            tlen -= 1024;
            pdat += 1024;
        }
        /**
         * @brief 如果efd小于0，配置请求来自tn，调用rpc
         *
         */
        void set_report(uint8_t nodeid);
        set_report(atbui_cmd.uiforward_nodeid);
        if ((0 != atbui_cmd.uiforward_nodeid) || (efd < 0))
        {
            // LOG_OUT(LOG_INFO,"respond tn:%d\n",atbui_cmd.uiforward_nodeid);
            rpc_ui_send(atbui_cmd.uiforward_nodeid, pkt_to_addr(ipkt), ipkt->length);
            pkt_free(ipkt);
        }
        else
        {
            iret = send(efd, ipkt);
            if (iret != 1)
            {
                pkt_free(ipkt);
                return 2;
            }
        }
    }
    atbui_cmd.uiforward_nodeid = 0;
    return 0;
}

/**
 * @brief 多包接收函数
 *
 * @param pdat
 * @param rlen
 * @return int
 */
static int eth_recv(uint8_t *pdat, uint16_t rlen)
{
    int i;
    int finish;
    uint16_t type;
    uint16_t datlen;
    uint32_t crc;
    uint32_t crcrcv;

    type = *(uint16_t *)pdat;
    pdat = pdat + 2;
    datlen = *(uint16_t *)pdat;
    pdat = pdat + 2;
    // LOG_OUT(LOG_DBG, "type:0x%x--%d\n", type, datlen);
    /*避免数据太长，memcpy错误*/
    if (datlen > 1024)
    {
        LOG_OUT(LOG_DBG, "err pkt data len.\n");
        rxtxbuf.ridx = 0;
        return 1;
    }
    if ((rxtxbuf.ridx + datlen) > BUF_SIZE)
    {
        rxtxbuf.ridx = 0;
        LOG_OUT(LOG_DBG, "buf limit exceed.\n");
        return 1;
    }
    finish = 0;
    switch (type)
    {
    case 0x1234:
        memcpy(rxtxbuf.rbuf + rxtxbuf.ridx, pdat, datlen);
        rxtxbuf.ridx += datlen;
        finish = 1;
        break;
    case 0x1235:
        memcpy(rxtxbuf.rbuf, pdat, datlen);
        rxtxbuf.ridx = datlen;
        break;
    case 0x1236:
        memcpy(rxtxbuf.rbuf + rxtxbuf.ridx, pdat, datlen);
        rxtxbuf.ridx += datlen;
        break;
    default:
        rxtxbuf.ridx = 0;
        break;
    }

    if (0 != finish)
    {
        crc = 0;
        for (i = 0; i < (rxtxbuf.ridx - 4); i++)
        {
            crc += rxtxbuf.rbuf[i];
        }
        memcpy((void *)&crcrcv, &rxtxbuf.rbuf[rxtxbuf.ridx - 4], 4);
        // LOG_OUT(LOG_DBG, "recv crc 0x%lx---0x%lx\n", crc, crcrcv);
        if (crc != crcrcv)
        {
            rxtxbuf.ridx = 0;
            return 1;
        }
        return 0;
    }
    return 1;
}

int ui_update_nodecfg(uint8_t *pbuf);
int ui_get_names(basicnode_t *pnodes);
static void atb_ui_parse(int efd, int ffd, uint8_t *ptr, uint32_t len)
{
    int nodescnt;
    int iret;
    uint16_t type;
    uint16_t rtype;
    uint32_t farg[2];
    int tlen;
    type = *(uint16_t *)ptr;
    ptr += 2;
   

    switch (type)
    {
    case SET_ATB_CFG:
        memcpy(&atbcfg, ptr, sizeof(atbcfg_t));
        tlen = 2;
        if (atbui_cmd.atbui_cmd == E_UI_CMD_NONE)
        {
            atbui_cmd.atbui_cmd = E_UPDATE_TXGAIN;
        }
        rtype = SET_ATB_CFG_REPLY;
        memcpy(rxtxbuf.sbuf, &rtype, 2);
        ui_eth_send(efd, rxtxbuf.sbuf, tlen);
        flash_save_atbcfg(ffd);
        break;
    case GET_ATB_CFG:
        tlen = 2;
        rtype = GET_ATB_CFG_REPLY;
        memcpy(rxtxbuf.sbuf, &rtype, 2);
        tlen += sizeof(atbcfg_t);
        memcpy(&rxtxbuf.sbuf[2], (void *)&atbcfg, sizeof(atbcfg_t));
        ui_eth_send(efd, rxtxbuf.sbuf, tlen);
        break;
    case GET_NODE_INFO:
        tlen = 2;
        rtype = GET_NODE_INFO_REPLY;
        memcpy(rxtxbuf.sbuf, &rtype, 2);
        nodescnt = ui_get_nodes((atb_nodeinfo_t *)&rxtxbuf.sbuf[2]);
        tlen += nodescnt * sizeof(nodeinfo_t) + sizeof(atb_nodeinfo_t);
        ui_eth_send(efd, rxtxbuf.sbuf, tlen);
        break;
    case SET_NODE_NAME:
        break;
    case GET_NODE_BASIC_INFO:
        tlen = 2;
        rtype = GET_NODE_BASIC_INFO_REPLY;
        memcpy(rxtxbuf.sbuf, &rtype, 2);
        nodescnt = ui_get_names((basicnode_t *)&rxtxbuf.sbuf[2]);
        tlen += nodescnt * sizeof(basic_nodeinfo_t) + sizeof(basicnode_t);
        ui_eth_send(efd, rxtxbuf.sbuf, tlen);
        break;
    case GET_RES_CONFIG:
        tlen = 2;
        rtype = GET_RES_CONFIG_REPLY;
        memcpy(rxtxbuf.sbuf, &rtype, 2);
        ptr = get_resmgr();
        memcpy(&rxtxbuf.sbuf[2], ptr, sizeof(resmgr_config_t));
        tlen += sizeof(resmgr_config_t);
        ui_eth_send(efd, rxtxbuf.sbuf, tlen);
        break;
    case SET_RES_CONFIG:
        set_resmgr(ptr);
        tlen = 2;
        rtype = SET_RES_CONFIG_REPLY;
        memcpy(rxtxbuf.sbuf, &rtype, 2);
        ui_eth_send(efd, rxtxbuf.sbuf, tlen);
        flash_save_rescfg(ffd);
        break;

    case GET_ATB_SIG:
        tlen = 2;
        rtype = GET_ATB_SIG_REPLY;
        memcpy(rxtxbuf.sbuf, &rtype, 2);
        if (atbui_cmd.atbui_cmd == E_UI_CMD_NONE)
        {
            atbui_cmd.atbui_cmd = E_GET_ATB_SIG;
            atbui_cmd.atb_sig_nodeid = (*ptr) & 0xff;
        }
        memcpy(&rxtxbuf.sbuf[2], atbui_cmd.ui_dat_buf, UI_BUF_SIZE);
        tlen += UI_BUF_SIZE;
        ui_eth_send(efd, rxtxbuf.sbuf, tlen);
        break;
    case GET_BPS:
        tlen = 2;

        if (atbui_cmd.atbui_cmd == E_UI_CMD_NONE)
        {
            atbui_cmd.atbui_cmd = E_GET_BPS_DATA;
            atbui_cmd.dbgget_nodeid = (*ptr) & 0xff;
        }
        rtype = GET_BPS_REPLY;
        memcpy(rxtxbuf.sbuf, &rtype, 2);
        /**
         * @brief 和信道质量共用一个缓冲区
         *
         */
        memcpy(&rxtxbuf.sbuf[2], atbui_cmd.ui_dat_buf, UI_BUF_SIZE);
        tlen += UI_BUF_SIZE;
        ui_eth_send(efd, rxtxbuf.sbuf, tlen);
        break;
    case GET_DEBUG:
        tlen = 2;

        if (atbui_cmd.atbui_cmd == E_UI_CMD_NONE)
        {
            atbui_cmd.atbui_cmd = E_GET_DEBUG_DATA;
            atbui_cmd.dbgget_nodeid = (*ptr) & 0xff;
        }
        rtype = GET_DEBUG_REPLY;
        memcpy(rxtxbuf.sbuf, &rtype, 2);
        /**
         * @brief 和信道质量共用一个缓冲区
         *
         */
        memcpy(&rxtxbuf.sbuf[2], atbui_cmd.ui_dat_buf, UI_BUF_SIZE);
        tlen += UI_BUF_SIZE;
        ui_eth_send(efd, rxtxbuf.sbuf, tlen);
        break;
    case GET_DEVICE:
        tlen = 2;
        rtype = GET_DEVICE_REPLY;
        memcpy(rxtxbuf.sbuf, &rtype, 2);
        memcpy(&rxtxbuf.sbuf[2], atbui_cmd.dev_set_buf, DEV_SET_SIZE);
        tlen += DEV_SET_SIZE;
        if (atbui_cmd.atbui_cmd == E_UI_CMD_NONE)
        {
            atbui_cmd.atbui_cmd = E_GET_DEVICE_DATA;
            atbui_cmd.devget_nodeid = (*ptr) & 0xff;
            memcpy(atbui_cmd.dev_set_buf, (ptr + 4), 4);
        }
        ui_eth_send(efd, rxtxbuf.sbuf, tlen);
        break;

    case SET_DEVICE:
        tlen = 2;

        if (atbui_cmd.atbui_cmd == E_UI_CMD_NONE)
        {

            atbui_cmd.devset_nodeid = *(uint32_t *)ptr;
            atbui_cmd.atbui_cmd = E_SET_DEVICE_DATA;
            memcpy(atbui_cmd.dev_set_buf, (ptr), DEV_SET_SIZE);
            memcpy(atbui_cmd.ui_dat_buf, (ptr), DEV_SET_SIZE);
            ui_update_nodecfg(atbui_cmd.dev_set_buf);
        }

        rtype = SET_DEVICE_REPLY;
        memcpy(rxtxbuf.sbuf, &rtype, 2);
        ui_eth_send(efd, rxtxbuf.sbuf, tlen);
        break;

    case SET_DEBUG:
        tlen = 2;
        if (atbui_cmd.atbui_cmd == E_UI_CMD_NONE)
        {
            atbui_cmd.atbui_cmd = E_SET_DEBUG_DATA;
            atbui_cmd.dbgset_nodeid = (*ptr) & 0xff;
        }
        ptr += 1;
        memcpy(atbui_cmd.ui_dat_buf, ptr, len - 3);
        rtype = SET_DEBUG_REPLY;
        memcpy(rxtxbuf.sbuf, &rtype, 2);
        ui_eth_send(efd, rxtxbuf.sbuf, tlen);
        break;
    case OTA_START:
        tlen = 2;
        if (atbui_cmd.atbui_cmd == E_UI_CMD_NONE)
        {
            farg[0] = 0;
            ioctl(ffd, 2, sizeof(uint32_t), farg);
            LOG_OUT(LOG_DBG, "erase start.\n");
            farg[0] = APP_FLASH_UPGRADE_ADDR;
            farg[1] = APP_FLASH_UPGRADE_SIZE;
            iret = ioctl(ffd, 3, 8, farg);
            LOG_OUT(LOG_DBG, "erase finish %d.\n", iret);
            atbui_cmd.atbui_cmd = E_OTA_START;
            memcpy(atbui_cmd.ui_dat_buf, ptr, len - 2);
            rtype = OTA_START_REPLY;
            memcpy(rxtxbuf.sbuf, &rtype, 2);
            memcpy(atbui_cmd.ota_reply_buf, rxtxbuf.sbuf, tlen);
            atbui_cmd.ota_reply_cmd = E_OTA_START;
            atbui_cmd.ota_reply_len = tlen;
        }
        break;
    case OTA_DATA:
        tlen = 2;
        if (atbui_cmd.atbui_cmd == E_UI_CMD_NONE)
        {
            atbui_cmd.atbui_cmd = E_OTA_DATA;
            memcpy(atbui_cmd.ui_dat_buf, ptr, len - 2);
            rtype = OTA_DATA_REPLY;
            memcpy(rxtxbuf.sbuf, &rtype, 2);
            memcpy(atbui_cmd.ota_reply_buf, rxtxbuf.sbuf, tlen);
            atbui_cmd.ota_reply_cmd = E_OTA_DATA;
            atbui_cmd.ota_reply_len = tlen;
        }

        break;
    case OTA_END:
        tlen = 2;
        if (atbui_cmd.atbui_cmd == E_UI_CMD_NONE)
        {
            atbui_cmd.atbui_cmd = E_OTA_END;
            memcpy(atbui_cmd.ui_dat_buf, ptr, len - 2);
            rtype = OTA_END_REPLY;
            memcpy(rxtxbuf.sbuf, &rtype, 2);
            memcpy(atbui_cmd.ota_reply_buf, rxtxbuf.sbuf, tlen);
            atbui_cmd.ota_reply_cmd = E_OTA_END;
            atbui_cmd.ota_reply_len = tlen;
        }

        break;
    default:
        break;
    }
    rxtxbuf.ridx = 0;
}
/**
 * @brief  ui界面交互
 *
 * @param efd 以太网描述符
 * @param ffd flash描述符
 */

void atb_ui_process(int efd, int ffd)
{
    int iret;
    pbuf_t *ipkt;
    uint8_t *ptr;

    while (1)
    {
        iret = recv(efd, &ipkt);
        if (iret <= 0 || ipkt == NULL)
        {
            break;
        }

        ptr = pkt_to_addr(ipkt);
        if (0 == memcmp((void *)srcmac, ptr, 6))
        {
            memcpy((void *)dstmac, ptr + 6, 6);
            atbui_cmd.uiforward_nodeid = 0;
            iret = eth_recv(ptr + 12, ipkt->length - 12);
            // debug_dump_hex(ptr + 12,ipkt->length - 12);
            if (0 == iret)
            {
                atb_ui_parse(efd, ffd, rxtxbuf.rbuf, rxtxbuf.ridx);   
            }
        }
        pkt_free(ipkt);
    }
}

/**
 * @brief cn接收从rpc来的tn配置请求
 *
 * @param efd
 * @param ffd
 */
void atb_ui_rpc_recv(uint8_t *ptr, int rlen, int ffd)
{
    int iret;
    if (0 == memcmp((void *)srcmac, ptr, 6))
    {
        memcpy((void *)dstmac, ptr + 6, 6);
        iret = eth_recv(ptr + 12, rlen - 12);
        if (0 == iret)
        {
            atb_ui_parse(-1, ffd, rxtxbuf.rbuf, rxtxbuf.ridx);
        }
    }
}

void atb_ui_tn_process(int efd, int ffd)
{
    int iret;
    pbuf_t *ipkt;
    uint8_t *ptr;

    int i;
    while (1)
    {
        iret = recv(efd, &ipkt);
        if (iret <= 0 || ipkt == NULL)
        {
            break;
        }

        ptr = pkt_to_addr(ipkt);
        rpc_ui_send(0, ptr, ipkt->length);
        pkt_free(ipkt);
    }
}

/* 1M + 832K . */
// #define CFG_DEBUG_OFFSET 0x1C0000
// #define CFG_RES_OFFSET 0x1D0000
// #define CFG_ATB_OFFSET 0x1E0000
// #define CFG_NODE_OFFSET 0x1F0000

// #define FLASH_CRC 0x11223344

/**
 * @brief 保存资源配置，只能在打开flash任务中调用
 *
 * @param ffd
 * @return int
 */
int flash_save_rescfg(int ffd)
{
    int iret;
    uint32_t temp;
    uint32_t uary[2];
    int i;
    atbcfg_t * pcfg;
    resmgr_config_t *presmgr;
    presmgr = get_resmgr();
    memset((void *)presmgr, 'r', sizeof(res_config_t));
   
    /*NDT 获取上位机配置，如果模式为3模式的话，就将第61个symbol变为不可配置*/
    pcfg = get_atbcfg();
    if(pcfg -> msp == 3)
    {
        for(i = 0; i < MAX_RES_CFG_ID; i++)
        {
            if((presmgr->res_cfg_tbl[i].pos) + (presmgr->res_cfg_tbl[i].len) >= 61)
            {
                if ((presmgr->res_cfg_tbl[i].cfg_state == CONFIG_ENABLE_E))
                {
                    // printf("i = %d,presmgr->res_cfg_tbl[i].pos = %d  %d\n",i,presmgr->res_cfg_tbl[i].pos,presmgr->res_cfg_tbl[i].len); 
                    presmgr->res_cfg_tbl[i].len = presmgr->res_cfg_tbl[i].len -1;
                    
                }
            }
        } 
    }   

    /* flash, unlock */
    uary[0] = 0;
    ioctl(ffd, 2, sizeof(uint32_t), &uary[0]);
    /* flash, erase, ofs = 2M, 4K */
    uary[0] = CFG_RES_OFFSET;
    uary[1] = ((sizeof(resmgr_config_t) / 0x1000) + 1) * 0x1000;
    ioctl(ffd, 3, 2 * sizeof(uint32_t), &uary[0]);
    /* seek and write */
    uary[0] = CFG_RES_OFFSET;
    ioctl(ffd, 0, sizeof(uint32_t), &uary[0]);
    iret = write(ffd, presmgr, sizeof(resmgr_config_t));
    if (iret != sizeof(resmgr_config_t))
    {
        LOG_OUT(LOG_DBG, "write res cfg to flash fail.\n");
        return iret;
    }
    LOG_OUT(LOG_INFO, "save res cfg success.\n");
    return 0;
}
/**
 * @brief 保存两线网络参数配置，只能在打开flash任务中调用
 *
 * @param ffd
 * @return int
 */

int flash_save_atbcfg(int ffd)
{
    int iret;
    uint32_t temp;
    uint32_t uary[2];
    atbcfg.crc = FLASH_CRC;
    /* flash, unlock */
    uary[0] = 0;
    ioctl(ffd, 2, sizeof(uint32_t), &uary[0]);
    /* flash, erase, ofs = 2M, 4K */
    uary[0] = CFG_ATB_OFFSET;
    uary[1] = 0x1000;
    ioctl(ffd, 3, 2 * sizeof(uint32_t), &uary[0]);
    /* seek and write */
    uary[0] = CFG_ATB_OFFSET;
    ioctl(ffd, 0, sizeof(uint32_t), &uary[0]);
    // atbcfg.dynband = 1;
    iret = write(ffd, (void *)&atbcfg, sizeof(atbcfg_t));
    if (iret != sizeof(atbcfg_t))
    {
        LOG_OUT(LOG_DBG, "write atb cfg to flash fail.\n");
        return iret;
    }
    if (atbcfg.dynband == 1)
    {
        /* flash, erase precfg */
        uary[0] = PRE_RES_OFFSET;
        uary[1] = 0x1000;
        ioctl(ffd, 3, 2 * sizeof(uint32_t), &uary[0]);
        /* flash, erase rescfg */
        uary[0] = CFG_RES_OFFSET;
        uary[1] = ((sizeof(resmgr_config_t) / 0x1000) + 1) * 0x1000;
        ioctl(ffd, 3, 2 * sizeof(uint32_t), &uary[0]);
    }
    LOG_OUT(LOG_INFO, "save atb cfg success.\n");

    return 0;
}

/**
 * @brief  读取资源配置，只能在打开flash的任务中调用
 *
 * @param ffd
 * @return int
 */
int flash_load_rescfg(int ffd)
{
    int iret , i;
    uint32_t temp;
    uint8_t tary[64];
    resmgr_config_t *presmgr;
    atbcfg_t * pcfg;

    set_resmgr(NULL);
    temp = CFG_RES_OFFSET;
    ioctl(ffd, 0, sizeof(temp), &temp);
    presmgr = get_resmgr();
    if (presmgr == NULL)
    {
        LOG_OUT(LOG_ERR, "load res cfg fail.\n");
        return -1;
    }

    iret = read(ffd, presmgr, sizeof(resmgr_config_t));
    if (iret != sizeof(resmgr_config_t))
    {
        LOG_OUT(LOG_ERR, "read flash fail,load res cfg fail.\n");
        return -1;
    }

    // /*NDT 获取上位机配置，如果模式为3模式的话，就将第61个symbol变为不可配置*/
    pcfg = get_atbcfg();
    if(pcfg -> msp == 3)
    {
        for( i = 0; i < MAX_RES_CFG_ID; i++)
        {
            if((presmgr->res_cfg_tbl[i].pos) + (presmgr->res_cfg_tbl[i].len) >= 61)
            {
                if ((presmgr->res_cfg_tbl[i].cfg_state == CONFIG_ENABLE_E))
                {
                    // printf("i = %d,presmgr->res_cfg_tbl[i].pos = %d  %d\n",i,presmgr->res_cfg_tbl[i].pos,presmgr->res_cfg_tbl[i].len); 
                    presmgr->res_cfg_tbl[i].len = presmgr->res_cfg_tbl[i].len -1;
                    
                }
            }
        } 
    }   

    memset(tary, 'r', sizeof(res_config_t));
    // printf("---------len = %d\r\n",presmgr->res_cfg_tbl[2].len);
    if (0 != memcmp(tary, presmgr, sizeof(res_config_t)))
    {
        
        set_resmgr(NULL);
        LOG_OUT(LOG_DBG, "res cfg unvalid.\n");
        return -1;
    }
    LOG_OUT(LOG_INFO, "res cfg load success.\n");

    return 0;
}

int flash_save_pre_res(int ffd)
{
    int iret;
    uint32_t temp;
    uint32_t uary[2];

    pre_res.magic = PRE_RES_MAGIC;

    /* flash, unlock */
    uary[0] = 0;
    ioctl(ffd, 2, sizeof(uint32_t), &uary[0]);
    /* flash, erase, ofs = 2M, 4K */
    uary[0] = PRE_RES_OFFSET;
    uary[1] = 0x1000;
    ioctl(ffd, 3, 2 * sizeof(uint32_t), &uary[0]);
    /* seek and write */
    uary[0] = PRE_RES_OFFSET;
    ioctl(ffd, 0, sizeof(uint32_t), &uary[0]);
    iret = write(ffd, (uint8_t*)&pre_res, sizeof(pre_res_config_t));
    if (iret != sizeof(pre_res_config_t))
    {
        return iret;
    }
    LOG_OUT(LOG_INFO, "save pres res success.\n");
    return 0;
}

int flash_load_pre_res(int ffd)
{
    int iret;
    uint32_t temp;
    uint8_t tary[64];
    
    temp = PRE_RES_OFFSET;
    ioctl(ffd, 0, sizeof(temp), &temp);


    iret = read(ffd,(uint8_t*)&pre_res, sizeof(pre_res_config_t));
    // if (iret != sizeof(resmgr_config_t))
    // {
    // 	 pre_res.num = 0;
    //     LOG_OUT(LOG_ERR, "read flash fail,load pre res fail.\n");
    //     return -1;
    // }
    if (PRE_RES_MAGIC != pre_res.magic)
    {
    	 pre_res.num = 0;
         pre_res.state = 0;
        LOG_OUT(LOG_DBG, "pre res unvalid.\n");
        return -1;
    }
    pre_res.state = 1;
    LOG_OUT(LOG_INFO, "pre res cfg load success.\n");

    return 0;
}
int flash_erase_pre_res(int ffd)
{
    int iret;
    uint32_t temp;
    uint32_t uary[2];


    pre_res.magic = PRE_RES_MAGIC;

    /* flash, unlock */
    uary[0] = 0;
    ioctl(ffd, 2, sizeof(uint32_t), &uary[0]);
    /* flash, erase, ofs = 2M, 4K */
    uary[0] = PRE_RES_OFFSET;
    uary[1] = 0x1000;
    ioctl(ffd, 3, 2 * sizeof(uint32_t), &uary[0]);
    LOG_OUT(LOG_INFO, "clr pres res success.\n");
    return 0;
}

/**
 * @brief  读取autbus网络配置，只能在打开flash的任务中调用
 *
 * @param ffd
 * @return int
 */
int flash_load_atbcfg(int ffd)
{
    int iret;
    uint32_t temp;
    void *ptr;
    uint32_t uary[2];
    
    ptr = (void *)&atbcfg;

    temp = CFG_ATB_OFFSET;
    ioctl(ffd, 0, sizeof(temp), &temp);

    iret = read(ffd, ptr, sizeof(atbcfg_t));
    if (iret != sizeof(atbcfg_t))
    {
        LOG_OUT(LOG_ERR, "read flash fail,load atb cfg fail %d.\n", iret);
        return -1;
    }
    if (FLASH_CRC != atbcfg.crc)
    {
        memset((void *)&atbcfg, 0, sizeof(atbcfg_t));
        atbcfg.role = 2;
        LOG_OUT(LOG_ERR, "autbus net cfg unvalid.\n");
        return -1;
    }
    //动态带宽
    if(atbcfg.dynband == 1)
    {
        /* flash, erase precfg */
        uary[0] = PRE_RES_OFFSET;
        uary[1] = 0x1000;
        ioctl(ffd, 3, 2 * sizeof(uint32_t), &uary[0]);
        /* flash, erase rescfg */
        // uary[0] = CFG_RES_OFFSET;
        // uary[1] = ((sizeof(resmgr_config_t) / 0x1000) + 1) * 0x1000;
        // ioctl(ffd, 3, 2 * sizeof(uint32_t), &uary[0]); //ndt 
    }
    LOG_OUT(LOG_INFO, "atb cfg load success.\n");
    
    return 0;
}
/*读取相关的设备参数*/
int flash_load_devcfg(int ffd)
{
    int iret;
    uint32_t temp;
    void *ptr;
    uint32_t uary[2];
    devcfg_t *p;
    ptr = (void *)&devcfg;
    temp = CFG_DEVICE_OFFSET;
    ioctl(ffd, 0, sizeof(temp), &temp);
    // dprintf(0, "########## %d \n", devcfg.rs485cfg[0].paritybit);
    iret = read(ffd, ptr, sizeof(devcfg_t));
    if (iret != sizeof(devcfg_t))
    {
        LOG_OUT(LOG_ERR, "read flash fail,load debug cfg fail %d.\n", iret);
        return -1;
    }
    if (FLASH_DEV_CRC != devcfg.crc)
    {
        memset((void *)&devcfg, 0, sizeof(devcfg_t));
        devcfg.rs485cfg[0].baudrate = 115200;
        devcfg.rs485cfg[0].databit = 8;
        devcfg.rs485cfg[0].stopbit = 1;
        devcfg.rs485cfg[0].paritybit = 0;
        devcfg.cancfg[0].baudrate = 3;
        devcfg.cancfg[1].baudrate = 3;
        devcfg.cancfg[2].baudrate = 3;
        memcpy(&devcfg.rs485cfg[1], &devcfg.rs485cfg[0], sizeof(rs485cfg_t));
        memcpy(&devcfg.rs485cfg[2], &devcfg.rs485cfg[0], sizeof(rs485cfg_t));
        memcpy(devcfg.nodecfg.name, "unknown", 8);
        devcfg.crc = FLASH_DEV_CRC;
        flash_save_devcfg(ffd, &devcfg, sizeof(devcfg_t));
    }

    LOG_OUT(LOG_INFO, "--------devcfg load success.\n");

    return 0;
}
/**
 * @brief Get the atbcfg object
 *
 * @return atbcfg_t*
 */
atbcfg_t *get_atbcfg()
{
    return &atbcfg;
}

uint8_t *get_maccfg()
{
    return maccfg;
}
int flash_load_maccfg(int ffd)
{
    int iret;
    uint32_t temp;
    uint8_t *ptr;
    // uint8_t mac[MAC_LEN] = {0};

    ptr = (uint8_t *)&maccfg;

    temp = CFG_MAC_OFFSET;
    ioctl(ffd, 0, sizeof(temp), &temp);

    iret = read(ffd, ptr, MAC_LEN);
    if (iret != MAC_LEN)
    {
        LOG_OUT(LOG_ERR, "read flash fail,load mac cfg fail %d.\n", iret);
        return -1;
    }
    // sysinfo(1, mac);
    // maccfg[0] = mac[0];
    // maccfg[1] = mac[1];
    // maccfg[2] = mac[2];
    svc_register_uid( maccfg );
    LOG_OUT(LOG_INFO, "mac cfg load success.\n");

    return 0;
}

int flash_save_maccfg(int ffd, void *ptr, uint32_t size)
{
    int iret;
    uint32_t temp;
    uint32_t uary[2];
    /* flash, unlock */
    uary[0] = 0;
    ioctl(ffd, 2, sizeof(uint32_t), &uary[0]);
    /* flash, erase, ofs = 2M, 4K */
    uary[0] = CFG_MAC_OFFSET;
    uary[1] = 0x1000;
    ioctl(ffd, 3, 2 * sizeof(uint32_t), &uary[0]);
    /* seek and write */
    uary[0] = CFG_MAC_OFFSET;
    ioctl(ffd, 0, sizeof(uint32_t), &uary[0]);
    iret = write(ffd, ptr, size);
    if (iret != size)
    {
        LOG_OUT(LOG_DBG, "write mac cfg to flash fail.\n");
        return iret;
    }
    LOG_OUT(LOG_INFO, "save mac cfg success.\n");
    return 0;
}
/**
 * @brief 读取调试参数，用户自定义，
 *        参考get_debug_data和set_debug_data
 *
 * @param ffd flash文件描述符
 * @param ptr 数据首地址
 * @param size 数据大小
 * @return int
 */

int flash_load_debugcfg(int ffd, void *ptr, uint32_t size)
{
    int iret;
    uint32_t temp;

    temp = CFG_DEBUG_OFFSET;
    ioctl(ffd, 0, sizeof(temp), &temp);

    iret = read(ffd, ptr, size);
    if (iret != size)
    {
        LOG_OUT(LOG_ERR, "read flash fail,load debug cfg fail %d.\n", iret);
        return -1;
    }
    LOG_OUT(LOG_INFO, "debug  cfg load success.\n");

    return 0;
}
/**
 * @brief 保存调试参数，用户自定义，
 *        参考get_debug_data和set_debug_data
 *
 * @param ffd
 * @param ptr
 * @param size
 * @return int
 */
int flash_save_debugcfg(int ffd, void *ptr, uint32_t size)
{
    int iret;
    uint32_t temp;
    uint32_t uary[2];
    /* flash, unlock */
    uary[0] = 0;
    ioctl(ffd, 2, sizeof(uint32_t), &uary[0]);
    /* flash, erase, ofs = 2M, 4K */
    uary[0] = CFG_DEBUG_OFFSET;
    uary[1] = 0x1000;
    ioctl(ffd, 3, 2 * sizeof(uint32_t), &uary[0]);
    /* seek and write */
    uary[0] = CFG_DEBUG_OFFSET;
    ioctl(ffd, 0, sizeof(uint32_t), &uary[0]);
    iret = write(ffd, ptr, size);
    if (iret != size)
    {
        LOG_OUT(LOG_DBG, "write debug cfg to flash fail.\n");
        return iret;
    }
    LOG_OUT(LOG_INFO, "save debug cfg success.\n");
    return 0;
}
int flash_save_devcfg(int ffd, void *ptr, uint32_t size)
{
    int iret;
    uint32_t temp;
    uint32_t uary[2];
    /* flash, unlock */
    uary[0] = 0;
    ioctl(ffd, 2, sizeof(uint32_t), &uary[0]);
    /* flash, erase, ofs = 2M, 4K */
    uary[0] = CFG_DEVICE_OFFSET;
    uary[1] = 0x1000;
    ioctl(ffd, 3, 2 * sizeof(uint32_t), &uary[0]);
    /* seek and write */
    uary[0] = CFG_DEVICE_OFFSET;
    ioctl(ffd, 0, sizeof(uint32_t), &uary[0]);
    iret = write(ffd, ptr, size);
    if (iret != size)
    {
        LOG_OUT(LOG_DBG, "write device cfg to flash fail.\n");
        return iret;
    }
    flash_load_devcfg(ffd);
    LOG_OUT(LOG_INFO, "save device cfg success.\n");
    return 0;
}