
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "sys_api.h"

#include "atimer.h"
#include "minip.h"
#include "tftp.h"

// TFTP Opcodes:
#define TFTP_OPCODE_RRQ                 1
#define TFTP_OPCODE_WRQ                 2
#define TFTP_OPCODE_DATA                3
#define TFTP_OPCODE_ACK                 4
#define TFTP_OPCODE_ERROR               5

// TFTP Errors:
#define TFTP_ERROR_UNDEF                0
#define TFTP_ERROR_NOT_FOUND            1
#define TFTP_ERROR_ACCESS               2
#define TFTP_ERROR_FULL                 3
#define TFTP_ERROR_ILLEGAL_OP           4
#define TFTP_ERROR_UNKNOWN_XFER         5
#define TFTP_ERROR_EXISTS               6
#define TFTP_ERROR_NO_SUCH_USER         7


/**/
static  tftp_wrq_open_t  sys_wrq_open = NULL;
static  tftp_rrq_open_t  sys_rrq_open = NULL;

/**/
pbuf_t * tftp_build_error_pkt( uint16_t ecode, char * emsg )
{
    int  tlen;
	pbuf_t * tpkt;
	uint16_t * pu16;
    char * nmsg = "None";

	/* opcode(5), ecode, emsg, 0 */
	tpkt = pkt_alloc( 48 );
    if ( tpkt == NULL )
    {
        return NULL;
    }

    /**/
    if ( emsg == NULL )
    {
        emsg = nmsg;        
    }

    /**/
    tlen = sizeof(uint16_t) + sizeof(uint16_t) + strlen(emsg) + 1;
	pu16 = (uint16_t *)pkt_append( tpkt, tlen );
    *pu16++ = __htons( TFTP_OPCODE_ERROR );
    *pu16++ = __htons( ecode );

    /**/
    strcpy( (char *)pu16, emsg );
    return tpkt;

}


int  tftp_send_error_direct( int sock, uint32_t dstip, uint16_t dstport, uint16_t ecode, char * emsg )
{
    int  iret;
    pbuf_t * tpkt;

    /**/
    tpkt = tftp_build_error_pkt( ecode, emsg );
    if ( tpkt == NULL )
    {
        return -1;
    }

    /**/
	iret = minsock_udp_send_to( sock, dstip, dstport, tpkt );
    if ( iret != 0 )
    {
        /* todo : free ??? */
        return -2;
    }

    return 0;
}


int  tftp_send_error( int sock, uint16_t ecode, char * emsg )
{
    int  iret;
    pbuf_t * tpkt;

    /**/
    tpkt = tftp_build_error_pkt( ecode, emsg );
    if ( tpkt == NULL )
    {
        return -1;
    }

    /**/
	iret = minsock_udp_send( sock, tpkt );
    if ( iret != 0 )
    {
        /* todo : free ??? */
        return -2;
    }

    /**/
    return  0;
}

int  tftp_send_ack( int sock, uint16_t snblk )
{
    int  iret;
    int  tlen;
    pbuf_t * tpkt;
    uint16_t * pu16;

    /* opcode(4), blk_sn */
	tpkt = pkt_alloc( 48 );
    if ( tpkt == NULL )
    {
        return -1;
    }

    /**/
    tlen = sizeof(uint16_t) + sizeof(uint16_t);
    pu16 = (uint16_t *)pkt_append( tpkt, tlen );
    *pu16++ = __htons( TFTP_OPCODE_ACK );
    *pu16++ = __htons( snblk );

    /**/
    iret = minsock_udp_send( sock, tpkt );
    if ( iret != 0 )
    {
        /* todo : free ??? */
        return -2;
    }

    return 0;
}


int  tftp_send_data( int sock, uint16_t snblk, uint8_t * pdat, int tsiz )
{
    int  iret;
    int  tlen;
    pbuf_t * tpkt;
    uint16_t * pu16;

    /* opcode(4), blk_sn */
	tpkt = pkt_alloc( 48 );
    if ( tpkt == NULL )
    {
        return -1;
    }

    /**/
    tlen = sizeof(uint16_t) + sizeof(uint16_t) + tsiz;
    pu16 = (uint16_t *)pkt_append( tpkt, tlen );
    *pu16++ = __htons( TFTP_OPCODE_DATA );
    *pu16++ = __htons( snblk );

    /**/
    if ( tsiz > 0 )
    {
        memcpy( (void *)pu16, pdat, tsiz );
    }

    /**/
    iret = minsock_udp_send( sock, tpkt );
    if ( iret != 0 )
    {
        /* todo : free ??? */
        return -2;
    }

    return 0;
}


