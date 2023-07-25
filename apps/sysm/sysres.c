
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dlist.h"
#include "hcb_comn.h"
#include "hcb_msg.h"
#include "pkt_api.h"
#include "sys_api.h"
#include "debug.h"
#include "atimer.h"
#include "sysent.h"
#include "sysmgr.h"
#include "sysrpc.h"
#include "sysres.h"
#include "atbui.h"
#include "phb1.h"
#include "resmgr.h"
uint8_t bpskey = 0;
typedef struct _tag_trs_res
{
    uint8_t dnode;
    uint8_t chan;
    uint8_t freq;
    uint8_t pos;
    uint8_t len;
} trans_res_t;

typedef enum
{
    CFG_IDLE = 3,
    CFG_NODE_ONLINE,
    CFG_NODE_OFFLINE,
} NODE_STAGE_E;

typedef enum
{
    CFG_NODE_INIT = 3,
    CFG_NODE_DOWNLOAD_RES_ID,
    CFG_NODE_WAIT_ACK,
    CFG_NODE_ADD_RCV_RES,
    CFG_NODE_ADD_TRS_RES,
    CFG_NODE_ADD_FINISH,
    CFG_NODE_FAIL,
    CFG_NODE_DEL_RES,
    CFG_NODE_DEL_FINISH,
} RES_STAGE_E;

typedef struct _tag_node_info
{
    struct list_node node;
    /*acpu软件版本号*/
    uint8_t aver[VER_LEN];
    /*bcpu软件版本号*/
    uint8_t bver[VER_LEN];
    /*iom or ddc name type*/
    uint8_t nodetype[VER_LEN];
    uint8_t name[NAME_LEN];
    uint8_t nindx;
    uint8_t sigerr;
    uint8_t report;
    uint8_t role;
    uint8_t nodeaddr;
    uint8_t listen;
    uint8_t mac[MAC_LEN];
    /*节点上线动作状态*/
    uint8_t state;
    uint8_t online;
    uint8_t offline;
    uint8_t lostcnt;
    /**
     * @brief 信道质量错误连续展示10s后清除
     *
     */
    uint8_t clearcnt;
    uint64_t onlinetime;
    uint64_t losttime;
    sysm_context_t *pctx;
} node_info_t;

/* CN info, and list head */
struct list_node ncn;

/*用于广播报文时的应答记录，每一位表示一个节点nodeid*/
static uint32_t ack_flag[8];

static int msgfd;

/**
 * @brief 设置标志位，表示节点与上位机软件连接中
 *
 * @param nodeid
 */
void set_report(uint8_t nodeid)
{
    node_info_t *pnd;
    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        if (pnd->nindx == nodeid)
        {
            pnd->report = 2;
        }
        else if (pnd->report == 2)
        {
            pnd->report = 0;
        }
    }
}

static void send_message(uint8_t *ptr, int tlen)
{
    int iret;
    if (tlen > 128)
    {
        LOG_OUT(LOG_ERR, "send message tlen err:%d, must less than 128.\n", tlen);
        return;
    }
    iret = write(msgfd, ptr, tlen);
    if (iret < 0)
    {
        LOG_OUT(LOG_ERR, "send message err.\n");
    }
}

static inline void init_ack_flag()
{
    int i;
    for (i = 0; i < 8; ++i)
    {
        ack_flag[i] = 0;
    }
}

uint32_t is_ack_zero()
{
    int i;
    uint32_t ret;
    ret = ack_flag[0];
    for (i = 1; i < 8; ++i)
    {
        ret |= ack_flag[i];
    }
    return ret;
}
static inline void clear_bit(uint8_t nid)
{
    int i;
    int j;
    i = nid / 32;
    j = nid % 32;
    ack_flag[i] &= ~(1 << j);
}
static inline void set_bit(uint8_t nid)
{
    int i;
    int j;
    i = nid / 32;
    j = nid % 32;
    ack_flag[i] |= 1 << j;
}

static inline int hcb_res_frame_test(uint32_t ecode, int frm)
{
    /* code : 4 bits,  frm : 0-7 */
    ecode = ecode & 0xF;
    frm = frm & 0x7;

    /**/
    switch (ecode)
    {
    case 1:
        /* 1: 0-7 */
        return 1;

    case 2:
        /* 2: 0,2,4,6 */
        return (0 == (frm & 0x1));

    case 3:
        /* 2: 1,3,5,7 */
        return (0 != (frm & 0x1));

    case 4:
        /* 4: 0,4 */
        return (0 == (frm & 0x3));

    case 5:
        /* 4: 1,5 */
        return (1 == (frm & 0x3));

    case 6:
        /* 4: 2,6 */
        return (2 == (frm & 0x3));

    case 7:
        /* 4: 3,7 */
        return (3 == (frm & 0x3));

    case 8:
        /* 8: 0 */
        return (frm == 0);
    case 9:
        /* 8: 1 */
        return (frm == 1);

    case 10:
        /* 8: 2 */
        return (frm == 2);

    case 11:
        /* 8: 3 */
        return (frm == 3);

    case 12:
        /* 8: 4 */
        return (frm == 4);

    case 13:
        /* 8: 5 */
        return (frm == 5);

    case 14:
        /* 8: 6 */
        return (frm == 6);

    case 15:
        /* 8: 7 */
        return (frm == 7);

    case 0:
    default:
        return 0;
    }
}

/* 8 frames,  64 symbols */
static uint64_t resfrm[8];

/**/
static void resfrm_init(void)
{
    uint64_t temp;

    /* CN 固定占用了开始 4 个 symbol 资源.. */
    temp = 0xF;
    temp = ~temp;

    /**/
    resfrm[0] = resfrm[1] = resfrm[2] = resfrm[3] = temp;
    resfrm[4] = resfrm[5] = resfrm[6] = resfrm[7] = temp;
    return;
}

int find_cont_bits(uint64_t tval, int tlen, uint8_t *ppos)
{
    int iret;
    uint64_t temp;

retry:
    /**/
    iret = __builtin_ffsll(tval);
    if (iret == 0)
    {
        return 1;
    }

    /**/
    iret = iret - 1;
    if ((iret + tlen) >= 64)
    {
        return 2;
    }

    /**/
    temp = (1ull << tlen) - 1;
    temp = temp << iret;
    if ((tval & temp) != temp)
    {
        /* skip some 1 bits */
        temp = tval + (1ull << iret);
        tval = tval & temp;
        goto retry;
    }

    *ppos = (uint8_t)iret;
    return 0;
}

/*
itvl : 1,2,4,8
return : 0 - succ, other - fail
*/
int resfrm_alloc(uint8_t itvl, uint8_t tlen, uint8_t *pfreq, uint8_t *ppos)
{
    int i, tidx;
    int iret;
    uint8_t tpos;
    uint8_t tmin;
    uint64_t temp;

    /**/
    tidx = INT32_MAX;
    tmin = 0xFF;
    if (itvl >= 8)
    {
        itvl = 8;
    }
    else if (itvl >= 4)
    {
        itvl = 4;
    }
    else if (itvl >= 2)
    {
        itvl = 2;
    }
    else
    {
        itvl = 1;
    }

    switch (itvl)
    {
    case 1:

        temp = resfrm[0] & resfrm[1] & resfrm[2] & resfrm[3];
        temp = temp & resfrm[4] & resfrm[5] & resfrm[6] & resfrm[7];
        iret = find_cont_bits(temp, tlen, &tpos);
        if (0 != iret)
        {
            iret = 10;
            break;
        }

        /* clear bits */
        temp = (1 << tlen) - 1;
        temp = ~(temp << tpos);
        resfrm[0] &= temp;
        resfrm[1] &= temp;
        resfrm[2] &= temp;
        resfrm[3] &= temp;
        resfrm[4] &= temp;
        resfrm[5] &= temp;
        resfrm[6] &= temp;
        resfrm[7] &= temp;

        /**/
        *ppos = tpos;
        *pfreq = 1;
        iret = 0;
        break;

    case 2:

        temp = resfrm[0] & resfrm[2] & resfrm[4] & resfrm[6];
        iret = find_cont_bits(temp, tlen, &tpos);
        if (0 == iret)
        {
            tidx = 0;
            tmin = tpos;
        }

        /**/
        temp = resfrm[1] & resfrm[3] & resfrm[5] & resfrm[7];
        iret = find_cont_bits(temp, tlen, &tpos);
        if ((0 == iret) && (tpos < tmin))
        {
            tidx = 1;
            tmin = tpos;
        }

        /**/
        if (tidx >= 2)
        {
            iret = 11;
            break;
        }

        /**/
        temp = (1 << tlen) - 1;
        temp = ~(temp << tmin);
        resfrm[tidx + 0] &= temp;
        resfrm[tidx + 2] &= temp;
        resfrm[tidx + 4] &= temp;
        resfrm[tidx + 6] &= temp;

        /* 2, 3 */
        *ppos = tmin;
        *pfreq = tidx + 2;
        iret = 0;
        break;

    case 4:

        for (i = 0; i < 4; i++)
        {
            temp = resfrm[i] & resfrm[i + 4];
            iret = find_cont_bits(temp, tlen, &tpos);
            if ((0 == iret) && (tpos < tmin))
            {
                tidx = i;
                tmin = tpos;
            }
        }

        /**/
        if (tidx >= 4)
        {
            iret = 12;
            break;
        }

        /* 4, 5, 6, 7 */
        temp = (1 << tlen) - 1;
        temp = ~(temp << tmin);
        resfrm[tidx + 0] &= temp;
        resfrm[tidx + 4] &= temp;

        /**/
        *ppos = tmin;
        *pfreq = tidx + 4;
        iret = 0;
        break;

    case 8:

        for (i = 0; i < 8; i++)
        {
            temp = resfrm[i];
            iret = find_cont_bits(temp, tlen, &tpos);
            if ((0 == iret) && (tpos < tmin))
            {
                tidx = i;
                tmin = tpos;
            }
        }

        /**/
        if (tidx >= 8)
        {
            iret = 13;
            break;
        }

        /**/
        temp = (1 << tlen) - 1;
        temp = ~(temp << tmin);
        resfrm[tidx] &= temp;

        /* 8, 9, 10, 11 */
        *ppos = tmin;
        *pfreq = tidx + 8;
        break;

    default:
        iret = 14;
        break;
    }

    /**/
    return iret;
}

/*
freq : 1, 2(0,1), 4(0,1,2,3), 8(xxxx)
*/
void resfrm_free(uint8_t freq, uint8_t tpos, uint8_t tlen)
{
    int i;
    uint64_t temp;

    /**/
    temp = (1ull << tlen) - 1;
    temp = temp << tpos;

    /**/
    for (i = 0; i < 8; i++)
    {
        if (hcb_res_frame_test(freq, i))
        {
            /**/
            resfrm[i] |= temp;
        }
    }

    return;
}

int debug_cmd_res(void *parg, int argc, const char **argv)
{
    int i;
    uint32_t t0, t1;

    /**/
    printf("\n");
    for (i = 0; i < 8; i++)
    {
        t0 = (uint32_t)(resfrm[i] & 0xFFFFFFFF);
        t1 = (uint32_t)((resfrm[i] >> 32) & 0xFFFFFFFF);

        printf("res[%d] : %08x %08x\n", i, t1, t0);
    }

    return 0;
}

/**
 * @brief 资源添加命令行
 *
 * @param parg
 * @param argc
 * @param argv
 * @return int
 */
