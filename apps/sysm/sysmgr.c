
#include <stdlib.h>
#include <stdint.h>

#include "dlist.h"
#include "hcb_comn.h"
#include "pkt_api.h"
#include "sys_api.h"

#include "hcb_msg.h"
#include "atimer.h"
#include "sysmgr.h"
#include "debug.h"


/**/
typedef struct _tag_sysm_cb
{
	struct list_node  tnd;				/* list node */
	uint8_t  hmsg;						/* key */

	int  tmrid;							/* timer id */
	sysm_cbk_f  scbk;					/* cbk func */
	void * parg;						/* cbk arg  */
	
} sysm_cb_s;


/**/
int  sysm_fd;
struct list_node  sysm_head;			/* search list header */
sysm_cb_s  sysm_cmn_cbk;



void   sysmgr_timer_cbk( int tid, void * parg )
{
	sysm_cb_s * pcb;

	/**/
	pcb = (	sysm_cb_s *)parg;
	
	/* remove register */
	list_delete( &(pcb->tnd) );

	/* call back,  */
	if ( pcb->scbk != NULL )
	{
		pcb->scbk( pcb->parg, NULL );
	}
	
	/**/
	free( pcb );
	return;
	
}


/*
*/
int  sysmgr_send( uint8_t * tbuf, sysm_cbk_f scbk, void * parg, int tcnt )
{
	int  iret;
	int  tid;
	sysm_cb_s * pcb;
	hcb_msg_hdr * phdr;
	/* arg check */
	phdr = (hcb_msg_hdr *)tbuf;
	if ( phdr->type > HCB_MSG_MAX )
	{
		return 1;
	}
	
	if ( phdr->leng > 128 )
	{
		return 2;
	}

	/* send only */
	if ( scbk == NULL )
	{
		/* send ctrl ;sysm_fd is dfd*/
		iret = write( sysm_fd, phdr, phdr->leng );
		if ( iret < 0 )
		{
			return iret;
		}

		/**/
		return 0;
	}

	
	/* */
	if ( tcnt <= 0 )
	{
		return 3;
	}
	
	/**/
	pcb = (sysm_cb_s *)malloc( sizeof(sysm_cb_s) );
	if ( pcb == NULL )
	{
		return 3;
	}

	/**/
	pcb->hmsg = phdr->type;
	pcb->tmrid = 0;
	pcb->scbk = scbk;
	pcb->parg = parg;
	
	/* create timer */
	tid = timer_create( sysmgr_timer_cbk, pcb );
	if ( tid <= 0 )
	{
		free( pcb );	
		return 4;
	}

	/**/
	pcb->tmrid = tid;
	
	/* send ctrl */
	iret = write( sysm_fd, phdr, phdr->leng );
	if ( iret <= 0 )
	{
		/**/
		timer_delete( tid );
		free( pcb );
		return 5;
	}
	
	/* register call back */
	timer_start( tid, tcnt, 0 );	
	list_add_tail( &sysm_head, &(pcb->tnd) );
	return 0;
	
}



int  sysmgr_reg_cbk( sysm_cbk_f scbk, void * parg )
{
	sysm_cmn_cbk.scbk = scbk;
	sysm_cmn_cbk.parg = parg;
	return 0;
}



int  sysmgr_proc_event( void )
{
	int  iret;
	uint8_t  tbuf[128];
	sysm_cb_s * pcb;
	hcb_msg_hdr * phdr;
	uint8_t  hmsg;
	uint64_t temptimer[3];
	
	
	/**/
	while ( 1 )
	{
		/* try read */
		// printf("read*-***************\n");
		iret = read( sysm_fd, (void *)tbuf, 128 );
		if ( iret < 0 )
		{
			break;
		}
		// temptimer[0] = arch_timer_get_current();
		// temptimer[1] = read_reg(0x701f60);
		// temptimer[1] = (temptimer[1] <<32|(read_reg(0x701f5c)));
		// printf("------TIMER %Ld\r\n",(temptimer[0]-temptimer[1]));
		//  debug_dump_hex(tbuf,32);
		
		/**/
		phdr = (hcb_msg_hdr *)tbuf;
		hmsg = phdr->type;
		// printf("msg type:%d msg len: %d\n",hmsg,phdr->leng);

		/* check */
		if ( phdr->leng > 128 )
		{
			continue;
		}

		/* report message */
		if ( hmsg >= HCB_MSG_RPT_NWNODE )
		{
			if ( sysm_cmn_cbk.scbk != NULL )
			{
				sysm_cmn_cbk.scbk( sysm_cmn_cbk.parg, tbuf );
			}

			/**/
			continue;
		}
		
		/* for each, search */
		list_for_every_entry( &sysm_head, pcb, sysm_cb_s, tnd )
		{
			if ( pcb->hmsg == hmsg )
			{
				/* remove register */
				list_delete( &(pcb->tnd) );
				
				/* call back,  */
				if ( pcb->scbk != NULL )
				{
					pcb->scbk( pcb->parg, tbuf );
				}

				/**/
				timer_delete( pcb->tmrid );
				free( pcb );
				break;
			}
		}
		
		/**/
		
	}

	
	/**/
	return 0;
	
}



int  sysmgr_init( int tfd )
{
	/**/
	sysm_fd = tfd;

	/**/
	list_initialize( &sysm_head );
	return 0;
}