typedef struct _tag_tftp_session {

    /**/
    int  ffd;           /* file desc */
    tftp_file_read_t  pread;
    tftp_file_write_t  pwrite;
    tftp_file_close_t  pclose;
    
    /**/
    int  sock;          /* udp socket */
    int  ttmr;          /* timer handle */

    /**/
    uint8_t  tpriv[2];

} tftp_session;


typedef struct _tag_write_priv {
    
    /**/
    int  retry;
    uint16_t  rsn;      /* ready block sn */
    uint32_t  ofs;      /* ready bytes */

} write_priv;


typedef struct _tag_read_priv {
    
    /**/
    int  retry;
    uint16_t  rsn;      /* ready block sn */
    uint32_t  ofs;      /* ready bytes */

    /**/
    int  tlen;
    uint8_t  tary[512];

} read_priv;


void  tftp_close_session( tftp_session * pssen )
{
    /* close file desc, ignore ret */
    if ( pssen->ffd >= 0 )
    {
        pssen->pclose( pssen->ffd );
    }

    /**/
    if ( pssen->ttmr >= 0 )
    {
        timer_delete( pssen->ttmr );
    }

    if ( pssen->sock >= 0 )
    {
        minsock_udp_free( pssen->sock );
    }
    
    /**/
    free( pssen );
    return;
}



int  tftp_new_session( uint32_t srcip, uint16_t srcport, udp_cbk_t pucbk, timer_cbk_t ptcbk, int szpriv, tftp_session ** ppssen )
{
    int  iret;
    tftp_session * pssen;

    /**/
    pssen = (tftp_session *)malloc( sizeof(tftp_session) + szpriv );
    if ( pssen == NULL )
    {
        return -1;
    }

    /**/
    pssen->pwrite = NULL;
    pssen->pread = NULL;
    pssen->pclose = NULL;
    pssen->ffd = -1;
    pssen->sock = -1;
    pssen->ttmr = -1;

    /* new sock */
    pssen->sock = minsock_udp_new( 0 );
    if ( pssen->sock < 0 )
    {
        tftp_close_session( pssen );
        return -2;
    }

    minsock_udp_connect(pssen->sock, srcip, srcport);
    minsock_udp_listen(pssen->sock, pucbk, pssen );

    /* timer, wait data message */
    pssen->ttmr = timer_create( ptcbk, pssen );
    if ( pssen->ttmr < 0 )
    {
        tftp_close_session( pssen );
        return 307;
    }

    /**/
    *ppssen = pssen;
    return 0;
}


void  tftp_write_timer_cbk( int tid, void * parg )
{
    tftp_session * pssen;
    write_priv * priv;

    /**/
    pssen = (tftp_session *)parg;
    priv = (write_priv *)pssen->tpriv;
    
    /**/
    priv->retry += 1;
    if ( priv->retry >= 3 )
    {
        /* stop */
        tftp_send_error( pssen->sock, 200, "retry max" );
        tftp_close_session( pssen );
        return;
    }

    /* retry send ack */
    tftp_send_ack( pssen->sock, priv->rsn );
    return;
}


void  tftp_read_timer_cbk( int tid, void * parg )
{
    tftp_session * pssen;
    read_priv * priv;

    /**/
    pssen = (tftp_session *)parg;
    priv = (read_priv *)pssen->tpriv;

    /**/    
    priv->retry += 1;
    if ( priv->retry >= 3 )
    {
        /* stop */
        tftp_send_error( pssen->sock, 200, "retry max" );
        tftp_close_session( pssen );
        return;
    }

    /* retry send data */
    tftp_send_data( pssen->sock, priv->rsn, priv->tary, priv->tlen );
    return;
}