int debug_cmd_add_res(void *parg, int argc, const char **argv)
{
    int i;
    int iret;
    node_info_t *pnd;
    uint32_t temp;
    res_config_t res;
    uint8_t nodeid;
    uint8_t resid;

    /**/
    if (argc < 5)
    {
        goto usage;
    }

    /**/
    iret = debug_str2uint(argv[1], &temp);
    if (iret != 0)
    {
        printf("resid fmt err\n");
        goto usage;
    }
    resid = (uint8_t)temp;

    /**/
    iret = debug_str2uint(argv[2], &temp);
    if (iret != 0)
    {
        printf("chan fmt err\n");
        goto usage;
    }
    res.chan = (uint8_t)temp;

    iret = debug_str2uint(argv[3], &temp);
    if (iret != 0)
    {
        printf("freq fmt err\n");
        goto usage;
    }
    res.freq = (uint8_t)temp;
    iret = debug_str2uint(argv[4], &temp);
    if (iret != 0)
    {
        printf("len fmt err\n");
        goto usage;
    }
    res.len = (uint8_t)temp;

    res.pos = 0;
    if (argc > 5)
    {
        iret = debug_str2uint(argv[5], &temp);
        if (iret != 0)
        {
            printf("pos fmt err\n");
            goto usage;
        }
        res.pos = temp & 0x3f;
    }
    add_res_config(resid, &res);
    return 0;
usage:
    printf("usage: %s <resid>  <chan> <freq> <len> [pos]\n", argv[0]);
    return 0;
}
#define MAX_DYN_NODE 32
uint8_t res_hold[MAX_DYN_NODE];
void config_cn_res(void)
{
	res_config_t res;
	if(pre_res.num == 0)
		return;
	res.chan = pre_res.chan;
	res.freq = pre_res.freq; 
	res.pos = 0;
	res.len = pre_res.len;
	add_res_config(1, &res);
	for(int i = 0;i<pre_res.num - 1;i++)
	{
		add_res_config(2+i, &res);		
	}
}
/**
 * @brief 删除一段资源 配置
 *
 * @param parg
 * @param argc
 * @param argv
 * @return int
 */

int debug_cmd_del_res(void *parg, int argc, const char **argv)
{
    int i;
    int iret;
    uint32_t temp;
    uint8_t resid;

    /**/
    if (argc < 2)
    {
        goto usage;
    }

    /**/
    iret = debug_str2uint(argv[1], &temp);
    if (iret != 0)
    {
        printf("resid fmt err\n");
        goto usage;
    }
    resid = (uint8_t)temp;
    del_res_config(resid);
    return 0;
usage:
    printf("usage: %s <resid>\n", argv[0]);
    return 0;
}

/**
 * @brief 添加发送资源，只能在节点在线时修改，因为需要根据nodeid去查找其 mac地址
 *        而节点的nodeid只有在节点上线后才会生成。
 *        直接输入mac地址会比较麻烦，后面再实现
 *
 * @param parg
 * @param argc
 * @param argv
 * @return int
 */

int debug_cmd_add_trs_res(void *parg, int argc, const char **argv)
{
    int i;
    int iret;
    node_info_t *pnd;
    uint32_t temp;
    uint8_t nodeid;
    uint8_t resid;
    uint8_t mac[MAC_LEN];

    /**/
    if (argc < 3)
    {
        goto usage;
    }
    /**/
    iret = debug_str2uint(argv[1], &temp);
    if (iret != 0)
    {
        printf("resid fmt err\n");
        goto usage;
    }

    resid = (uint8_t)temp;
    /**/
    iret = debug_str2uint(argv[2], &temp);
    if (iret != 0)
    {
        printf("nodeid fmt err\n");
        goto usage;
    }

    nodeid = (uint8_t)temp;
    iret = 1;
    /*根据nodeid去查找节点mac地址，添加资源配置的时候需要*/
    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        if (pnd->nindx == nodeid)
        {
            memcpy((void *)mac, pnd->mac, MAC_LEN);
            iret = 0;
        }
    }
    if (iret != 0)
    {
        printf("do not find node %d,mac empty,can not set trs res.\n", nodeid);
        return 0;
    }

    if (argc > 3)
    {

        /**/
        iret = debug_str2uint(argv[3], &temp);
        if (iret != 0)
        {
            printf("send2all fmt err\n");
            goto usage;
        }
        add_trs_res(resid, mac, temp & 0xff);
    }
    else
    {
        add_trs_res(resid, mac, 0);
    }

    return 0;
usage:
    printf("usage: %s <resid> <nodeid> [send2all]\n", argv[0]);
    return 0;
}

/**
 * @brief 添加发送资源，只能在节点在线时修改，因为需要根据nodeid去查找其 mac地址
 *        而节点的nodeid只有在节点上线后才会生成。
 *        直接输入mac地址会比较麻烦，后面再实现
 *
 * @param parg
 * @param argc
 * @param argv
 * @return int
 */

int debug_cmd_add_rcv_res(void *parg, int argc, const char **argv)
{
    int i;
    int iret;
    node_info_t *pnd;
    uint32_t temp;
    uint8_t nodeid;
    uint8_t resid;
    uint8_t mac[MAC_LEN];

    /**/
    if (argc < 3)
    {
        goto usage;
    }
    /**/
    iret = debug_str2uint(argv[1], &temp);
    if (iret != 0)
    {
        printf("resid fmt err\n");
        goto usage;
    }

    resid = (uint8_t)temp;
    /**/
    iret = debug_str2uint(argv[2], &temp);
    if (iret != 0)
    {
        printf("nodeid fmt err\n");
        goto usage;
    }

    nodeid = (uint8_t)temp;
    iret = 1;
    /*根据nodeid去查找节点mac地址，添加资源配置的时候需要*/
    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        if (pnd->nindx == nodeid)
        {
            memcpy((void *)mac, pnd->mac, MAC_LEN);
            iret = 0;
        }
    }
    if (iret != 0)
    {
        printf("do not find node %d,mac empty,can not set recv res.\n", nodeid);
        return 0;
    }
    add_rcv_res(resid, mac);
    return 0;
usage:
    printf("usage: %s  <resid> <nodeid> \n", argv[0]);
    return 0;
}

/**
 * @brief  删除某节点发送资源
 *
 * @param parg
 * @param argc
 * @param argv
 * @return int
 */

int debug_cmd_del_trs_res(void *parg, int argc, const char **argv)
{
    int i;
    int iret;
    node_info_t *pnd;
    uint32_t temp;
    uint8_t nodeid;
    uint8_t resid;
    uint8_t mac[MAC_LEN];

    /**/
    if (argc < 3)
    {
        goto usage;
    }
    /**/
    iret = debug_str2uint(argv[1], &temp);
    if (iret != 0)
    {
        printf("resid fmt err\n");
        goto usage;
    }

    resid = (uint8_t)temp;
    /**/
    iret = debug_str2uint(argv[2], &temp);
    if (iret != 0)
    {
        printf("nodeid fmt err\n");
        goto usage;
    }

    nodeid = (uint8_t)temp;
    iret = 1;
    /*根据nodeid去查找节点mac地址，添加资源配置的时候需要*/
    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        if (pnd->nindx == nodeid)
        {
            memcpy((void *)mac, pnd->mac, MAC_LEN);
            iret = 0;
        }
    }
    if (iret != 0)
    {
        printf("do not find node %d,mac empty,can not set trs res.\n", nodeid);
        return 0;
    }
    del_trs_res(resid, mac);
    return 0;
usage:
    printf("usage: %s <resid> <nodeid> \n", argv[0]);
    return 0;
}

/**
 * @brief 删除某节点接收资源，只能在节点在线时修改，因为需要根据nodeid去查找其 mac地址
 *        而节点的nodeid只有在节点上线后才会生成。
 *        直接输入mac地址会比较麻烦，后面再实现
 *
 * @param parg
 * @param argc
 * @param argv
 * @return int
 */

int debug_cmd_del_rcv_res(void *parg, int argc, const char **argv)
{
    int i;
    int iret;
    node_info_t *pnd;
    uint32_t temp;
    uint8_t nodeid;
    uint8_t resid;
    uint8_t mac[MAC_LEN];

    /**/
    if (argc < 3)
    {
        goto usage;
    }
    /**/
    iret = debug_str2uint(argv[1], &temp);
    if (iret != 0)
    {
        printf("resid fmt err\n");
        goto usage;
    }

    resid = (uint8_t)temp;
    /**/
    iret = debug_str2uint(argv[2], &temp);
    if (iret != 0)
    {
        printf("nodeid fmt err\n");
        goto usage;
    }

    nodeid = (uint8_t)temp;
    iret = 1;
    /*根据nodeid去查找节点mac地址，添加资源配置的时候需要*/
    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        if (pnd->nindx == nodeid)
        {
            memcpy((void *)mac, pnd->mac, MAC_LEN);
            iret = 0;
        }
    }
    if (iret != 0)
    {
        printf("do not find node %d,mac empty,can not set recv res.\n", nodeid);
        return 0;
    }
    del_rcv_res(resid, mac);
    return 0;
usage:
    printf("usage: %s  <resid> <nodeid> \n", argv[0]);
    return 0;
}

/**
 * @brief 删除某节点资源配置，只能在节点在线时修改，因为需要根据nodeid去查找其 mac地址
 *        而节点的nodeid只有在节点上线后才会生成。
 *        直接输入mac地址会比较麻烦，后面再实现
 *
 * @param parg
 * @param argc
 * @param argv
 * @return int
 */
int debug_cmd_del_node_config(void *parg, int argc, const char **argv)
{
    int i;
    int iret;
    node_info_t *pnd;
    uint32_t temp;
    uint8_t nodeid;
    uint8_t mac[MAC_LEN];

    /**/
    if (argc < 2)
    {
        goto usage;
    }
    /**/
    iret = debug_str2uint(argv[1], &temp);
    if (iret != 0)
    {
        printf("nodeid fmt err\n");
        goto usage;
    }

    nodeid = (uint8_t)temp;
    iret = 1;
    /*根据nodeid去查找节点mac地址，添加资源配置的时候需要*/
    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        if (pnd->nindx == nodeid)
        {
            memcpy((void *)mac, pnd->mac, MAC_LEN);
            iret = 0;
        }
    }
    if (iret != 0)
    {
        printf("do not find node %d,mac empty,can not set recv res.\n");
        return 0;
    }
    del_node_config(mac);
    return 0;
usage:
    printf("usage: %s   <nodeid> \n", argv[0]);
    return 0;
}

int res_push_commit(void)
{
    uint8_t tary[16];
    hcb_msg_hdr *phdr;

    /* 检查是否有积压的..  */

    /* 发起 commit */
    phdr = (hcb_msg_hdr *)&tary[1];
    phdr->type = HCB_MSG_COMMIT_FIXS;
    phdr->leng = sizeof(hcb_msg_hdr);
    phdr->flag = 0;
    phdr->info = 0;
    tary[0] = 'c';
    send_message(tary, sizeof(hcb_msg_hdr) + 4);
    return 0;
}

