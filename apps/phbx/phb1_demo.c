/**========================================================================
 * ?                                ABOUT
 * @author         :  liuxinxin
 * @email          :
 * @repo           :
 * @createdOn      :
 * @description    :  2021.12.17
 *========================================================================**/
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "dlist.h"
#include "hcb_comn.h"
#include "pkt_api.h"
#include "sys_api.h"
#include "debug.h"
#include "phb1.h"
#include "atimer.h"
#include "phb1_demo.h"
#include "atbui.h"

#define DEBUG_LOG       0
#define DEBUG_TRACE     0
#define TESEETH_PKPKT   0

char devStr[DEV_NUM][6]=
{
	"error",
	"eth",
	"can",
	"485-0",
	"485-1",
	"485-2",
};

extern uint32_t *ethpkcn;

extern uint8_t bpskey;

extern uint8_t tnnodeid;

#if (TESEETH_PKPKT == 0)
    #define MAX_BURST_PKT_SIZE configPKT_BURSTSIZE
    #define MAX_BURST_TIME     configPKT_BURSTTIME
#else
    /* 1500 / 2 = 750 */
    #define MAX_BURST_PKT_SIZE 750
    /* 1000ms / ((10M/8) / 1500) = 1.2  */
    #define MAX_BURST_TIME     1200
    /* just allow the packet with TEST_PKT_LEN is passed
     * by eth input */
    #define TEST_PKT_LEN 61
    /* log printf */
    #define INPRINTF dprintf
    #define OUTPRINTF dprintf
    /* received packet number by eth input */
    static int inpkcnt = 0;
    /* sent packet number by eth output */
    static int outpkcnt=0;
#endif

/**======================================================
    *?               流量统计
    *@brief 延时统计，cn收到报文，原路返回,
    *@tn收到表示是cn返回的数据
    *@get timer tick/ * data*/
//=======================================================
void traffic_statistics(pbuf_t *ipkt , phb_context_t *pctx)
{
    int iret;
    uint8_t *ptr;
    const char *pflag = "atbtimedelaytick";

    ptr = pkt_to_addr(ipkt);
    if (ipkt->length == (strlen(pflag) + 2 + XHEADER_LEN))
    {
        // printf("get timer tick data.\n");
        if (0 == memcmp(pflag, ptr + 4, strlen(pflag)))
        {
            --ethpkcn[9];
            --ethpkcn[11];
            ptr += (strlen(pflag) + 4);
            // printf("drop pkt:%d--%d\n",*ptr,tnnodeid);
            if (0 == tnnodeid)
            {
                iret = send(pctx->dfd, ipkt);
                if (iret != 1)
                {
                    pkt_free(ipkt);
                }
                // printf("cn reback to tn.\n");
            }
            else if (*ptr == tnnodeid)
            {
                bpsdata.tx_delay = arch_timer_get_current() - bpsdata.tx_lasttime;
                pkt_free(ipkt);
                // printf("tn calc delay:%d.\n",(uint32_t)bpsdata.tx_delay);
            }
            else
            {	
                pkt_free(ipkt);
            }
        }
    }

}

/**==============================================
 **             PackPacket
 *?  PackPacket 小包拼包功能
 *@param name *pctx
 *@param name portid
 *@return none
 *=============================================**/
void timer_send_eth( int tid, void *parg )
{
	int iret;
    phb_context_t *pctx = (phb_context_t *)parg;
    pbuf_t *curpkt;
    xheader_t *phdr;
    uint8_t cn;

    sys_mutex_acquire( &pctx->sem );

	if (NULL == pctx->ethpkt)
	{
        sys_mutex_release( &pctx->sem );
	    return;
	}

    curpkt = (pbuf_t *)pctx->ethpkt;
    pctx->ethpkt = NULL;

    sys_mutex_release( &pctx->sem );

#if DEBUG_LOG
    printf("timer\n");
    debug_dump_hex(pkt_to_addr(curpkt), curpkt->length);
#endif

#if (TESEETH_PKPKT == 1)
    xheader_t *phdr;
    phdr = (xheader_t *)pkt_to_addr(curpkt);
    INPRINTF(0,"itsd %d %d %d\n", inpkcnt, phdr->count,curpkt->length);
    inpkcnt-=phdr->count;
#endif

    phdr = (xheader_t *)pkt_to_addr(curpkt);
    cn = phdr->count;
    ethpkcn[5] += cn;

	iret = send(pctx->dfd, curpkt);
	if (iret != 1)
	{
		pkt_free(curpkt);
        ethpkcn[7] += cn;
	}
    else
    {
        ethpkcn[6] += cn;
        ++ethpkcn[8];
    }

    return;
}