void  tftp_read_sock_cbk( int sock, pbuf_t * ipkt, uint32_t srcip, uint16_t srcport, void * parg )
{
    int  iret;
    uint16_t * pu16;
    uint16_t temp;
    tftp_session * pssen;
    read_priv * priv;

    /**/
    pssen = (tftp_session *)parg;
    priv = (read_priv *)pssen->tpriv;

    /**/
    if ( ipkt->length < 4 )
    {
        /* nothing ?? */
        return;
    }

    /* check ack */
    pu16 = (uint16_t *)pkt_to_addr( ipkt );
    temp = __ntohs( *pu16++ );
    if ( temp != TFTP_OPCODE_ACK )
    {
        tftp_send_error( sock, TFTP_ERROR_UNKNOWN_XFER, "type err" );
        tftp_close_session( pssen );
        return;
    }

    /**/
    temp = __ntohs( *pu16++ );
    if ( temp == priv->rsn )
    {
        /* final?? */
        if ( priv->tlen < 512 )
        {
            tftp_close_session( pssen );
            return;
        }

        /* try read next block */
        iret = pssen->pread( pssen->ffd, priv->tary, 512 );
        if ( iret < 0 )
        {
            tftp_send_error( sock, TFTP_ERROR_ACCESS, "io err" );
            tftp_close_session( pssen );
        }

        /**/
        priv->tlen = iret;
        priv->rsn += 1;

        /**/
        timer_start( pssen->ttmr, 10000, 10000 );
        tftp_send_data( sock, priv->rsn, priv->tary, priv->tlen );
    }
    else if ( (uint16_t)(temp + 1) == priv->rsn )
    {
        timer_start( pssen->ttmr, 10000, 10000 );
        tftp_send_data( sock, priv->rsn, priv->tary, priv->tlen );
    }
    else
    {
        /* error, close */
        tftp_send_error( sock, TFTP_ERROR_UNKNOWN_XFER, "sn xxx" );
        tftp_close_session( pssen );
    }
    
    return;
}


void  tftp_write_sock_cbk( int sock, pbuf_t * ipkt, uint32_t srcip, uint16_t srcport, void * parg )
{
    int  iret;
    int  tlen;
    uint16_t * pu16;
    uint16_t  opcode;
    uint16_t  nblk;
    tftp_session * pssen;
    write_priv * priv;

    /**/
    pssen = (tftp_session *)parg;
    priv = (write_priv *)pssen->tpriv;

    /**/
    if ( ipkt->length < 4 )
    {
        /* error */
        tftp_send_error( sock, TFTP_ERROR_ILLEGAL_OP, "small packet" );
        tftp_close_session( pssen );
        return;
    }

    /**/
    pu16 = (uint16_t *)pkt_to_addr( ipkt );
    opcode = __ntohs( *pu16++ );
    nblk =  __ntohs( *pu16++ );

    /**/
    if ( opcode != TFTP_OPCODE_DATA )
    {
        /* error */
        // tftp_send_error( sock, TFTP_ERROR_ILLEGAL_OP, "not data" );
        tftp_close_session( pssen );
        return;
    }

    /* timer restart */
    priv->retry = 0;
    timer_start( pssen->ttmr, 10000, 10000 );

    /* check sn */
    if ( nblk != (uint16_t)(priv->rsn + 1) )
    {
        /* retry */
        tftp_send_ack( sock, priv->rsn );
        return;
    }

    /**/
    tlen = sizeof(uint16_t) + sizeof(uint16_t);
    pkt_clip( ipkt, tlen );
    
    if ( ipkt->length == 512 )
    {
        /**/
        priv->rsn = nblk;
        priv->ofs += 512;

        /**/
        iret = pssen->pwrite( pssen->ffd, pkt_to_addr(ipkt), 512 );
        if ( iret != 0 )
        {
            /* close session */
            tftp_send_error( sock, TFTP_ERROR_FULL, "write fail" );
            tftp_close_session( pssen );
        }
        else
        {
            /* ok, expect next pkt */
            tftp_send_ack( pssen->sock, nblk );
        }
        
    }
    else if ( ipkt->length > 512 )
    {
        /* check fail, session close */
        tftp_send_error( sock, TFTP_ERROR_UNKNOWN_XFER, "size beyond 512" );
        tftp_close_session( pssen );
    }
    else
    {
        if ( ipkt->length > 0 )
        {
            /**/
            priv->rsn = nblk;
            priv->ofs += ipkt->length;

            /**/
            iret = pssen->pwrite( pssen->ffd, pkt_to_addr(ipkt), ipkt->length );
            if ( iret != 0 )
            {
                /* close session */
                tftp_send_error( sock, TFTP_ERROR_FULL, "write fail" );
                tftp_close_session( pssen );
                return;
            }
        }

        /* close */
        printf( "\nrecv: %u, total = %u\n", nblk, priv->ofs );
        tftp_send_ack( sock, nblk );
        tftp_close_session( pssen );
        
    }

    return;
}



