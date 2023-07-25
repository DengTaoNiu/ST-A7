
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

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

typedef struct _tag_rpc_req
{
    struct list_node node;
    /**/
    uint8_t dstnd;
    uint32_t srvid;
    uint32_t sessn;
    /**/
    rpc_cbk_f cbkf;
    void *parg;

    /**/
    int retry;
    int tmout;

    /* msg body, for retry. */
    int tlen;
    void *pmsg;
    int tid; /* timer id */

} rpc_req_t;

typedef struct _tag_rpc_srv
{
    struct list_node node;
    /**/
    uint32_t srvid;
    rpc_srv_f cbkf;
    void *parg;

} rpc_srv_t;

/**/
struct list_node req_list;
struct list_node srv_list;
uint32_t req_sessn = 0;
int rpc_fd = -1;
static uint8_t srcnid;

/* dummy function */
static int rpc_dummy_cbk(void *parg, int ilen, void *ibuf)
{
    return 0;
}

/*
dnode, srvid : 标示一个远程服务， 在运行时还有一个动态 sn 序号。
*/
int rpc_request_create(uint8_t dnode, uint32_t srvid, intptr_t *pret)
{
    rpc_req_t *preq;

    /**/
    preq = (rpc_req_t *)malloc(sizeof(rpc_req_t));

    /**/
    preq->dstnd = dnode;
    preq->srvid = srvid;
    preq->sessn = req_sessn++;

    preq->cbkf = rpc_dummy_cbk;
    preq->parg = NULL;

    preq->retry = 3;
    preq->tmout = 1000000;

    preq->tlen = 0;
    preq->pmsg = NULL;

    /**/
    *pret = (intptr_t)preq;
    return 0;
}

int rpc_request_set_callbk(intptr_t req, rpc_cbk_f cbkf, void *parg)
{
    rpc_req_t *preq;

    /**/
    preq = (rpc_req_t *)req;

    /**/
    preq->cbkf = cbkf;
    preq->parg = parg;
    return 0;
}

/*
retry : 每次超时没有收到 resp 会进行重新发送， 重试次数。
 tcnt : 每次的超时时间， 单位 us 。
*/
int rpc_request_set_retry(intptr_t req, int retry, int tmout)
{
    rpc_req_t *preq;
 
    /**/
    preq = (rpc_req_t *)req;

    /**/
    preq->retry = retry;
    preq->tmout = tmout;
    return 0;
}

typedef struct _tag_rpc_header
{
    uint32_t srvid; /* service id, most bit: 0 -->req,  1 --> resp */
    uint32_t sessn; /* session no */

    uint16_t status; /* resp, succ or fail */
    uint16_t msglen;

    /**/
    uint8_t msg[0];

} rpc_hdr_t;

static void rpc_delete_request(rpc_req_t *preq)
{
    /**/
    timer_delete(preq->tid);
    list_delete(&(preq->node));

    /**/
    if (preq->pmsg != NULL)
    {
        free(preq->pmsg);
    }

    free(preq);
    return;
}

static int rpc_make_request(rpc_req_t *preq)
{
    int iret;
    pbuf_t *pkt;
    rpc_hdr_t *phdr;
    uint8_t *pu8;

    /**/
    pkt = pkt_alloc(8);
    if (NULL == pkt)
    {
        return 1;
    }

    /* make pbuf */
    phdr = (rpc_hdr_t *)pkt_append(pkt, sizeof(rpc_hdr_t) + preq->tlen);
    phdr->srvid = preq->srvid;
    phdr->sessn = preq->sessn;
    phdr->status = 0;
    phdr->msglen = preq->tlen;
    memcpy(phdr->msg, preq->pmsg, preq->tlen);

    /* dnode */
    pu8 = (uint8_t *)pkt_prepend(pkt, 1);
    *pu8 = preq->dstnd;

    /**/
    iret = send(rpc_fd, pkt);
    if (iret != 1)
    {
        pkt_free(pkt);
        return 2;
    }

    /**/
    return 0;
}