static int recv_pack( phb_context_t *pctx, int revid, pbuf_t *ipkt, pbuf_t **poutpkt )
{
	int iret = -1;
	pbuf_t *curpkt = (pbuf_t *)pctx->ethpkt;
    xheader_t *phdr;
    int revfd = pctx->efd;
    int timer = pctx->timer_id;
    uint8_t *sbuf, *dbuf;

    sys_mutex_acquire( &pctx->sem );

    /* link pkt */
    if(NULL != curpkt)
    {
        /* after first pkt */
        phdr = (xheader_t *)pkt_to_addr(curpkt);
        if ((curpkt->length + ipkt->length + 2) < MAX_BURST_PKT_SIZE)
        {
            sbuf = pkt_prepend(ipkt, 2);
            /*pkt len*/
            *(uint16_t *)sbuf = ipkt->length - 2;
            dbuf = pkt_to_addr(curpkt);
            memcpy(dbuf + curpkt->length, sbuf, ipkt->length);
            curpkt->length += ipkt->length;
            ++phdr->count;
            pkt_free(ipkt);
            *poutpkt = NULL;
#if DEBUG_LOG
            printf("pck %d\n", phdr->count);
            debug_dump_hex(pkt_to_addr(curpkt), curpkt->length);
#endif
        }
        else
        {
#if DEBUG_LOG
            printf("pck-e %d\n", phdr->count);
            debug_dump_hex(pkt_to_addr(curpkt), curpkt->length);
#endif
            timer_stop(timer);
            *poutpkt = curpkt;
            iret = curpkt->length;
            phdr = (xheader_t *)pkt_to_addr(curpkt);
            pctx->ethpkt = NULL;
            curpkt = NULL;
        }
    }

    if (NULL == curpkt)
    {
        /*first pkt*/
        curpkt = pkt_alloc(sizeof(xheader_t));
        pctx->ethpkt = curpkt;
        dbuf = pkt_to_addr(curpkt);
        sbuf = pkt_to_addr(ipkt);
        memcpy(dbuf, sbuf, ipkt->length);
        pkt_append(curpkt, ipkt->length);
        phdr = (xheader_t *)pkt_prepend(curpkt, XHEADER_LEN);
        phdr->id = XHEADER_ID + revid;
        phdr->count = 1;
        phdr->length = curpkt->length - XHEADER_LEN;
        pkt_free(ipkt);

        if ((curpkt->length >= MAX_BURST_PKT_SIZE) && (iret <= 0))
        {
#if DEBUG_LOG
            printf("pck-e %d\n", phdr->count);
            debug_dump_hex(pkt_to_addr(curpkt), curpkt->length);
#endif
            iret = curpkt->length;
            *poutpkt = curpkt;
            phdr = (xheader_t *)pkt_to_addr(curpkt);
            pctx->ethpkt = NULL;
            curpkt = NULL;
        }
        else
        {
            timer_start(timer, MAX_BURST_TIME, 0);
        }
    }

finish:

    sys_mutex_release( &pctx->sem  );

    return iret;
}

static int recv_unpack( int fd, int keephdr, pbuf_t **ppkpkt, pbuf_t **poutpkt )
{
	int iret;
	pbuf_t *ipkt;
	uint8_t *srcbuf;
    uint8_t *desbuf;
    uint16_t len;
    uint8_t *plen;
    pbuf_t *txpkt;

    *poutpkt = NULL;

    if (*ppkpkt == NULL)
    {
        iret = recv(fd, &ipkt);
        if (iret <= 0)
        {
            return iret;
        }
        *ppkpkt = ipkt;

        ++ethpkcn[9];
        srcbuf = pkt_to_addr(ipkt);
        if ((srcbuf[0] & XHEADER_IDMASK) == XHEADER_ID)
        {
            ethpkcn[11] += srcbuf[1];
        }


#if (TESEETH_PKPKT == 1)
        srcbuf = pkt_to_addr(ipkt);
        OUTPRINTF(0, "orecv %d %d %d\n", outpkcnt, srcbuf[1], ipkt->length);
        outpkcnt = srcbuf[1];
#endif
#if DEBUG_LOG
        debug_dump_hex(pkt_to_addr(ipkt), ipkt->length);
#endif
    }
    else
    {
        ipkt = *ppkpkt;
    }

    iret = -1;

    srcbuf = pkt_to_addr(ipkt);
    if ((srcbuf[0] & XHEADER_IDMASK) == XHEADER_ID)
    {
        if (srcbuf[1] > 1)
        {
            plen = (uint8_t *)&len;
            plen[0] = srcbuf[2];
            plen[1] = srcbuf[3];
            if ((len == 0) || (len > PBUF_MAXSIZE))
            {
                pkt_free(*ppkpkt);
                *ppkpkt = NULL;
                ++ethpkcn[10];
                return -1;
            }
            txpkt = pkt_alloc(0);
            desbuf = pkt_to_addr(txpkt);
            memcpy(desbuf, srcbuf, len + XHEADER_LEN);
            desbuf[1] = 1;
            txpkt->length = len + XHEADER_LEN;
            if (!keephdr)
            {
                pkt_clip(txpkt, XHEADER_LEN);
            }
            *poutpkt = txpkt;
            iret = 1;
#if DEBUG_LOG
            printf("unpck %d\n", srcbuf[1]);
            debug_dump_hex(pkt_to_addr(txpkt), txpkt->length);
#endif
            pkt_clip(ipkt, len + XHEADER_LEN - 2);
            desbuf = pkt_to_addr(ipkt);
            desbuf[0] = srcbuf[0];
            desbuf[1] = srcbuf[1] - 1;
        }
        else
        {
            if (!keephdr)
            {
                pkt_clip(ipkt, XHEADER_LEN);
            }
            *poutpkt = ipkt;
            *ppkpkt = NULL;
            iret = 1;
#if DEBUG_LOG
            printf("unpck-e %d\n", srcbuf[1]);
            debug_dump_hex(pkt_to_addr(ipkt), ipkt->length);
#endif

        }
    }
    else
    {
        pkt_free(*ppkpkt);
        *ppkpkt = NULL;
        ++ethpkcn[10];
        return -1;
    }

    return iret;
}