#if 0

int  tftp_new_rd_session( char * fname, uint32_t srcip, uint16_t srcport )
{
    int  iret;
    tftp_file_read_t  pread;
    tftp_file_close_t  pclose;
    read_session * pssen;

    /**/
    iret = sys_rrq_open( fname, &pread, &pclose);
    if ( iret < 0 )
    {
        return 200 - iret;
    }

    /**/
    pssen = (read_session *)malloc( sizeof(read_session) );
    if ( pssen == NULL )
    {
        pclose(iret);
        return 301;
    }

    /**/
    pssen->ffd = iret;
    pssen->pread = pread;
    pssen->pclose = pclose;
    pssen->retry = 0;
    pssen->rsn = 1;
    pssen->ofs = 0;
    pssen->tlen = 0;
    pssen->sock = -1;
    pssen->ttmr = -1;

    /* new sock */
    pssen->sock = minsock_udp_new( 0 );
    if ( pssen->sock < 0 )
    {
        tftp_read_ssen_close( pssen );
        return 302;
    }

    minsock_udp_connect(pssen->sock, srcip, srcport);
    minsock_udp_listen(pssen->sock, tftp_read_sock_cbk, pssen );

    /* timer, wait data message */
    pssen->ttmr = timer_create(tftp_read_timer_cbk, pssen);
    if (pssen->ttmr < 0)
    {
        tftp_read_ssen_close( pssen );
        return 307;
    }

    /* read block */
    iret = pssen->pread( pssen->ffd, pssen->tary, 512 );
    if ( iret < 0 )
    {
        tftp_read_ssen_close( pssen );
        return 308;
    }

    /**/
    pssen->tlen = iret;

    /* 10ms */
    timer_start( pssen->ttmr, 10000, 10000 );
    tftp_send_data( pssen->sock, pssen->rsn, pssen->tary, pssen->tlen );
    return 0;
}


int  tftp_new_wr_session( char * fname, uint32_t srcip, uint16_t srcport )
{
    int  iret;
    tftp_file_write_t  pwrite;
    tftp_file_close_t  pclose;
    write_session * pssen;

    /**/
    iret = sys_wrq_open( fname, &pwrite, &pclose);
    if ( iret < 0 )
    {
        return 200 - iret;
    }

    /**/
    pssen = (write_session *)malloc( sizeof(write_session) );
    if ( pssen == NULL )
    {
        pclose(iret);
        return 301;
    }

    /**/
    pssen->ffd = iret;
    pssen->pwrite = pwrite;
    pssen->pclose = pclose;
    pssen->retry = 0;
    pssen->rsn = 0;
    pssen->ofs = 0;

    /* new sock */
    pssen->sock = minsock_udp_new(0);
    if (pssen->sock < 0)
    {
        tftp_write_ssen_close( pssen );
        return 302;
    }

    minsock_udp_connect( pssen->sock, srcip, srcport );
    minsock_udp_listen( pssen->sock, tftp_write_sock_cbk, pssen );

    /* timer, wait data message */
    pssen->ttmr = timer_create( tftp_write_timer_cbk, pssen );
    if (pssen->ttmr < 0)
    {
        tftp_write_ssen_close( pssen );
        return 307;
    }

    /* 10ms */
    timer_start(pssen->ttmr, 10000, 10000);
    tftp_send_ack(pssen->sock, pssen->rsn);
    return 0;
}

#endif