int res_add_rcv_res(node_info_t *pnd);
int res_id_rpc_cbk(void *parg, int ilen, void *ibuf)
{
    uint8_t *pnid;
    node_info_t *pnd;
    pnd = (node_info_t *)parg;
  
    pnd->nodeaddr = pnid[1];
    if (ibuf == NULL)
    {
        LOG_OUT(LOG_ERR, "res id ack timeout err.\n");
        pnd->state = CFG_NODE_FAIL;
        return 1;
    }
    pnid = (uint8_t *)ibuf;
    memcpy(pnd->aver, pnid + 2, VER_LEN);
    memcpy(pnd->bver, pnid + VER_LEN + 2, VER_LEN);
    memcpy(pnd->nodetype, pnid + VER_LEN * 2 + 2, VER_LEN);
    memcpy(pnd->name, pnid + VER_LEN * 3 + 2, NAME_LEN);
    pnd->listen = pnid[VER_LEN * 3 + 2 + NAME_LEN];
    pnd->aver[VER_LEN - 1] = 0;
    pnd->bver[VER_LEN - 1] = 0;
    /**/
    LOG_OUT(LOG_INFO, "name:%s,aver:%s--bver:%s.\n", pnd->name, pnd->aver, pnd->bver);
    LOG_OUT(LOG_INFO, "res id ack node %d\n", pnid[0]);

    pnd = (node_info_t *)parg;
    pnd->state = CFG_NODE_ADD_RCV_RES;

    return 0;
}
int res_download_res_id(node_info_t *pnd);
int res_add_trs_rpc_cbk(void *parg, int ilen, void *ibuf)
{
    uint8_t *pnid;
    node_info_t *pnd;
    pnd = (node_info_t *)parg;
    if ((ilen != 0) && (ibuf != NULL))
    {
        pnid = (uint8_t *)ibuf;
        LOG_OUT(LOG_INFO, "trs res ack node, %d\n", pnid[0]);
        clear_bit(pnid[0]);
    }
    else
    {
        if (0 != is_ack_zero())
        {
            pnd->state = CFG_NODE_FAIL;
            LOG_OUT(LOG_INFO, "node %d trs rpc timeout\n", pnd->nindx);
        }
    }
    /*判断是否所有节点都应答了*/
    if (0 == is_ack_zero())
    {
        /**/
        pnd->state = CFG_NODE_ADD_FINISH;
        LOG_OUT(LOG_INFO, "all node ack ok.\n");
        return 0;
    }
    /**/
    return 1;
}
int res_add_trs_res(node_info_t *pnd);
int res_add_rcv_rpc_cbk(void *parg, int ilen, void *ibuf)
{
    uint8_t *pnid;
    node_info_t *pnd;
    node_info_t *ptmp;
    pnd = (node_info_t *)parg;

    /**/
    if (ibuf == NULL)
    {
        LOG_OUT(LOG_ERR, "rcv res ack timeout err.\n");
        pnd->state = CFG_NODE_FAIL;
        return 1;
    }
    pnid = (uint8_t *)ibuf;
    LOG_OUT(LOG_INFO, "rcv res ack node, %d\n", pnid[0]);

    // res_add_trs_res(pnd);
    pnd->state = CFG_NODE_ADD_TRS_RES;

    return 0;
}

int res_download_res_id(node_info_t *pnd)
{
    int iret;
    intptr_t irpc;
    node_config_t *pncfg;
    uint8_t tary[128];

    pncfg = (node_config_t *)&tary[8];
    /*获取 节点 资源id表,如果没有相关资源配置，直接返回了*/
    iret = get_node_res_id(pnd->mac, pncfg);
    if (iret != 0)
    {
        memset(pncfg->mac, 0X66, MAC_LEN);
    }
    /**/
    iret = rpc_request_create(pnd->nindx, 0x12, &irpc);
    if (iret != 0)
    {
        pnd->state = CFG_NODE_FAIL;
        return 1;
    }

    rpc_request_set_callbk(irpc, res_id_rpc_cbk, pnd);
    tary[0] = 'r';
    memcpy(&tary[4], &irpc, 4);
    send_message(tary, sizeof(node_config_t) + 8);

    /**/
    return 0;
}
int devset_rpc_cbk(void *parg, int ilen, void *ibuf)
{
    if (ibuf == NULL)
    { 
        return 1;
    }
    if (ilen != DEV_SET_SIZE)
    {
        return 0;
    }
    memcpy(atbui_cmd.dev_set_buf, ibuf, DEV_SET_SIZE);
    return 0;
}

int atbsig_rpc_cbk(void *parg, int ilen, void *ibuf)
{
    if (ilen != UI_BUF_SIZE)
    {
        return 0;
    }
    memcpy(atbui_cmd.ui_dat_buf, ibuf, UI_BUF_SIZE);
    return 0;
}

int dyn_rpc_cbk(void *parg, int ilen, void *ibuf)
{
   if (ibuf == NULL)
    { 
        return 1;
    }
    if (ilen != 16)
    {
        return 0;
    }
    return 0;
}
static int uisend_dummy_cbk(void *parg, int ilen, void *ibuf)
{
    // te = arch_timer_get_current();
    // dprintf(0, "timer end:%U--%U---%U\n", ts, te, te - ts);
    return 0;
}

int rpc_get_atbsig(int nodeid)
{
    int iret;
    intptr_t irpc;
    uint8_t tary[128];

    /**/
    iret = rpc_request_create(nodeid, 0x16, &irpc);
    if (iret != 0)
    {
        return 1;
    }
    rpc_request_set_retry(irpc, 0, 1000000);

    rpc_request_set_callbk(irpc, atbsig_rpc_cbk, NULL);
    rpc_request_send(irpc, tary, 4);
    // LOG_OUT(LOG_DBG, "tn atb sig:%d\n",nodeid);
    /**/
    return 0;
}

int rpc_ota_start()
{
    int iret;
    intptr_t irpc;
    /**/
    iret = rpc_request_create(0xff, 0x19, &irpc);
    if (iret != 0)
    {
        return 1;
    }
    rpc_request_set_retry(irpc, 0, 1000000);
    rpc_request_send(irpc, atbui_cmd.ui_dat_buf, UI_BUF_SIZE);
    /**/
    return 0;
}

int rpc_ota_data()
{
    int iret;
    intptr_t irpc;
    /**/
    iret = rpc_request_create(0xff, 0x20, &irpc);
    if (iret != 0)
    {
        return 1;
    }
    rpc_request_set_retry(irpc, 0, 1000000);
    rpc_request_send(irpc, atbui_cmd.ui_dat_buf, UI_BUF_SIZE);
    /**/
    return 0;
}

int rpc_ota_end()
{
    int iret;
    intptr_t irpc;
    /**/
    iret = rpc_request_create(0xff, 0x21, &irpc);
    if (iret != 0)
    {
        return 1;
    }
    rpc_request_set_retry(irpc, 0, 1000000);
    rpc_request_send(irpc, atbui_cmd.ui_dat_buf, UI_BUF_SIZE);
    /**/
    return 0;
}

int rpc_get_debug(int nodeid)
{
    int iret;
    intptr_t irpc;
    uint8_t tary[32];

    /**/
    iret = rpc_request_create(nodeid, 0x17, &irpc);
    if (iret != 0)
    {
        return 1;
    }
    rpc_request_set_retry(irpc, 0, 1000000);
    rpc_request_set_callbk(irpc, atbsig_rpc_cbk, NULL);
    rpc_request_send(irpc, tary, 4);
    /**/
    return 0;
}

int rpc_get_bps(int nodeid)
{
    int iret;
    intptr_t irpc;
    uint8_t tary[32];

    /**/
    iret = rpc_request_create(nodeid, 0x24, &irpc);
    if (iret != 0)
    {
        return 1;
    }
    rpc_request_set_retry(irpc, 0, 1000000);
    rpc_request_set_callbk(irpc, atbsig_rpc_cbk, NULL);
    rpc_request_send(irpc, tary, 4);
    /**/
    return 0;
}

int rpc_set_dyn(int nodeid, uint8_t dynband)
{
    int iret;
    intptr_t irpc;
    uint8_t tary[32];
    tary[0] = dynband;
    /**/
    iret = rpc_request_create(nodeid, 0x26, &irpc);
    if (iret != 0)
    {
        return 1;
    }
    rpc_request_set_retry(irpc, 0, 1000000);
    rpc_request_set_callbk(irpc, dyn_rpc_cbk, NULL);
    rpc_request_send(irpc, tary, 4);
    /**/
    return 0;
}
int rpc_ui_send(int nodeid, uint8_t *pbuf, uint32_t tlen)
{
    int iret;
    intptr_t irpc;

    /**/
    iret = rpc_request_create(nodeid, 0x25, &irpc);
    if (iret != 0)
    {
        return 1;
    }
    rpc_request_set_retry(irpc, 3, 1000000);
    rpc_request_set_callbk(irpc, uisend_dummy_cbk, NULL);
    iret = rpc_request_send(irpc, pbuf, tlen);

    /**/
    return 0;
}

int rpc_tn_send(int nodeid, uint8_t *pbuf, uint32_t tlen)
{
    int iret;
    intptr_t irpc;

    /**/
    iret = rpc_request_create(nodeid, 0x27, &irpc);
    if (iret != 0)
    {
        return 1;
    }
    rpc_request_set_retry(irpc, 3, 1000000);
    rpc_request_set_callbk(irpc, uisend_dummy_cbk, NULL);
    iret = rpc_request_send(irpc, pbuf, tlen);

    /**/
    return 0;
}

int rpc_get_device(int nodeid, sysm_context_t *pctx)
{
    int iret;
    intptr_t irpc;
    uint8_t tary[32];

    /**/
    iret = rpc_request_create(nodeid, 0x22, &irpc);
    if (iret != 0)
    {
        return 1;
    }
    rpc_request_set_retry(irpc, 0, 1000000);
    rpc_request_set_callbk(irpc, devset_rpc_cbk, pctx);
    rpc_request_send(irpc, atbui_cmd.dev_set_buf, DEV_SET_SIZE);

    /**/
    return 0;
}
int rpc_set_debug(int nodeid)
{
    int iret;
    intptr_t irpc;
    uint8_t tary[32];

    /**/
    iret = rpc_request_create(nodeid, 0x18, &irpc);
    if (iret != 0)
    {
        return 1;
    }
    rpc_request_set_retry(irpc, 0, 1000000);
    rpc_request_send(irpc, atbui_cmd.ui_dat_buf, DEBUG_BUF_SIZE);
    /**/
    return 0;
}
int rpc_set_device(int nodeid)
{
    int iret;
    intptr_t irpc;
    uint8_t tary[32];

    /**/
    iret = rpc_request_create(nodeid, 0x23, &irpc);
    if (iret != 0)
    {
        return 1;
    }
    rpc_request_set_retry(irpc, 0, 1000000);
    rpc_request_send(irpc, atbui_cmd.dev_set_buf, DEV_SET_SIZE);
    /**/
    return 0;
}
int debug_cmd_test(void *parg, int argc, const char **argv)
{
#if 0
    node_info_t *pnd;
    list_for_every_entry(&(ncn.node), pnd, node_info_t, node)
    {
        if (pnd->nindx == 2)
        {
            res_download_res_id(pnd);
        }
    }
#endif
    uint64_t data = 1311768467139281697;
    printf("test print u64 %ld--%lx\n", data, data);

    return 0;
}
/**
 * @brief 广播，下发节点的发送资源，所有节点接收
 *
 * @param pnd
 * @return int
 */