static void rqc_request_tout(int tid, void *parg)
{
    int iret;
    rpc_req_t *preq;

    /**/
    preq = (rpc_req_t *)parg;
    /**/
    preq->retry -= 1;

    if (preq->retry < 0)
    {
        preq->cbkf(preq->parg, 0, NULL);
        rpc_delete_request(preq);
        return;
    }
    LOG_OUT(LOG_DBG, "-----request timeout,retry count %d,srvid 0x%x, dstnode %d.\n", preq->retry, preq->srvid, preq->dstnd);

    iret = rpc_make_request(preq);
    if (iret != 0)
    {
        preq->cbkf(preq->parg, 0, NULL);
        rpc_delete_request(preq);
        return;
    }

    /**/
    timer_start(preq->tid, preq->tmout, 0);
    return;
}

/**/
int rpc_request_send(intptr_t req, void *pmsg, int tlen)
{
    int iret;
    rpc_req_t *preq;
    uint64_t time;

    /**/
    preq = (rpc_req_t *)req;
    preq->tlen = tlen;
    preq->pmsg = malloc(tlen);
    if (preq->pmsg == NULL)
    {
        return 2;
    }

    /**/
    memcpy(preq->pmsg, pmsg, tlen);

    /**/
    iret = rpc_make_request(preq);
    if (iret != 0)
    {
        printf("make request, %d,srvid:0x%x\n", iret,preq->srvid);
        return 3;
    }

    /* add to link list */
    list_add_head(&req_list, &(preq->node));
    /* timer, timeout */
    preq->tid = timer_create(rqc_request_tout, preq);

    // LOG_OUT(LOG_DBG,"req tmout:%d\n",preq->tmout);
    timer_start(preq->tid, preq->tmout, 0);
    return 0;
}


int rpc_service_register(uint32_t srvid, rpc_srv_f cbkf, void *parg)
{
    rpc_srv_t *psrv;

    /**/
    psrv = (rpc_srv_t *)malloc(sizeof(rpc_srv_t));
    if (psrv == NULL)
    {
        return 1;
    }

    /**/
    psrv->srvid = srvid;
    psrv->cbkf = cbkf;
    psrv->parg = parg;

    /**/
    list_add_head(&srv_list, &(psrv->node));
    return 0;
}

static int rpc_srv_search(uint32_t srvid, rpc_srv_t **ppsrv)
{
    rpc_srv_t *psrv;

    list_for_every_entry(&srv_list, psrv, rpc_srv_t, node)
    {
        if (psrv->srvid == srvid)
        {
            *ppsrv = psrv;
            return 0;
        }
    }

    return 2;
}

static int rpc_req_search(uint32_t srvid, uint32_t sessn, rpc_req_t **ppreq)
{
    rpc_req_t *preq;

    list_for_every_entry(&req_list, preq, rpc_req_t, node)
    {
        if ((preq->srvid == srvid) && (preq->sessn == sessn))
        {
            *ppreq = preq;
            return 0;
        }
    }

    return 2;
}

/* static service session */
typedef struct _tag_srv_session
{
    uint8_t srcnd;
    uint32_t srvid;
    uint32_t sessn; /* session no */

    /**/
    int tlen;
    uint8_t pmsg[1500];

} rpc_srv_session_t;

/**/
static rpc_srv_session_t srv_session;

uint8_t get_srcnid()
{
    return srv_session.srcnd;
}

static int rpc_make_respond(int status)
{
    int iret;
    pbuf_t *pkt;
    rpc_hdr_t *phdr;
    uint8_t *pu8;

    /**/
    pkt = pkt_alloc(8);
    if (NULL == pkt)
    {
        return 1;
    }

    /* make pbuf */
    phdr = (rpc_hdr_t *)pkt_append(pkt, sizeof(rpc_hdr_t) + srv_session.tlen);
    phdr->srvid = srv_session.srvid | 0x80000000;
    phdr->sessn = srv_session.sessn;
    phdr->status = status;
    phdr->msglen = srv_session.tlen;
    if (srv_session.tlen > 0)
    {
        memcpy(phdr->msg, srv_session.pmsg, srv_session.tlen);
    }

    /* dnode */
    pu8 = (uint8_t *)pkt_prepend(pkt, 1);
    *pu8 = srv_session.srcnd;

    /**/
    iret = send(rpc_fd, pkt);
    if (iret != 1)
    {
        pkt_free(pkt);
        return 3;
    }

    /**/
    return 0;
}