/**==============================================
 **              Autbus
 *?  recv from autbus; send to otherfd
 *@param name pctx
 *@param name fdid
 *@return none
 *=============================================**/


void test_dump_dpdata(phb_context_t *pctx)
{
	int iret;
	pbuf_t *ipkt;
	int otherfd = -1;
	uint8_t *tbuf = NULL;
    uint8_t *pbuf = NULL;
	char *str = NULL;
    uint8_t channel;
    xheader_t *phdr;

#if (DEBUG_TRACE)
	printf("aut data\n");
#endif

    while (1)
    {
        if (devcfg.nodecfg.listenautbus == 0x77)
        {
            iret = recv(pctx->dfd, &ipkt);
            if (iret <= 0)
            {
                break;
            }
            iret = send(pctx->efd, ipkt);
            if (1 != iret)
            {
                pkt_free(ipkt);
            }
            continue;
        }
        if (pctx->packed == 0)
        {
            iret = recv(pctx->dfd, &ipkt);
            if (iret <= 0)
            {
                break;
            }
            ++ethpkcn[11];
#if DEBUG_LOG
            debug_dump_hex(pkt_to_addr(ipkt), ipkt->length);
#endif
        }
        else
        {
            iret = recv_unpack(pctx->dfd, 1, (pbuf_t **)&pctx->dpdatpkt, &ipkt);
            if (iret <= 0)
            {
                break;
            }
        }

        traffic_statistics(ipkt,pctx); //流量统计

		tbuf = pkt_to_addr(ipkt);

        if ((tbuf[0] & XHEADER_IDMASK) == XHEADER_ID)
        {
            channel = tbuf[0] & XHEADER_CHNNLMASK;
        }
        else
        {
            channel = DEV_NUM;
        }
		switch (channel)
		{
		case DEV_ETH:
			otherfd = pctx->efd;
			break;
		case DEV_CAN:
			otherfd = pctx->cfd[0];
			break;
		case DEV_RS485_0:
			otherfd = pctx->rs485fd[0];
			break;
		case DEV_RS485_1:
			otherfd = pctx->rs485fd[1];
			break;
		case DEV_RS485_2:
			otherfd = pctx->rs485fd[2];
			break;
		default:
			otherfd = -1;
			break;
		}

		if( otherfd >= 0 )
		{
            /* throw away transmit header */
            pkt_clip(ipkt, XHEADER_LEN);
            str = devStr[channel];

#if DEBUG_LOG
            printf("aut to %s\n", str);
            debug_dump_hex(pkt_to_addr(ipkt), ipkt->length);
#endif
#if (TESEETH_PKPKT == 1)
            --outpkcnt;
#endif
			iret = send(otherfd, ipkt);
		}
		else
		{
			iret = -1;
		}

		if (iret != 1)
		{
#if (TESEETH_PKPKT == 1)
            OUTPRINTF(0, "osd err %d\n", outpkcnt);
            --outpkcnt;
#endif
			pkt_free(ipkt);
            ++ethpkcn[13];
		}
        else
        {
            ++ethpkcn[12];
        }
	}
}

/**==============================================
 **              ETH
 *?  recv from eth; send to autbus
 *@param name pctx
 *@return none
 *=============================================**/