int res_add_trs_res(node_info_t *pnd)
{
    int i;
    int j;
    int iret;
    intptr_t irpc;
    hcb_msg_hdr *phdr;
    node_info_t *ptmp;
    hcb_rcv_fixres *prcv;
    hcb_fixres_t *res_trs;
    uint8_t tary[256];
    uint8_t ltary[64];
    res_trs = (hcb_fixres_t *)&tary[8];

    /*获取 节点 发送资源列表,如果没有发送资源，则直接调用回调函数 */
    iret = get_trs_res_config(pnd->mac, res_trs);
    if (iret != 0)
    {
        pnd->state = CFG_NODE_ADD_FINISH;
        // res_add_trs_rpc_cbk(pnd, NULL, NULL);
        return 1;
    }
    /*分配资源*/
    for (i = 0; i < res_trs->rescnt; i++)
    {
        if (res_trs->res.trs[i].pos == 0)
        {
            iret = resfrm_alloc(res_trs->res.trs[i].freq, res_trs->res.trs[i].len, &res_trs->res.trs[i].freq, &res_trs->res.trs[i].pos);
            if (iret != 0)
            {
                LOG_OUT(LOG_ERR, "get trs res fail, nodeid %d,resid %d.\n", ptmp->nindx, res_trs->res.trs[i].resid);
                // res_add_trs_rpc_cbk(pnd, NULL, NULL);
                pnd->state = CFG_NODE_ADD_FINISH;
                return 1;
            }
        }
        active_res(res_trs->res.trs[i].resid, pnd->nindx, res_trs->res.trs[i].freq, res_trs->res.trs[i].pos);
    }
    res_trs->snode = pnd->nindx;

    /*CN本地检查是否需要接收该资源*/
    phdr = (hcb_msg_hdr *)&ltary[1];
    phdr->flag = 0;
    phdr->info = 0;
    phdr->type = HCB_MSG_ADD_RCV_FIXS;
    phdr->leng = sizeof(hcb_msg_hdr) + sizeof(hcb_rcv_fixres);
    prcv = (hcb_rcv_fixres *)(phdr + 1);
    for (i = 0; i < res_trs->rescnt; i++)
    {
        /*广播检查*/
        if ((pnd->pctx->rcvall != 0) || (res_trs->res.trs[i].send2all != 0))
        {
            prcv->snode = res_trs->snode;
            prcv->pos = res_trs->res.trs[i].pos;
            prcv->freq = res_trs->res.trs[i].freq;
            prcv->len = res_trs->res.trs[i].len;
            ltary[0] = 'c';
            send_message(ltary, sizeof(hcb_msg_hdr) + sizeof(hcb_rcv_fixres) + 4);
            continue;
        }
        for (j = 0; j < MAX_RCV_RES_CNT; j++)
        {
            if (res_trs->res.trs[i].resid == pnd->pctx->rcv_res_id[j])
            {
                prcv->snode = res_trs->snode;
                prcv->pos = res_trs->res.trs[i].pos;
                prcv->freq = res_trs->res.trs[i].freq;
                prcv->len = res_trs->res.trs[i].len;
                ltary[0] = 'c';
                send_message(ltary, sizeof(hcb_msg_hdr) + sizeof(hcb_rcv_fixres) + 4);
                LOG_OUT(LOG_INFO, "cn add rcv res,%d\n", res_trs->res.trs[i].resid);
            }
        }
    }

    /*广播报文，初始化应答标记*/
    init_ack_flag();
    list_for_every_entry(&(ncn), ptmp, node_info_t, node)
    {
        if ((ptmp->nindx > 0) && (ptmp->offline == 0) && (ptmp->state == CFG_NODE_ADD_FINISH))
        {
            LOG_OUT(LOG_INFO, "send trs res to node %d.\n", ptmp->nindx);
            set_bit(ptmp->nindx);
        }
    }

    /**/
    iret = rpc_request_create(0xff, 0x13, &irpc);
    if (iret != 0)
    {
        pnd->state = CFG_NODE_FAIL;
        LOG_OUT(LOG_ERR, "rpc req create fail,node:%s,%d\n", pnd->name, pnd->nindx);
        return 1;
    }

    rpc_request_set_callbk(irpc, res_add_trs_rpc_cbk, pnd);
    tary[0] = 'r';
    memcpy(&tary[4], &irpc, 4);
    send_message(tary, sizeof(hcb_fixres_t) + 8);
    /**/
    return 0;
}

/**
 * @brief 节点上线第三条报文，发送节点需要接收的资源
 *
 * @param pnd
 * @return int
 */

int res_add_rcv_res(node_info_t *pnd)
{
    int i;
    int iret;
    intptr_t irpc;
    node_info_t *ptmp;
    uint8_t tary[128];
    hcb_fixres_t *res_rcv;
    res_rcv = (hcb_fixres_t *)&tary[8];

    /*获取 节点 接收资源列表,如果没有接收资源，则添加发送资源 */
    iret = get_rcv_res_config(pnd->mac, res_rcv);
    if (iret != 0)
    {
        pnd->state = CFG_NODE_ADD_TRS_RES;
        return 1;
    }
    /**/
    iret = rpc_request_create(pnd->nindx, 0x14, &irpc);
    if (iret != 0)
    {
        pnd->state = CFG_NODE_FAIL;
        LOG_OUT(LOG_ERR, "rpc req create fail,node:%s,%d\n", pnd->name, pnd->nindx);
        return 1;
    }

    rpc_request_set_callbk(irpc, res_add_rcv_rpc_cbk, pnd);
    tary[0] = 'r';
    memcpy(&tary[4], &irpc, 4);
    send_message(tary, sizeof(hcb_fixres_t) + 8);

    /**/
    return 0;
}

/**
 * @brief 更新CN上线信息
 *
 * @param nindx
 * @param pctx
 * @return int
 */
int res_new_cn(uint8_t nindx, sysm_context_t *pctx)
{
    uint8_t *ptr;
    node_info_t *pnd;
    pnd = (node_info_t *)malloc(sizeof(node_info_t));
    if (pnd == NULL)
    {
        return 1;
    }
    memset(pnd, 0, sizeof(node_info_t));
    ptr = get_build_time();
    memcpy(pnd->aver, ptr, VER_LEN);
    pnd->aver[VER_LEN - 1] = 0;
    memcpy(pnd->bver, pctx->bver, VER_LEN);
    memcpy(pnd->nodetype, "CNSDK", 9);
    memcpy(pnd->name, devcfg.nodecfg.name, NAME_LEN); 
    pnd->nodeaddr = devcfg.nodecfg.addr;   
    pnd->listen = devcfg.nodecfg.listenautbus; 
    pnd->role = 0;
    /* resource zero */
    pnd->nindx = nindx;
    extern uint64_t systime;
    pnd->onlinetime = systime;
    pnd->pctx = pctx;
    memcpy(pnd->mac, pctx->mac, MAC_LEN);
    if (pre_res.state == 1)
    {
        config_cn_res();
        add_trs_res(1,pnd->mac,1);
        memset(res_hold, 0, MAX_DYN_NODE);
    }
    pnd->online = 1;
    pnd->state = CFG_NODE_INIT;
    list_add_tail(&(ncn), &(pnd->node));
    LOG_OUT(LOG_INFO, "cn online %d,%s.\n", pnd->nindx, pnd->name);
    return 0;
}

int res_add_cn(node_info_t *pnd)
{
    int iret;
    int i;
    int j;
    int flag;
    node_config_t *pncfg;
    hcb_fixres_t *fixres;
    hcb_msg_hdr *phdr;
    hcb_rcv_fixres *prcv;
    hcb_trs_fixres *ptrs;
    uint8_t tary[128];
    uint8_t ltary[64];
    /*获取 节点 资源id表,如果没有相关资源配置，直接返回了*/
    pncfg = (node_config_t *)tary;
    iret = get_node_res_id(pnd->mac, pncfg);
    if (iret != 0)
    {
        pnd->state = CFG_NODE_ADD_FINISH;
        return 1;
    }
    pnd->pctx->rcvall = pncfg->rcvall;
    memcpy((void *)pnd->pctx->trs_res_id, (void *)pncfg->trs_res_id, MAX_TRS_RES_CNT);
    memcpy((void *)pnd->pctx->rcv_res_id, (void *)pncfg->rcv_res_id, MAX_RCV_RES_CNT);
    /*下载接收资源表*/
    fixres = (hcb_fixres_t *)&tary[1];
    iret = get_rcv_res_config(pnd->mac, fixres);
    if (iret == 0)
    {
        phdr = (hcb_msg_hdr *)&ltary[1];
        phdr->flag = 0;
        phdr->info = 0;
        phdr->type = HCB_MSG_ADD_RCV_FIXS;
        phdr->leng = sizeof(hcb_msg_hdr) + sizeof(hcb_rcv_fixres);
        prcv = (hcb_rcv_fixres *)(phdr + 1);
        for (i = 0; i < fixres->rescnt; i++)
        {
            prcv->snode = fixres->res.rcv[i].snode;
            prcv->pos = fixres->res.rcv[i].pos;
            prcv->freq = fixres->res.rcv[i].freq;
            prcv->len = fixres->res.rcv[i].len;
            ltary[0] = 'c';
            send_message(ltary, sizeof(hcb_msg_hdr) + sizeof(hcb_rcv_fixres) + 4);
            LOG_OUT(LOG_INFO, "cn add rcv res,%d\n", fixres->res.rcv[i].resid);
        }
    }

    iret = get_trs_res_config(pnd->mac, fixres);
    if (iret == 0)
    {
        phdr = (hcb_msg_hdr *)&ltary[1];
        phdr->flag = 0;
        phdr->info = 0;
        phdr->type = HCB_MSG_ADD_TRS_FIXS;
        phdr->leng = sizeof(hcb_msg_hdr) + sizeof(hcb_trs_fixres);
        ptrs = (hcb_trs_fixres *)(phdr + 1);
        /*分配资源*/
        for (i = 0; i < fixres->rescnt; i++)
        {
            if (fixres->res.trs[i].pos == 0)
            {
                iret = resfrm_alloc(fixres->res.trs[i].freq, fixres->res.trs[i].len, &fixres->res.trs[i].freq, &fixres->res.trs[i].pos);
                if (iret != 0)
                {
                    LOG_OUT(LOG_ERR, "get trs res fail, nodeid %d,resid %d.\n", pnd->nindx, fixres->res.trs[i].resid);
                    break;
                }
            }
            active_res(fixres->res.trs[i].resid, pnd->nindx, fixres->res.trs[i].freq, fixres->res.trs[i].pos);
            flag = 1;
            for (j = 0; j < MAX_TRS_RES_CNT; j++)
            {
                if (fixres->res.trs[i].resid == pnd->pctx->trs_res_id[j])
                {
                    ptrs->chan = fixres->res.trs[i].chan;
                    ptrs->freq = fixres->res.trs[i].freq;
                    ptrs->pos = fixres->res.trs[i].pos;
                    ptrs->len = fixres->res.trs[i].len;
                    ltary[0] = 'c';
                    send_message(ltary, sizeof(hcb_msg_hdr) + sizeof(hcb_trs_fixres) + 4);
                    LOG_OUT(LOG_INFO, "cn add trs res,%d\n", fixres->res.trs[i].resid);
                    flag = 0;
                    bpskey = 1;
                    break;
                }
            }
            if (flag)
            {
                LOG_OUT(LOG_DBG, "not my trs res,config err,check it,nodeid %d,resid %d.\n", pnd->pctx->nodeid, fixres->res.trs[i].resid);
            }
        }
    }

    return 0;
}