void  tftp_srv_sock_cbk( int sock, pbuf_t * ipkt, uint32_t srcip, uint16_t srcport, void * parg )
{
    int  iret;
    tftp_session * pssen;
    uint16_t  opcode;
    char * pmsg;    
    char * popt;
    int  maxs;
    int  tlen;
    int  ffd;
    tftp_file_read_t  pread;
    tftp_file_write_t  pwrite;
    tftp_file_close_t  pclose;
    

    do  {

        if ( ipkt->length < 4 )
        {
            iret = TFTP_ERROR_UNDEF;
            break;
        }

        /**/
        pmsg = (char *)pkt_to_addr( ipkt );
        opcode = __ntohs( *(uint16_t *)pmsg );
        if ( (opcode != TFTP_OPCODE_WRQ) && (opcode != TFTP_OPCODE_RRQ) )
        {
            iret = TFTP_ERROR_ILLEGAL_OP;
            break;
        }

        /**/
        pmsg = (char *)pkt_clip( ipkt, sizeof(uint16_t) );
        maxs = ipkt->length;

        /**/
        tlen = strnlen( pmsg, maxs );
        if ( tlen >= maxs )
        {
            /* no tail zero */
            iret = 101;
            break;
        }

        /**/
        maxs = maxs - tlen - 1;
        popt = pmsg + tlen + 1;

        /* mode is octet ? */
        if ( maxs < 2 )
        {
            iret = 102;
            break;
        }
    
        tlen = strnlen( popt, maxs );
        if ( tlen >= maxs )
        {
            iret = 103;
            break;
        }

        // printf( "name: %s\n", pmsg );    
        // printf( "opt: %s\n", popt );
        if ( 0 != strcmp( popt, "octet" ) )
        {
            iret = 105;
            break;
        }

        if ( opcode == TFTP_OPCODE_WRQ )
        {
            write_priv * priv;

            /**/
            ffd = sys_wrq_open( pmsg, &pwrite, &pclose );
            if ( ffd < 0 )
            {
                iret = 200 - ffd;
                break;
            }

            /* malloc session */
            iret = tftp_new_session( srcip, srcport, tftp_write_sock_cbk, tftp_write_timer_cbk, sizeof(write_priv), &pssen );
            if ( iret < 0 )
            {
                iret = 200 - iret;
                break;
            }
            
            /**/
            pssen->ffd = ffd;
            pssen->pwrite = pwrite;
            pssen->pclose = pclose;

            priv = (write_priv *)pssen->tpriv;
            priv->ofs = 0;
            priv->retry = 0;
            priv->rsn = 0;

            /* 10ms */
            timer_start(pssen->ttmr, 10000, 10000);
            tftp_send_ack(pssen->sock, priv->rsn);
            return;
        }
        else
        {
            read_priv * priv;

            /**/
            ffd = sys_rrq_open( pmsg, &pread, &pclose);
            if ( ffd < 0 )
            {
                iret = 200 - ffd;
                break;
            }

            /* malloc session */
            iret = tftp_new_session( srcip, srcport, tftp_read_sock_cbk, tftp_read_timer_cbk, sizeof(read_priv), &pssen );
            if ( iret < 0 )
            {
                iret = 200 - iret;
                break;
            }

            /**/
            pssen->ffd = ffd;
            pssen->pread = pread;
            pssen->pclose = pclose;

            priv = (read_priv *)pssen->tpriv;
            priv->ofs = 0;
            priv->retry = 0;
            priv->rsn = 1;

            /* read block */
            iret = pssen->pread( pssen->ffd, priv->tary, 512 );
            if ( iret < 0 )
            {
                tftp_close_session( pssen );
                iret = 300 - iret;
                break;
            }

            /**/
            priv->tlen = iret;

            /* 10ms */
            timer_start( pssen->ttmr, 10000, 10000 );
            tftp_send_data( pssen->sock, priv->rsn, priv->tary, priv->tlen );            
            return;
        }

    } while( 0 );

    /**/
    if ( iret != 0 )
    {
        tftp_send_error_direct( sock, srcip, srcport, iret, NULL );
    }

    return;
}


int  tftp_init( uint16_t srvport, tftp_wrq_open_t pwrqop, tftp_rrq_open_t prrqop )
{
    int  tsck;

    /**/
    sys_wrq_open = pwrqop;
    sys_rrq_open = prrqop;

    /**/
	tsck = minsock_udp_new( srvport );
	minsock_udp_listen( tsck, tftp_srv_sock_cbk, NULL );

    /**/
    return 0;
}