int rpc_respond_send(int tlen, void *pmsg)
{
    if (srv_session.tlen != 0)
    {
        return 1;
    }

    if (tlen > 1500)
    {
        return 2;
    }

    /**/
    srv_session.tlen = tlen;
    memcpy(srv_session.pmsg, pmsg, tlen);

    /**/
    rpc_make_respond(0);
    return 0;
}

uint32_t is_ack_zero();
int rpc_proc_input(int qfd, uint8_t tnd)
{
    int iret;
    uint32_t srvid;
    pbuf_t *ipkt;
    uint8_t *ptr;
    rpc_hdr_t *phdr;
    rpc_req_t *preq;
    rpc_srv_t *psrv;

    while (1)
    {

        iret = recv(qfd, &ipkt);
        if (iret < 0)
        {
            break;
        }

        if (iret == 0)
        {
            continue;
        }
        // debug_dump_hex(pkt_to_addr(ipkt),ipkt->length);
        /* too short : 5 + 12 */
        if (ipkt->length < 17)
        {
            pkt_free(ipkt);
            continue;
        }

        /* remove type, len, then check dnode */
        ptr = pkt_to_addr(ipkt);
        if (ptr[0] != 0x18)
        {
            pkt_free(ipkt);
            continue;
        }

        if ((ptr[4] != tnd) && (ptr[4] != 0xff))
        {
            pkt_free(ipkt);
            continue;
        }
        srv_session.srcnd = ptr[3];

        /* skip snode, dnode. */
        phdr = (rpc_hdr_t *)pkt_clip(ipkt, 5);

        /**/
        if (0 == (phdr->srvid & 0x80000000))
        {
            /**/
            srvid = phdr->srvid;

            /**/
            if ((srv_session.srvid == srvid) && (srv_session.sessn == phdr->sessn))
            {
                /* respond re-send */
                rpc_make_respond(0);
            }
            else
            {
                /**/
                srv_session.srvid = srvid;
                srv_session.sessn = phdr->sessn;
                srv_session.tlen = 0;

                /* request, for servide list, search */
                iret = rpc_srv_search(srvid, &psrv);
                if (iret == 0)
                {
                    /* call srv function */
                    psrv->cbkf(psrv->parg, phdr->msglen, phdr->msg);
                }
                else
                {
                    rpc_make_respond(0);
                }
            }
        }
        else
        {
            // printf( "resp, %u, %u\n", phdr->srvid, phdr->sessn );

            /**/
            srvid = phdr->srvid & 0x7FFFFFFF;
            /* respond, for req list, search */
            iret = rpc_req_search(srvid, phdr->sessn, &preq);
            if (iret == 0)
            {
                // LOG_OUT(LOG_DBG,"search srvid 0x%x\n",srvid);
                /* todo : ?? */
                iret = preq->cbkf(preq->parg, phdr->msglen, (void *)(phdr + 1));
                if (0 == iret)
                {
                    // LOG_OUT(LOG_DBG,"del srvid 0x%x\n",srvid);
                    rpc_delete_request(preq);
                }
            }
        }

        /**/
        pkt_free(ipkt);
    }

    return 0;
}

int dbg_cmd_rrpc_ctrl(void *parg, int argc, const char **argv)
{
    sysm_context_t *pctx;

    /**/
    pctx = (sysm_context_t *)parg;

    /**/
    ioctl(pctx->qfd, 0, 0, NULL);
    return 0;
}
int rpc_update_srcid(int nodeid)
{
    srcnid = nodeid;
    return 0;
}

int rpc_init(int qfd)
{
    /**/
    rpc_fd = qfd;
    /**/
    list_initialize(&req_list);
    list_initialize(&srv_list);

    /**/
    srv_session.srvid = -1;
    srv_session.sessn = -1;
    srv_session.tlen = 0;
    return 0;
}