int res_new_tn(uint8_t nindx, uint8_t *pmac, sysm_context_t *pctx)
{
    int iret;
    node_info_t *pnd;

    /**
     * @brief 节点掉线不删除，重新上线不分配
     *
     */
    iret = 0;
    if (pre_res.state == 1)
    {
        for (int i = 0; i < MAX_DYN_NODE; i++)
        {
            if(res_hold[i] == 0)
            {
                res_hold[i] = nindx;
            add_trs_res(i+2,pmac,1);
            LOG_OUT(LOG_INFO, "add_trs_res id %d\n", i+2);
            break;
            }
        }
    }//yjn
    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        if (0 == memcmp(pmac, pnd->mac, MAC_LEN))
        {
            iret = 1;
            break;
        }
    }
    if (iret == 0)
    {
        /**/
        pnd = (node_info_t *)malloc(sizeof(node_info_t));
        if (pnd == NULL)
        {
            return 1;
        }
        memset(pnd, 0, sizeof(node_info_t));
        pnd->state = CFG_NODE_INIT;
        list_add_tail(&(ncn), &(pnd->node));
    }
    else if (pnd->state == CFG_NODE_DEL_FINISH)
    {
        pnd->state = CFG_NODE_INIT;
    }

    pnd->role = 2;
    /* resource zero */
    pnd->nindx = nindx;
    pnd->pctx = pctx;
    extern uint64_t systime;
    pnd->onlinetime = systime;
    pnd->online = 1;
    memcpy(pnd->mac, pmac, 6);
    LOG_OUT(LOG_INFO, "new tn %d,%s.\n", pnd->nindx, pnd->name);
    return 0;
}
int res_del_res_rpc_cbk(void *parg, int ilen, void *ibuf);
int res_delete_node(node_info_t *pnd, sysm_context_t *pctx)
{
    int iret;
    int i;
    int j;
    intptr_t irpc;
    node_info_t *ptmp;
    hcb_fixres_t *fixres;
    uint8_t tary[256];
    uint8_t ltary[64];
    hcb_msg_hdr *phdr;
    hcb_rcv_fixres *prcv;

    fixres = (hcb_fixres_t *)&tary[8];

    /*获取需要删除的资源*/
    iret = get_del_res_config(pnd->mac, fixres);
    if (iret != 0)
    {
        // res_del_res_rpc_cbk(pnd, 0, NULL);
        pnd->state = CFG_NODE_DEL_FINISH;
        return 0;
    }
    fixres->snode = pnd->nindx;
    /**/
    phdr = (hcb_msg_hdr *)&ltary[1];
    phdr->type = HCB_MSG_DEL_RCV_FIXS;
    phdr->leng = sizeof(hcb_msg_hdr) + sizeof(hcb_rcv_fixres);
    phdr->flag = 0;
    phdr->info = 0;
    prcv = (hcb_rcv_fixres *)(phdr + 1);
    /*cn 本地删除接收资源*/
    for (i = 0; i < fixres->rescnt; i++)
    {
        resfrm_free(fixres->res.del[i].freq, fixres->res.del[i].pos, fixres->res.del[i].len);
        deactive_res(fixres->res.del[i].resid);
        /*广播检查*/
        if ((pnd->pctx->rcvall != 0) || (fixres->res.del[i].send2all != 0))
        {
            prcv->snode = pnd->nindx;
            prcv->freq = fixres->res.del[i].freq;
            prcv->pos = fixres->res.del[i].pos;
            prcv->len = fixres->res.del[i].len;
            ltary[0] = 'c';
            send_message(ltary, sizeof(hcb_msg_hdr) + sizeof(hcb_rcv_fixres) + 4);
            continue;
        }
        for (j = 0; j < MAX_RCV_RES_CNT; ++j)
        {
            if (fixres->res.del[i].resid == pctx->rcv_res_id[j])
            {
                /**/
                prcv->snode = pnd->nindx;
                prcv->freq = fixres->res.del[i].freq;
                prcv->pos = fixres->res.del[i].pos;
                prcv->len = fixres->res.del[i].len;
                /**/
                ltary[0] = 'c';
                send_message(ltary, sizeof(hcb_msg_hdr) + sizeof(hcb_rcv_fixres) + 4);
                LOG_OUT(LOG_INFO, "cn del rcv res %d.\n", pctx->rcv_res_id[j]);
            }
        }
    }
    /*广播报文，初始化应答标记*/
    init_ack_flag();
    list_for_every_entry(&(ncn), ptmp, node_info_t, node)
    {
        if ((ptmp->nindx > 0) && (ptmp->offline == 0) && (ptmp->state == CFG_NODE_ADD_FINISH))
        {
            LOG_OUT(LOG_INFO, "send del res to node %d.\n", ptmp->nindx);
            set_bit(ptmp->nindx);
        }
    }
    if (0 == is_ack_zero())
    {
        LOG_OUT(LOG_DBG, "no node to inform %d.\n", pnd->nindx);
        pnd->state = CFG_NODE_DEL_FINISH;
        return 0;
    }
    /**/
    iret = rpc_request_create(0xff, 0x15, &irpc);
    if (iret != 0)
    {
        LOG_OUT(LOG_ERR, "rpc create fail.\n");
        return 1;
    }

    rpc_request_set_callbk(irpc, res_del_res_rpc_cbk, pnd);
    tary[0] = 'r';
    memcpy(&tary[4], &irpc, 4);
    send_message(tary, sizeof(hcb_fixres_t) + 8);

    return 0;
}

int res_del_res_rpc_cbk(void *parg, int ilen, void *ibuf)
{
    uint8_t *pnid;
    node_info_t *pnd;
    node_info_t *ptmp;
    /**/
    pnd = (node_info_t *)parg;
    if (ibuf != NULL)
    {
        pnid = (uint8_t *)ibuf;
        LOG_OUT(LOG_INFO, "del node %d res, node  %d ack\n", pnd->nindx, pnid[0]);
        clear_bit(pnid[0]);
    }
    else
    {
        if (0 != is_ack_zero())
        {
            pnd->state = CFG_NODE_FAIL;
            LOG_OUT(LOG_INFO, "del node %d res timeout err.\n", pnd->nindx);
        }
    }

    if (0 == is_ack_zero())
    {
        pnd->state = CFG_NODE_DEL_FINISH;
        LOG_OUT(LOG_INFO, "del node %d res ok.\n", pnd->nindx);
        return 0;
    }
    else
    {
        return 1;
    }
}

int res_delete_tn(uint8_t nid, uint8_t *pmac, sysm_context_t *pctx)
{
    node_info_t *pnd;
    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        if (pnd->nindx == nid)
        {
            pnd->offline = 1;
            extern uint64_t systime;
            pnd->losttime = systime - pnd->onlinetime;
            pnd->lostcnt++;
            pnd->pctx = pctx;
            /*节点掉线，正在进行的广播报文可能收不到*/
            clear_bit(nid);
            LOG_OUT("node  %s,%d offline.\n", (const char *)pnd->name, pnd->nindx);
            for(int i = 0;i<MAX_DYN_NODE;i++)
		    {
		    	if(res_hold[i] == nid)
		    	{
		    		res_hold[i] = 0;
		    	}
		    }
            break;
        }
    }
    return 0;
}

int debug_cmd_nodes(void *parg, int argc, const char **argv)
{
    node_info_t *pnd;

    printf("\n");
    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        printf("nodeid = %d,", pnd->nindx);
        printf("\tmac =%02X%02X%02X%02X%02X%02X\trole=%d\tstate=%02d\t name=%s \n",
               pnd->mac[0], pnd->mac[1], pnd->mac[2], pnd->mac[3], pnd->mac[4], pnd->mac[5], pnd->role, pnd->state, pnd->name);
    }
    return 0;
}

int ui_update_nodecfg(uint8_t *pbuf)
{
    node_info_t *pnd;
    nodeusercfg_t *pcfg;
    uint32_t *ptr;
    ptr = (uint32_t *)pbuf;
    /**
     * @brief 如果不是节点配置，返回
     *
     */
    if (ptr[1] != E_CFG_NODE)
    {
        return 0;
    }
    pcfg = (nodeusercfg_t *)&pbuf[8];

    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        if (pnd->nindx == pbuf[0])
        {
            memcpy(pnd->name, pcfg->name, NAME_LEN);
            pnd->nodeaddr = pcfg->addr;
            pnd->listen = pcfg->listenautbus;
        }
    }
    return 0;
}

void set_cn_sigerr()
{
    node_info_t *pnd;
    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        if (pnd->nindx == 0)
        {
            pnd->sigerr = 1;
            break;
        }
    }
}
void set_cn_report()
{
    node_info_t *pnd;
    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        if (pnd->nindx == 0)
        {
            pnd->report = 1;
            break;
        }
    }
}

int ui_get_nodes(atb_nodeinfo_t *pnodes)
{
    extern uint64_t systime;
    node_info_t *pnd;
    nodeinfo_t *pinfo;
    uint64_t runtime;

    pinfo = pnodes->pnodecfg;
    pnodes->nodecnt = 0;

    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        pinfo->nid = pnd->nindx;
        pinfo->role = pnd->role;
        pinfo->state = pnd->state;
        memcpy(pinfo->name, pnd->name, NAME_LEN);
        memcpy(pinfo->mac, pnd->mac, MAC_LEN);
        memcpy(pinfo->aver, pnd->aver, VER_LEN);
        memcpy(pinfo->bver, pnd->bver, VER_LEN);
        memcpy(pinfo->nodetype, pnd->nodetype, VER_LEN);
        pinfo->nodeaddr = pnd->nodeaddr;
        pinfo->listen = pnd->listen;
        pinfo->sigerr = pnd->sigerr;
        pinfo->report = pnd->report;
        if (pnd->clearcnt < 3)
        {
            pnd->clearcnt++;
        }
        else
        {
            pnd->clearcnt = 0;
            pnd->sigerr = 0;
            pnd->report = 0;
        }

        if (pnd->state > CFG_NODE_ADD_FINISH)
        {
            memcpy(&pinfo->runtime, &pnd->losttime, sizeof(runtime));
        }
        else
        {
            runtime = systime - pnd->onlinetime;
            memcpy(&pinfo->runtime, &runtime, sizeof(runtime));
        }
        pinfo->lostcnt = pnd->lostcnt;

        pnodes->nodecnt++;
        pinfo++;
    }
    return pnodes->nodecnt;
}

/**
 * @brief 为了兼容性，提供最基本的获取节点基本信息的上位机接口
 *
 * @param pnodes
 * @return int
 */

int ui_get_names(basicnode_t *pnodes)
{
    extern uint64_t systime;
    node_info_t *pnd;
    basic_nodeinfo_t *pinfo;
    uint64_t runtime;

    pinfo = pnodes->pnodecfg;
    pnodes->nodecnt = 0;

    list_for_every_entry(&(ncn), pnd, node_info_t, node)
    {
        pinfo->nid = pnd->nindx;
        pinfo->role = pnd->role;
        pinfo->state = pnd->state;
        memcpy(pinfo->name, pnd->name, NAME_LEN);
        memcpy(pinfo->mac, pnd->mac, MAC_LEN);
        memcpy(pinfo->aver, pnd->aver, VER_LEN);
        memcpy(pinfo->bver, pnd->bver, VER_LEN);
        memcpy(pinfo->nodetype, pnd->nodetype, VER_LEN);
        
        if (pnd->state > CFG_NODE_ADD_FINISH)
        {
            memcpy(&pinfo->runtime, &pnd->losttime, sizeof(runtime));
        }
        else
        {
            runtime = systime - pnd->onlinetime;
            memcpy(&pinfo->runtime, &runtime, sizeof(runtime));
        }

        pnodes->nodecnt++;
        pinfo++;
    }
    return pnodes->nodecnt;
}

int rpc_update_srcid(int nodeid);
uint8_t tnnodeid;
void record_autbus_ctrl(uint32_t onoff);
void test_dump_sysm(void *parg, uint8_t *tbuf)
{
    sysm_context_t *pctx;
    hcb_msg_hdr *phdr;
    hcb_node_info *pinfo;
    atbcfg_t *pcfg;
    pcfg = get_atbcfg();
    /**/
    // debug_dump_hex( tbuf, 40 );

    /**/
    pctx = (sysm_context_t *)parg;
    phdr = (hcb_msg_hdr *)tbuf;
    printf("in sysm, rpt evt: %u\n", phdr->type);

    switch (phdr->type)
    {
    case HCB_MSG_RPT_ONLINE:
        pinfo = (hcb_node_info *)(phdr + 1);
        pctx->nodeid = pinfo->nodeid;
        tnnodeid= pinfo->nodeid;
        rpc_update_srcid(pctx->nodeid); //rpc更新nodeid
        memcpy(pctx->bver, (uint8_t *)(phdr + 1) + 1, VER_LEN); //
        memcpy(pctx->devtype, "TNSDK", 9);
        memcpy(pctx->name, devcfg.nodecfg.name, NAME_LEN);
        pctx->nodeaddr = devcfg.nodecfg.addr;
        if (pctx->nodeid == 0)
        {
            res_new_cn(0, pctx);
            if (pcfg->dynband == 1)
            {
                write_reg(0x701ff8, 0xaa55aa00);
                write_reg(0x701ffc, 0x0);
                LOG_OUT(LOG_INFO, "cn dynband CFG OK ,%d\n", pcfg->dynband);
            }
        }
        else
        {
            pctx->state = RES_NODE_ONLINE;
        }
        record_autbus_ctrl(devcfg.nodecfg.listenautbus);
        LOG_OUT(LOG_DBG, "listen autbus:0x%x\n", devcfg.nodecfg.listenautbus);
        break;

    case HCB_MSG_RPT_NWNODE:
        pinfo = (hcb_node_info *)(phdr + 1);
        res_new_tn(pinfo->nodeid, pinfo->mac, pctx);
		rpc_set_dyn(pinfo->nodeid, pcfg->dynband);
        break;

    case HCB_MSG_RPT_LSNODE:
        pinfo = (hcb_node_info *)(phdr + 1);
        res_delete_tn(pinfo->nodeid, pinfo->mac, pctx);
        break;

    case HCB_MSG_RPT_OFFLINE:
        if (pctx->nodeid != 0)
        {
            // reboot(0);
            soc_reboot();
        }
        break;
        
    default:
        break;
    }

    /**/
    return;
}