void test_dump_eth(phb_context_t *pctx)
{
	int iret;
	pbuf_t *ipkt;
	uint8_t *ethbuf = NULL;
    xheader_t *phdr;
    uint8_t cn;

#if ( DEBUG_TRACE)
	printf("eth to aut\n");
#endif

	/**/
	while (1)
	{
        iret = recv(pctx->efd, &ipkt);
        if (iret <= 0)
        {
             break;
        }

        ++ethpkcn[4];

#if (TESEETH_PKPKT == 1)
        if(ipkt->length == TEST_PKT_LEN)
        {
             ++inpkcnt;
        }
        else
        {
            pkt_free(ipkt);
            continue;
        }
#endif

        if(pctx->packed == 1)
        {
            iret = recv_pack(pctx, DEV_ETH,  ipkt, &ipkt);
            if (iret <= 0)
            {
                continue;
            }
        }
        else
        {
            phdr = (xheader_t *)pkt_prepend(ipkt, XHEADER_LEN);
            phdr->id = XHEADER_ID + DEV_ETH;
            phdr->count = 0x1;
            phdr->length = ipkt->length;
        }

#if DEBUG_LOG
        printf("eth to aut\n");
		debug_dump_hex(pkt_to_addr(ipkt), ipkt->length);
#endif
#if (TESEETH_PKPKT == 1)
        xheader_t *phdr;
        phdr = (xheader_t *)pkt_to_addr(ipkt);
        INPRINTF(0,"isd %d %d %d\n", inpkcnt, phdr->count,ipkt->length);
        inpkcnt-=phdr->count;
#endif
        phdr = (xheader_t *)pkt_to_addr(ipkt);
        if(pctx->packed == 1)
        {
            cn = phdr->count;
        }
        else
        {
            cn = 1;
        }
        ethpkcn[5] += cn;
        

		iret = send(pctx->dfd, ipkt);
		if (iret != 1)
		{
			pkt_free(ipkt);
            ethpkcn[7] += cn;
		}
        else
        {
            ethpkcn[6] += cn;
            ++ethpkcn[8];
        }
	}
}

/**==============================================
 **              rs485
 *?  recv from rs485; send to autbus
 *@param name *pctx
 *@param name portid
 *@return none
 *=============================================**/
void test_suart485(phb_context_t *pctx, int portid)
{
	int iret;
	int sfd;
	pbuf_t *ipkt;
	uint8_t *rs485buf = NULL;
	volatile int rs485id;

	switch (portid)
	{
	case SUART_PORT_0:
		sfd = pctx->rs485fd[0];
		rs485id = DEV_RS485_0;
		break;
	case SUART_PORT_1:
		sfd = pctx->rs485fd[1];
		rs485id = DEV_RS485_1;
		break;
	case SUART_PORT_2:
		sfd = pctx->rs485fd[2];
		rs485id = DEV_RS485_2;
		break;
	default:
		sfd = -1;
		break;
	}
	if (sfd < 0)
	{
		return;
	}

#if (DEBUG_TRACE)
	printf("485-%d to aut\n", portid);
#endif

	/**/
	while (1)
	{
        iret = recv(sfd, &ipkt);
        if (iret <= 0)
        {
            break;
        }
#if DEBUG_LOG
        debug_dump_hex(pkt_to_addr(ipkt), ipkt->length);
#endif

        xheader_t *phdr;
        phdr = (xheader_t *)pkt_prepend(ipkt, XHEADER_LEN);
        phdr->id = XHEADER_ID + rs485id;
        phdr->count = 0x1;
        phdr->length = ipkt->length;

#if DEBUG_LOG
        printf("485-%d to aut\n", portid);
	    debug_dump_hex(pkt_to_addr(ipkt), ipkt->length);
#endif

		iret = send(pctx->dfd, ipkt);
		if (iret != 1)
		{
			pkt_free(ipkt);
		}
	}
}

/**==============================================
 **              CAN
 *?  recv from can; send to autbus
 *@param name *pctx
 *@param name portid
 *@return none
 *=============================================**/
void test_dump_can(phb_context_t *pctx)
{
	int iret;
	pbuf_t *ipkt;
	uint8_t *canbuf = NULL;

#if (DEBUG_TRACE)
	printf("can to aut\n");
#endif

	while (1)
	{
		iret = recv(pctx->cfd[0], &ipkt);
		if (iret < 0)
		{
			break;
		}

#if DEBUG_LOG
        printf("can to aut\n");
		debug_dump_hex(pkt_to_addr(ipkt), ipkt->length);
#endif

        xheader_t *phdr;
        phdr = (xheader_t *)pkt_prepend(ipkt, XHEADER_LEN);
        phdr->id = XHEADER_ID + DEV_CAN;
        phdr->count = 0x1;
        phdr->length = ipkt->length;

		iret = send(pctx->dfd, ipkt);
		if (iret != 1)
		{
			pkt_free(ipkt);
		}
	}
}