/**
 * @brief 节点上线后，第二条rpc远程调用报文，cn将新上线节点发送资源广播出来
 *       所有节点根据之前收到的资源列表决定是否处理，是接收，还是发送
 * @param parg
 * @param ilen
 * @param ibuf
 * @return int
 */

int sysres_srv_add_trs_res(void *parg, int ilen, void *ibuf)
{
    int i;
    int j;
    int flag;
    sysm_context_t *pctx;
    uint8_t tary[128];
    hcb_msg_hdr *phdr;
    hcb_fixres_t *pfixres;
    hcb_trs_fixres *ptrs;
    hcb_rcv_fixres *prcv;

    pctx = (sysm_context_t *)parg;
    /**
     * @brief 资源配置还没开始，不处理
     *
     */
    if (pctx->state != RES_NODE_CFG)
    {
        return 0;
    }

    /*回复nodeid*/
    tary[0] = pctx->nodeid;
    rpc_respond_send(1, tary);
    LOG_OUT(LOG_INFO, "recv trs res\n");

    /**/
    phdr = (hcb_msg_hdr *)tary;
    phdr->flag = 0;
    phdr->info = 0;

    pfixres = (hcb_fixres_t *)ibuf;
    /*是自己的发送资源,添加到发送资源*/
    if (pfixres->snode == pctx->nodeid)
    {
        phdr->type = HCB_MSG_ADD_TRS_FIXS;
        phdr->leng = sizeof(hcb_msg_hdr) + sizeof(hcb_trs_fixres);
        ptrs = (hcb_trs_fixres *)(phdr + 1);
        for (i = 0; i < pfixres->rescnt; i++)
        {
            flag = 1;
            for (j = 0; j < MAX_TRS_RES_CNT; j++)
            {
                if (pfixres->res.trs[i].resid == pctx->trs_res_id[j])
                {
                    ptrs->chan = pfixres->res.trs[i].chan;
                    ptrs->freq = pfixres->res.trs[i].freq;
                    ptrs->pos = pfixres->res.trs[i].pos;
                    ptrs->len = pfixres->res.trs[i].len;
                    sysmgr_send((uint8_t *)phdr, NULL, NULL, 0);
                    LOG_OUT(LOG_INFO, "add trs res,%d\n", pfixres->res.trs[i].resid);
                    flag = 0;
                    bpskey = 1;
                    break;
                }
            }
            if (flag)
            {
                LOG_OUT(LOG_DBG, "not my trs res,config err,check it,nodeid %d,resid %d.\n", pctx->nodeid, pfixres->res.trs[i].resid);
            }
        }
    } /*不是自己的发送资源，检查是否需要接收该资源*/
    else
    {
        phdr->type = HCB_MSG_ADD_RCV_FIXS;
        phdr->leng = sizeof(hcb_msg_hdr) + sizeof(hcb_rcv_fixres);
        prcv = (hcb_rcv_fixres *)(phdr + 1);
        for (i = 0; i < pfixres->rescnt; i++)
        {
            /*判断是否支持广播*/
            if ((pctx->rcvall != 0) || (pfixres->res.trs[i].send2all != 0))
            {
                prcv->snode = pfixres->snode;
                prcv->pos = pfixres->res.trs[i].pos;
                prcv->freq = pfixres->res.trs[i].freq;
                prcv->len = pfixres->res.trs[i].len;
                sysmgr_send((uint8_t *)phdr, NULL, NULL, 0);
                LOG_OUT(LOG_INFO, "add rcv res,%d\n", pfixres->res.trs[i].resid);
                continue;
            }

            for (j = 0; j < MAX_RCV_RES_CNT; j++)
            {
                if (pfixres->res.trs[i].resid == pctx->rcv_res_id[j])
                {
                    prcv->snode = pfixres->snode;
                    prcv->pos = pfixres->res.trs[i].pos;
                    prcv->freq = pfixres->res.trs[i].freq;
                    prcv->len = pfixres->res.trs[i].len;
                    sysmgr_send((uint8_t *)phdr, NULL, NULL, 0);
                    LOG_OUT(LOG_INFO, "add rcv res,%d\n", pfixres->res.trs[i].resid);
                    break;
                }
            }
        }
    }

    return 0;
}

int sysres_srv_del_res(void *parg, int ilen, void *ibuf)
{

    int i;
    int j;
    int flag;
    sysm_context_t *pctx;
    uint8_t tary[128];
    hcb_msg_hdr *phdr;
    hcb_fixres_t *pfixres;
    hcb_rcv_fixres *prcv;
    hcb_trs_fixres *ptrs;

    /*回复nodeid*/
    pctx = (sysm_context_t *)parg;
    /**
     * @brief 资源配置还没开始，不处理
     *
     */
    if (pctx->state != RES_NODE_CFG)
    {
        return 0;
    }

    tary[0] = pctx->nodeid;
    rpc_respond_send(1, tary);
    LOG_OUT(LOG_INFO, "recv del res.\n");

    /**/
    phdr = (hcb_msg_hdr *)tary;
    phdr->flag = 0;
    phdr->info = 0;

    pfixres = (hcb_fixres_t *)ibuf;
    /*是自己的发送资源,删除该资源*/
    if (pfixres->snode == pctx->nodeid)
    {
        phdr->type = HCB_MSG_DEL_TRS_FIXS;
        phdr->leng = sizeof(hcb_msg_hdr) + sizeof(hcb_trs_fixres);
        ptrs = (hcb_trs_fixres *)(phdr + 1);
        for (i = 0; i < pfixres->rescnt; i++)
        {
            flag = 1;
            for (j = 0; j < MAX_TRS_RES_CNT; j++)
            {
                if (pfixres->res.del[i].resid == pctx->trs_res_id[j])
                {
                    ptrs->freq = pfixres->res.del[i].freq;
                    ptrs->pos = pfixres->res.del[i].pos;
                    ptrs->len = pfixres->res.del[i].len;
                    sysmgr_send((uint8_t *)phdr, NULL, NULL, 0);
                    LOG_OUT(LOG_INFO, "del trs res,%d\n", pfixres->res.del[i].resid);
                    flag = 0;
                    break;
                }
            }
            if (flag)
            {
                LOG_OUT(LOG_DBG, "not my trs res,config err,check it,nodeid %d,resid %d.\n", pctx->nodeid, pfixres->res.del[i].resid);
            }
        }
    } /*不是自己的发送资源，检查是否需要删除该接收资源*/
    else
    {
        phdr->type = HCB_MSG_DEL_RCV_FIXS;
        phdr->leng = sizeof(hcb_msg_hdr) + sizeof(hcb_rcv_fixres);
        prcv = (hcb_rcv_fixres *)(phdr + 1);
        for (i = 0; i < pfixres->rescnt; i++)
        {
            /*广播判断*/
            if ((pctx->rcvall != 0) || (pfixres->res.del[i].send2all != 0))
            {
                prcv->pos = pfixres->res.del[i].pos;
                prcv->freq = pfixres->res.del[i].freq;
                prcv->len = pfixres->res.del[i].len;
                sysmgr_send((uint8_t *)phdr, NULL, NULL, 0);
                LOG_OUT(LOG_INFO, "del rcv res,%d\n", pfixres->res.del[i].resid);
                continue;
            }
            for (j = 0; j < MAX_RCV_RES_CNT; j++)
            {
                if (pfixres->res.del[i].resid == pctx->rcv_res_id[j])
                {
                    prcv->pos = pfixres->res.del[i].pos;
                    prcv->freq = pfixres->res.del[i].freq;
                    prcv->len = pfixres->res.del[i].len;
                    sysmgr_send((uint8_t *)phdr, NULL, NULL, 0);
                    LOG_OUT(LOG_INFO, "del rcv res,%d\n", pctx->rcv_res_id[j]);
                    break;
                }
            }
        }
    }

    return 0;
}

int get_debug_data(uint8_t *ptr)
{
    return 0;
}

/**
 * @brief ui界面实时显示调试数据,uint32_t 类型，第一个为总数目
 *        后面每个uint32_t为一个值
 * @param parg
 * @param ilen
 * @param ibuf
 * @return int
 */
int sysres_srv_debug_get(void *parg, int ilen, void *ibuf)
{
    int cnt;
    uint8_t *ptr;
    ptr = (uint8_t *)atbui_cmd.ui_dat_buf;
    cnt = get_debug_data(ptr);
    memcpy(ptr, &cnt, 4);
    // debug_dump_hex(&cnt, 4);
    if ((cnt > 0) && (parg != NULL))
    {
        rpc_respond_send(UI_BUF_SIZE, ptr);
    }
    return 0;
}
/**
 * @brief tn配置时，rpc进行数据传输
 *
 * @param parg
 * @param ilen
 * @param ibuf
 * @return int
 */
uint8_t get_srcnid();
int sysres_srv_ui_data(void *parg, int ilen, void *ibuf)
{
    int iret;
    pbuf_t *ipkt;
    sysm_context_t *pctx;
    uint8_t *ptr;
    uint8_t tary[4];
    pctx = (sysm_context_t *)parg;
    if (NULL == ibuf)
    {
        LOG_OUT(LOG_DBG, "ibuf null ptr.\n");
        return -1;
    }
    if (ilen > 1300)
    {
        LOG_OUT(LOG_DBG, "data len error.\n");
        return -2;
    }

    /**
     * @brief cn收到此数据，是来自tn的，解析报文
     * tn收到此数据，是来自cn的,转到到eth
     *
     */

    atbui_cmd.uiforward_nodeid = get_srcnid();
    rpc_respond_send(4, tary);
    if (0 == pctx->nodeid)
    {
        atb_ui_rpc_recv(ibuf, ilen, pctx->ffd);
    }
    else
    {

        ipkt = pkt_alloc(0);
        if (NULL != ipkt)
        {
            ptr = pkt_append(ipkt, ilen);
            memcpy(ptr, ibuf, ilen);
            iret = send(pctx->efd, ipkt);
            if (iret != 1)
            {
                pkt_free(ipkt);
                return 2;
            }
            // LOG_OUT(LOG_DBG, "eth eth reply.\n");
        }
    }
    return 0;
}
/**
 * @brief tn请求，cn接收，目前为信号质量告警
 * @param parg
 * @param ilen
 * @param ibuf
 * @return int
 */
int sysres_srv_tn_info(void *parg, int ilen, void *ibuf)
{
    int iret;
    pbuf_t *ipkt;
    node_info_t *pnd;
    sysm_context_t *pctx;
    tnreport_t *preport;
    uint8_t tary[4];
    pctx = (sysm_context_t *)parg;
    if (NULL == ibuf)
    {
        LOG_OUT(LOG_DBG, "ibuf null ptr.\n");
        return -1;
    }
    if (ilen > 1300)
    {
        LOG_OUT(LOG_DBG, "data len error.\n");
        return -2;
    }

    /**
     * @brief cn收到此数据，是来自tn的，解析报文
     *        tn暂时不会收到此报文
     *
     */
    atbui_cmd.uiforward_nodeid = get_srcnid();
    rpc_respond_send(4, tary);
    preport = (tnreport_t **)ibuf;
    if (0 == pctx->nodeid)
    {
        list_for_every_entry(&(ncn), pnd, node_info_t, node)
        {
            if (pnd->nindx == preport->nodeid)
            {
                pnd->sigerr = preport->sigerr;
                pnd->report = preport->report;
            }
        }
    }

    return 0;
}

bps_t bpsdata;
int sysres_srv_bps_get(void *parg, int ilen, void *ibuf)
{
    int cnt;
    uint8_t *ptr;
    sysm_context_t *pctx;
    pctx = (sysm_context_t *)parg;

    ptr = (uint8_t *)atbui_cmd.ui_dat_buf;
    bpsdata.nodeid = pctx->nodeid;
    cnt = 1;
    memcpy(ptr, &cnt, 4);
    memcpy(&ptr[4], &bpsdata, sizeof(bps_t));
    // debug_dump_hex(ptr,sizeof(bpsdata)+4);
    if ((cnt > 0) && (parg != NULL))
    {
        rpc_respond_send(UI_BUF_SIZE, ptr);
    }
    return 0;
}
int sysres_srv_dyn_set(void *parg, int ilen, void *ibuf)
{
    atbcfg_t * pcfg;
    sysm_context_t *pctx;
    pctx = (sysm_context_t *)parg;
    uint8_t tary[16] = {0};
    int iret;
    pcfg = get_atbcfg();
    pcfg->dynband = *(uint8_t *)ibuf;
    if (pcfg->dynband == 1)
    {
        write_reg(0x701ff8, 0xaa55aa00);
        write_reg(0x701ffc, 0x0);
        LOG_OUT(LOG_INFO, "tn dynband CFG OK ,%d\n", pcfg->dynband);
    }
    iret = rpc_respond_send(16, tary);
    return 0;
}
int sysres_srv_device_get(void *parg, int ilen, void *ibuf)
{
    int cnt;
    uint8_t *ptr;
    uint32_t portid;
    uint8_t tary[64];
    sysm_context_t *pctx;
    pctx = (sysm_context_t *)parg;

    if (ibuf != NULL)
    {
        portid = *(uint32_t *)ibuf;
    }
    else
    {
        portid = *(uint32_t *)atbui_cmd.dev_set_buf;
    }

    ptr = (uint8_t *)atbui_cmd.dev_set_buf;
    memcpy(ptr, &pctx->nodeid, 1);
    memcpy(ptr + 4, &portid, 4);
    ptr = ptr + 8;
    // LOG_OUT(LOG_DBG,"get device:%d,port:%d\n",atbui_cmd.devget_nodeid,portid);
    switch (portid)
    {
    case E_CFG_RS4850:
        memcpy(ptr, &devcfg.rs485cfg[0], sizeof(rs485cfg_t));
        break;
    case E_CFG_RS4851:
        memcpy(ptr, &devcfg.rs485cfg[1], sizeof(rs485cfg_t));
        break;
    case E_CFG_RS4852:
        memcpy(ptr, &devcfg.rs485cfg[2], sizeof(rs485cfg_t));
        break;
    case E_CFG_CAN0:
        memcpy(ptr, &devcfg.cancfg[0], sizeof(cancfg_t));
        break;
    case E_CFG_CAN1:
        memcpy(ptr, &devcfg.cancfg[1], sizeof(cancfg_t));
        break;
    case E_CFG_CAN2:
        memcpy(ptr, &devcfg.cancfg[2], sizeof(cancfg_t));
        break;
    case E_CFG_IOM:
        memcpy(ptr, &devcfg.uiuocfg[0], sizeof(uiuocfg_t) * 12);
        break;
    case E_CFG_NODE:
        memcpy(ptr, &devcfg.nodecfg, sizeof(nodeusercfg_t));
        break;
    default:
        break;
    }
    if (ibuf != NULL)
    {
        rpc_respond_send(DEV_SET_SIZE, ptr - 8);
    }

    return 0;
}
int set_debug_data(uint8_t *ptr)
{
    return 0;
}
/**
 * @brief  rpc调用，ui界面设置参数，每4个字节为一个数，
 *         最前面4个字节是参数个数，暂时没什么用，后面每4个字节为一个参数
 *         参数具体意义，参考用户界面
 *
 * @param parg
 * @param ilen
 * @param ibuf
 * @return int
 */
int sysres_srv_debug_set(void *parg, int ilen, void *ibuf)
{
    int cnt;
    int i;
    uint8_t *ptr;
    sysm_context_t *pctx;
    pctx = (sysm_context_t *)parg;

    if ((ilen > 0) && (ibuf != NULL))
    {
        ptr = (uint8_t *)ibuf;
        memcpy(&cnt, ptr, 4);
        set_debug_data(ptr);
        rpc_respond_send(8, ptr);
    }
    else
    {
        ptr = atbui_cmd.ui_dat_buf;
        memcpy(&cnt, ptr, 4);
        set_debug_data(ptr);
    }
    memcpy(ptr, "cfg", 3);
    write(pctx->msguifd, ptr, 128);

    return 0;
}
int sysres_srv_device_set(void *parg, int ilen, void *ibuf)
{
    int i;
    uint32_t *pu32;
    uint8_t tbuf[32], *pu8;
    sysm_context_t *pctx;
    pctx = (sysm_context_t *)parg;
    if (ibuf != NULL)
    {
        rpc_respond_send(ilen, tbuf);
        memcpy(atbui_cmd.dev_set_buf, ibuf + 4, DEV_SET_SIZE);
    }
    else
    {
        memcpy(atbui_cmd.dev_set_buf, atbui_cmd.dev_set_buf + 4, DEV_SET_SIZE);
    }
    pu32 = (uint32_t *)atbui_cmd.dev_set_buf;
    pu8 = (uint8_t *)atbui_cmd.dev_set_buf;
    if (pu32[0] >= E_CFG_REBOOT)
    {
        /**
         * @brief 控制消息广播，nid在消息里面，如果匹配成功则处理否则，不处理。
         *
         */
        for (i = 0; i < pu8[4]; i++)
        {
            if (pctx->nodeid == pu8[i + 5])
            {
                memcpy(tbuf, "svd", 3);
                write(pctx->msguifd, tbuf, 16);
                break;
            }
            // LOG_OUT(LOG_DBG,"nid:%d--%d\n",pctx->nodeid,pu8[i+5]);
        }
    }
    else
    {
        memcpy(tbuf, "svd", 3);
        write(pctx->msguifd, tbuf, 16);
    }

    return 0;
}
int sysres_srv_ota_start(void *parg, int ilen, void *ibuf)
{
    int cnt;
    int i;
    uint8_t *ptr;
    uint8_t tary[16];
    sysm_context_t *pctx;
    pctx = (sysm_context_t *)parg;

    if ((ilen > 0) && (ibuf != NULL))
    {
        ptr = (uint8_t *)ibuf;
        memcpy(&cnt, ptr, 4);
        rpc_respond_send(8, ptr);
    }
    else
    {
        ptr = atbui_cmd.ui_dat_buf;
        memcpy(&cnt, ptr, 4);
    }
    ptr += 4;
    atbui_cmd.ota_data_sn = 1;
    for (i = 0; i < cnt; i++)
    {
        if (0 == memcmp(ptr, pctx->mac, MAC_LEN))
        {
            atbui_cmd.ota_flag = 1;
            break;
        }
        ptr += MAC_LEN;
    }
    memcpy(tary, "otastart", 8);
    write(pctx->msguifd, tary, 32);
    LOG_OUT(LOG_DBG, "ota start %d.\n", atbui_cmd.ota_flag);
    return 0;
}

int sysres_srv_ota_data(void *parg, int ilen, void *ibuf)
{
    uint16_t cnt;
    uint8_t *ptr;
    uint8_t tary[32];
    sysm_context_t *pctx;
    pctx = (sysm_context_t *)parg;
    if (atbui_cmd.ota_flag == 0)
    {
        return 0;
    }

    if ((ilen > 0) && (ibuf != NULL))
    {
        ptr = (uint8_t *)ibuf;
        memcpy(atbui_cmd.ui_dat_buf, ibuf, ilen);
        rpc_respond_send(8, ptr);
    }
    else
    {
        ptr = atbui_cmd.ui_dat_buf;
    }
    memcpy(&cnt, ptr, 2);
    if (cnt != atbui_cmd.ota_data_sn)
    {
        atbui_cmd.ota_flag = 0;
        atbui_cmd.ota_data_sn = 0;
        LOG_OUT(LOG_DBG, "data serial number error %d,abort.\n", atbui_cmd.ota_data_sn);
        return 0;
    }
    atbui_cmd.ota_data_sn++;
    /**
     * @brief 发消息，通知ui写入flash
     *
     */
    // LOG_OUT(LOG_DBG,"ota data sn %d.\n",atbui_cmd.ota_data_sn);
    memcpy(tary, "otadata", 7);
    write(pctx->msguifd, tary, 32);
    return 0;
}

int sysres_srv_ota_end(void *parg, int ilen, void *ibuf)
{
    uint8_t *ptr;
    sysm_context_t *pctx;
    uint8_t tary[32];

    pctx = (sysm_context_t *)parg;
    if (atbui_cmd.ota_flag == 0)
    {
        return 0;
    }
    if ((ilen > 0) && (ibuf != NULL))
    {
        ptr = (uint8_t *)ibuf;
        rpc_respond_send(ilen, ptr);
    }
    else
    {
        ptr = atbui_cmd.ui_dat_buf;
    }
    LOG_OUT(LOG_DBG, "ota end,data sn %d.\n", atbui_cmd.ota_data_sn);
    atbui_cmd.reboot_delay = 15;
    LOG_OUT(LOG_DBG, "ota finish,reboot...\n");
    return 0;
}

/**
 * @brief 节点上线，第三条报文应答，处理接收资源
 *
 * @param parg
 * @param ilen
 * @param ibuf
 * @return int
 */

int sysres_srv_add_rcv_res(void *parg, int ilen, void *ibuf)
{
    int i;
    sysm_context_t *pctx;
    uint8_t tary[128];
    hcb_msg_hdr *phdr;
    hcb_fixres_t *pfixres;
    hcb_rcv_fixres *prcv;
    /**/
    /*回复nodeid*/
    pctx = (sysm_context_t *)parg;
    /**
     * @brief 资源配置还没开始，不处理
     *
     */
    if (pctx->state != RES_NODE_CFG)
    {
        return 0;
    }

    tary[0] = pctx->nodeid;
    rpc_respond_send(1, tary);

    /**/
    phdr = (hcb_msg_hdr *)tary;
    phdr->flag = 0;
    phdr->info = 0;

    pfixres = (hcb_fixres_t *)ibuf;
    phdr->type = HCB_MSG_ADD_RCV_FIXS;
    phdr->leng = sizeof(hcb_msg_hdr) + sizeof(hcb_rcv_fixres);
    prcv = (hcb_rcv_fixres *)(phdr + 1);
    LOG_OUT(LOG_INFO, "recv rcv res cnt:%d.\n", pfixres->rescnt);
    for (i = 0; i < pfixres->rescnt; i++)
    {
        prcv->snode = pfixres->res.rcv[i].snode;
        prcv->pos = pfixres->res.rcv[i].pos;
        prcv->freq = pfixres->res.rcv[i].freq;
        prcv->len = pfixres->res.rcv[i].len;
        sysmgr_send((uint8_t *)phdr, NULL, NULL, 0);
        LOG_OUT(LOG_INFO, "add rcv res,%d\n", pfixres->res.rcv[i].resid);
    }
    return 0;
}
/**
 * @brief 节点上线后，第一条rpc远程调用报文，tn保存cn发过来的 资源id列表，包括接收和发送的
 *        后续tn根据此接收的资源id列表，判断后续资源处理方式。
 * @param parg
 * @param ilen
 * @param ibuf
 * @return int
 */

int sysres_srv_add_res_id(void *parg, int ilen, void *ibuf)
{
    int len;
    uint8_t *ptr;
    uint8_t tary[128];
    sysm_context_t *pctx;
    node_config_t *pncfg;
    uint8_t mac[MAC_LEN];
    pctx = (sysm_context_t *)parg;
    pncfg = (node_config_t *)ibuf;

    if (0 != memcmp(pctx->mac, pncfg->mac, MAC_LEN))
    {
        memset(mac, 0x66, MAC_LEN);
        if (0 != memcmp(mac, pncfg->mac, MAC_LEN))
        {
            LOG_OUT(LOG_ERR, "mac not mactch,error res id config.\n");
            return -1;
        }
        pctx->rcvall = 0;
        memset(pctx->rcv_res_id, 0, sizeof(uint8_t) * MAX_RCV_RES_CNT);
        memset(pctx->trs_res_id, 0, sizeof(uint8_t) * MAX_TRS_RES_CNT);
    }
    else
    {
        pctx->rcvall = pncfg->rcvall;
        if (devcfg.nodecfg.listenautbus == 0x77)
        {
            pctx->rcvall = 1;
        }
        else
        {
            pctx->rcvall = 0;
        }
        memcpy(pctx->rcv_res_id, pncfg->rcv_res_id, MAX_RCV_RES_CNT);
        memcpy(pctx->trs_res_id, pncfg->trs_res_id, MAX_TRS_RES_CNT);
    }
    memset(tary, 0, 128);
    pctx->state = RES_NODE_CFG;
    tary[0] = pctx->nodeid;
    tary[1] = pctx->nodeaddr;
    ptr = get_build_time();
    memcpy(&tary[2], ptr, VER_LEN);
    memcpy(&tary[18], pctx->bver, VER_LEN);
    memcpy(&tary[34], pctx->devtype, VER_LEN);
    memcpy(&tary[50], pctx->name, NAME_LEN);
    tary[82] = devcfg.nodecfg.listenautbus;
    rpc_respond_send(84, tary);
    LOG_OUT(LOG_INFO, "name:%s,aver:%s--bver:%s.\n", &tary[50], &tary[2], &tary[18]);
    LOG_OUT(LOG_INFO, "node %d recv res id.\n", pctx->nodeid);

    return 0;
}

/**
 * @brief  点灯控制，寻找节点用
 *
 * @param parg
 * @param ilen
 * @param ibuf
 * @return int
 */

int sysres_srv_led()
{
#define ETH_RST_IO_NUM 14
#define ETH_RST_IO_BIT (1 << (ETH_RST_IO_NUM & 0x7U))
#define ETH_BASE_RST_IO (BASE_AP_GPIO0 + 0x8000 * (ETH_RST_IO_NUM >> 3))

    static uint8_t onoff;
    uintptr_t pbase;
    uint32_t temp;

    pbase = ETH_BASE_RST_IO;
    write_reg(pbase + GPIOC_MODE, 0);
    temp = read_reg(pbase + GPIOC_MODE);
    write_reg(pbase + GPIOC_MODE, temp | ETH_RST_IO_BIT);
    temp = read_reg(pbase + GPIOC_DQ);
    write_reg(pbase + GPIOC_DQ, temp & (~ETH_RST_IO_BIT));
    temp = read_reg(pbase + GPIOC_DQE);
    write_reg(pbase + GPIOC_DQE, temp | ETH_RST_IO_BIT);

    onoff ^= 1;
    /**
     * @brief 低电平点亮
     *
     */
    if (onoff != 0)
    {
        write_reg(pbase + GPIOC_DQ, temp & (~ETH_RST_IO_BIT));
    }
    else
    {
        write_reg(pbase + GPIOC_DQ, temp | ETH_RST_IO_BIT);
    }

    return 0;
}

static int8_t agcval[64] = {-11, -10, -9, -8, -8, -6, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 4, 6, 6,
                            7, 9, 10, 11, 2, 1, 1, 1, 1, 1, 1, 1, 1, 20, 21, 22, 23, 24, 25, 26,
                            27, 28, 29, 30, 31, 31, 33, 34, 34, 36, 37, 38, 39, 40, 40, 41, 42,
                            44, 44, 45, 48};
/**
 * @brief 收集当前节点的两线通信质量信息
 *
 * @param parg
 * @param ilen
 * @param ibuf
 * @return int
 */
int sysres_srv_bcpu_rtdata(void *parg, int ilen, void *ibuf)
{
    int i;
    int cnt;
    uint8_t *ptr;
    uintptr_t pbase;
    atbsignal_t *psig;
    uint32_t temp0;
    uint32_t temp1;
    uint32_t temp2;
    uint32_t temp3;

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
    int sigerr;

    ptr = atbui_cmd.ui_dat_buf;

    memset(ptr, 0, UI_BUF_SIZE);
    psig = (atbsignal_t *)(ptr + 4);

    pbase = BASE_BB_HCBMAC;
    cnt = 0;
    sigerr = 0;
    for (i = 0; i < 30; i++)
    {
        temp0 = read_reg(pbase + HCB_TAB_CBN_0(i));
        temp1 = read_reg(pbase + HCB_TAB_CBN_1(i));
        temp2 = read_reg(pbase + HCB_TAB_CBN_2(i));
        temp3 = read_reg(pbase + HCB_TAB_CBN_3(i));
        if ((temp0 == 0) && (temp1 == 0))
        {
            continue;
        }
        /*autbus 两线错包统计*/
        cb_total = temp1 & 0xFFF;
        cb_total = (cb_total << 32) | temp0;
        cberr_dn = temp2 & 0x3fffff;
        cberr_dn = (cberr_dn << 20) | (temp1 >> 12);
        cberr_up = temp3;
        cberr_up = (cberr_up << 10) | (temp2 >> 22);

        temp0 = read_reg(pbase + HCB_TAB_SNR_X(i));
        amdn = temp0 & 0xffff;
        amup = temp0 >> 16;
        snrdn = (amdn >> 10);
        amdn = (amdn & 0x3FF);
        snrup = (amup >> 10);
        amup = (amup & 0x3FF);

        temp0 = read_reg(BASE_BB_HCBMAC + HCB_TAB_NHD_X(i));
        /*agc val*/
        temp1 = (temp0 >> 24) & 0xff;
        if (temp1 < 0 && temp1 > 63)
        {
            return 0;
        }
        agc = agcval[temp1];
        nhdvup = (temp0 >> 21) & 0x1;
        nhdup = (temp0 >> 11) & 0x3ff;
        nhdvdn = (temp0 >> 10) & 0x1;
        nhddn = temp0 & 0x3ff;

        memcpy(&psig->cb_total, &cb_total, sizeof(cb_total));
        memcpy(&psig->cberr_dn, &cberr_dn, sizeof(cberr_dn));
        memcpy(&psig->cberr_up, &cberr_up, sizeof(cberr_up));

        memcpy(&psig->amdn, &amdn, sizeof(amdn));
        memcpy(&psig->amup, &amup, sizeof(amup));
        memcpy(&psig->nhddn, &nhddn, sizeof(nhddn));
        memcpy(&psig->nhdup, &nhdup, sizeof(nhdup));

        psig->nhdvdn = nhdvdn;
        psig->nhdvup = nhdvup;
        psig->snrdn = snrdn;
        psig->snrup = snrup;
        psig->agc = agc;
        psig->nodeid = i;
        psig++;
        cnt++;
        if (cnt > 30)
        {
            break;
        }
    }

    *(uint32_t *)ptr = cnt;
    // LOG_OUT(LOG_DBG, "tn atb sig:%d\n",cnt);
    if ((cnt > 0) && (parg != NULL))
    {
        rpc_respond_send(UI_BUF_SIZE, ptr);
    }
    // sysres_srv_led();
    return sigerr;
}

int sysres_init(void)
{
    static uint8_t init = 0;
    if (init != 0)
    {
        return 0;
    }
    init = 1;
    resfrm_init();//CN资源占用前4个symbol
    list_initialize(&(ncn));
    return 0;
}

typedef struct _tag_resm_context
{
    comn_context cmctx;
    /**/
    int msgfd; /* /dprrpc  */

} resm_context_t;

int entry_resmgr(void *parg)
{
    int qfd;
    int tfd;
    int mfd;
    int dfd;
    int state;
    int pushflg;
    resm_context_t ctx;
    node_info_t *pnd;

    memset((void *)&ctx, 0, sizeof(resm_context_t));
    tls_set(&ctx);

    /* pts */
    tfd = open("/pts", 0);
    ctx.cmctx.fd_stdio = tfd;
    hcb_debug_init();
    debug_add_cmd("nodes", debug_cmd_nodes, NULL);

    /* dpram */
    msgfd = open("/mesg/resmsg", 0);
    LOG_OUT(LOG_INFO, "open resmsg, %d\n", msgfd);
    /**/
    sysres_init();

    /**
     * @brief CN肯定最先上线，处理完成才处理TN
     *        CN是不可能下线的，所以不会处理CN下线
     *
     */
    state = CFG_IDLE;
    while (1)
    {
        /**
         * @brief 注意！！！！！！
         * 此线程只能对该链表进行遍历操作，不能插入和删除
         *
         */
        list_for_every_entry(&(ncn), pnd, node_info_t, node)
        {
            if (pnd->role != 0)
            {
                continue;
            }
            if (pnd->online != 0)
            {
                pnd->online = 0;
                pnd->state = CFG_NODE_ADD_FINISH;
                state = CFG_NODE_ONLINE;
                LOG_OUT(LOG_DBG, "deal cn online:%s,%d\n", pnd->name, pnd->nindx);
                res_add_cn(pnd);
                break;
            }
            LOG_OUT(LOG_DBG, "deal cn online:%s,%d\n", pnd->name, pnd->nindx);
        }
        if (state != CFG_IDLE)
        {
            break;
        }
    }

    state = CFG_IDLE;
    pushflg = 0;
    while (1)
    {
        hcb_debug_pts(tfd);
        switch (state)
        {
        case CFG_IDLE:
            list_for_every_entry(&(ncn), pnd, node_info_t, node)
            {
                if (pnd->role == 0)
                {
                    continue;
                }
                if ((pnd->state == CFG_NODE_INIT) || (pnd->state == CFG_NODE_DEL_FINISH))
                {
                    if (pnd->online != 0)
                    {
                        pnd->online = 0;
                        state = CFG_NODE_ONLINE;
                        pnd->state = CFG_NODE_DOWNLOAD_RES_ID;
                        LOG_OUT(LOG_DBG, "deal node online:%s,%d\n", pnd->name, pnd->nindx);
                        break;
                    }
                }
                else if (pnd->offline != 0)
                {
                    pnd->offline = 0;
                    state = CFG_NODE_OFFLINE;
                    pnd->state = CFG_NODE_DEL_RES;
                    LOG_OUT(LOG_DBG, "deal node offline:%s,%d\n", pnd->name, pnd->nindx);
                    break;
                }
            }
            if ((state == CFG_IDLE) && (pushflg != 0))
            {
                pushflg = 0;
                res_push_commit();
                LOG_OUT(LOG_INFO, "res push commit.\n");
            }
            break;
        case CFG_NODE_ONLINE:
            switch (pnd->state)
            {
            case CFG_NODE_DOWNLOAD_RES_ID:
                pnd->state = CFG_NODE_WAIT_ACK;
                res_download_res_id(pnd);
                break;
            case CFG_NODE_ADD_RCV_RES:
                pnd->state = CFG_NODE_WAIT_ACK;
                res_add_rcv_res(pnd);
                break;
            case CFG_NODE_ADD_TRS_RES:
                pnd->state = CFG_NODE_WAIT_ACK;
                res_add_trs_res(pnd);
                break;
            case CFG_NODE_ADD_FINISH:
                pushflg = 1;
                state = CFG_IDLE;
                break;
            case CFG_NODE_FAIL:
                state = CFG_IDLE;
                break;
            default:
                break;
            }
            break;
        case CFG_NODE_OFFLINE:
            switch (pnd->state)
            {
            case CFG_NODE_DEL_RES:
                pnd->state = CFG_NODE_WAIT_ACK;
                res_delete_node(pnd, pnd->pctx);
                break;
            case CFG_NODE_DEL_FINISH:
                pushflg = 1;
                state = CFG_IDLE;
                break;
            case CFG_NODE_FAIL:
                state = CFG_IDLE;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }
}
