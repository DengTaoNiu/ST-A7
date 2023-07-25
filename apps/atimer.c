
#include <stdint.h>
#include <string.h>

#include "dlist.h"
#include "sys_api.h"

#include "debug.h"
#include "atimer.h"



#define  IFG_CALLBK			0x8000
#define  IFG_DELETE			0x4000
#define  IFG_STOPED			0x2000
#define  IFG_RSTART			0x1000




static void  timer_insert( comn_context *pctx, tq_node_t * tinfo )
{
	tq_node_t * curn;

	/**/
	if (list_is_empty(&(pctx->tq_list)))
	{
		/* insert and ...*/
		list_add_head(&(pctx->tq_list), &(tinfo->node));
	}
	else
	{
		/* 找到合适的位置. */
		list_for_every_entry(&(pctx->tq_list), curn, tq_node_t, node)
		{
			if (tinfo->nxt_tick < curn->nxt_tick)
			{
				break;
			}
		}

		/**/
		list_add_before(&(curn->node), &(tinfo->node));
	}

	/**/
	return;
}



int  timer_queue_event( void )
{
	comn_context * pctx;
	tq_node_t * tinfo;
	uint64_t  curr;


	/**/
	pctx = (comn_context *)tls_get();
	read( pctx->fd_timer, &curr, sizeof(curr) );
	
	/**/
	while ( 1 )
	{
		/**/
		if ( list_is_empty( &(pctx->tq_list) ) )
		{
			pctx->tq_nxt_tick = 0;
			write( pctx->fd_timer, &(pctx->tq_nxt_tick), sizeof(pctx->tq_nxt_tick) );
			return 0;
		}
		
		/* get list head */
		tinfo = (tq_node_t *)( pctx->tq_list.next );
		
		/**/
		if ( tinfo->nxt_tick > curr )
		{
			pctx->tq_nxt_tick = tinfo->nxt_tick;		
			write( pctx->fd_timer, &pctx->tq_nxt_tick, sizeof(pctx->tq_nxt_tick) );
			return 0;
		}
		
		/* fire */
		list_delete( &(tinfo->node) );
		tinfo->iflag = IFG_CALLBK;
		tinfo->cbkf( (int)(intptr_t)tinfo, tinfo->parg );

		/**/
		if ( tinfo->iflag & IFG_DELETE )
		{
			free( tinfo );
			continue;			
		}
		
		if ( tinfo->iflag & IFG_STOPED )
		{
			tinfo->iflag = 0;
			continue;
		}
		
		/**/
		if ( tinfo->iflag & IFG_RSTART )
		{
			tinfo->iflag = 0;
			timer_insert( pctx, tinfo );
		}
		else if ( tinfo->irepeat != 0 )
		{
			/* insert */
			tinfo->iflag = 0;
			tinfo->nxt_tick += tinfo->irepeat;
			timer_insert( pctx, tinfo );
		}
		else
		{
			/* clear flag */
			tinfo->iflag = 0;
		}
	}

	/* 是否需要重新设置 hardware timer */
	tinfo = (tq_node_t *)( pctx->tq_list.next );
	if ( pctx->tq_nxt_tick != tinfo->nxt_tick )
	{
		/* */
		pctx->tq_nxt_tick = tinfo->nxt_tick;		
		write( pctx->fd_timer, &(pctx->tq_nxt_tick), sizeof(pctx->tq_nxt_tick) );
	}

}


int  timer_queue_init( void )
{
	comn_context * pctx;

	/**/
	pctx = (comn_context *)tls_get();	


	/**/
	list_initialize( &(pctx->tq_list) );
	
	/**/
	pctx->tq_nxt_tick = 0;
	write( pctx->fd_timer, &(pctx->tq_nxt_tick), sizeof(pctx->tq_nxt_tick) );
	
	/**/
	return 0;
}


/**/
int  timer_create( timer_cbk_t cbkf, void * parg )
{
	tq_node_t * ptqn;

	/**/
	ptqn = (tq_node_t *)malloc( sizeof(tq_node_t) );
	if ( NULL == ptqn )
	{
		return -1;
	}

	/**/
	list_clear_node( &(ptqn->node) );
	ptqn->iflag = 0;
	ptqn->irepeat = 0;
	ptqn->nxt_tick = 0;
	ptqn->cbkf = cbkf;
	ptqn->parg = parg;

	/**/
	return (int)(intptr_t)ptqn;
	
}


/* 
u second.
itv : first time out interval.
irt : repeat timer out interval.
*/
int  timer_start( int tid, uint32_t itv, uint32_t irt )
{
	tq_node_t * ptqn;
	tq_node_t * curn;
	uint64_t  temp;
	comn_context * pctx;

	/**/
	pctx = (comn_context *)tls_get(); //tls机制。防止多线程访问同一变量。	
	ptqn = (tq_node_t *)(intptr_t)tid;
	if ( ptqn->iflag & IFG_CALLBK )
	{
		/* 如果是在 callback 中又调用了 start 函数, 相当于重新设置了 repeat 值.. */
		ptqn->iflag |= IFG_RSTART;
		ptqn->nxt_tick += itv;
		ptqn->irepeat = irt;
		return 0;
	}
	
	/* 如果已经在 运行状态, 先删除它..  */
	if ( list_in_list( &(ptqn->node) ) )
	{
		list_delete( &(ptqn->node) );		
	}
	
	/**/
	read( pctx->fd_timer, &temp, sizeof(temp) );	
	ptqn->iflag = 0;
	ptqn->nxt_tick = temp + itv;
	ptqn->irepeat = irt;
	
	/**/
	if ( list_is_empty( &(pctx->tq_list) ) )
	{
		/* insert and ...*/
		list_add_head( &(pctx->tq_list), &(ptqn->node) );
	}
	else
	{
		/* 找到合适的位置. */
		list_for_every_entry( &(pctx->tq_list), curn, tq_node_t, node )
		{
			if ( ptqn->nxt_tick < curn->nxt_tick )
			{
				break;
			}
		}

		/**/
		list_add_before( &(curn->node), &(ptqn->node) );
	}

	/* 是否需要重新设置 hardware timer */
	curn = (tq_node_t *)( pctx->tq_list.next );
	if ( pctx->tq_nxt_tick != curn->nxt_tick )
	{
		/* */
		pctx->tq_nxt_tick = curn->nxt_tick;		
		write( pctx->fd_timer, &(pctx->tq_nxt_tick), sizeof(pctx->tq_nxt_tick) );
	}
	
	/**/
	return 0;
	
}


int  timer_stop( int tid )
{
	tq_node_t * ptqn;
	tq_node_t * curn;
	comn_context * pctx;

	/**/
	pctx = (comn_context *)tls_get();	
	ptqn = (tq_node_t *)(intptr_t)tid;

	/**/
	if ( list_in_list( &(ptqn->node) ) )
	{
		list_delete( &(ptqn->node) );

		/**/
		if ( list_is_empty( &(pctx->tq_list) ) )
		{
			return 0;
		}
		
		/* 是否需要重新设置 hardware timer */
		curn = (tq_node_t *)( pctx->tq_list.next );
		if ( pctx->tq_nxt_tick != curn->nxt_tick )
		{
			/**/
			pctx->tq_nxt_tick = curn->nxt_tick;		
			write( pctx->fd_timer, &(pctx->tq_nxt_tick), sizeof(pctx->tq_nxt_tick) );
		}
		
		/**/
		return 0;
		
	}

	/**/
	if ( ptqn->iflag & IFG_CALLBK )
	{
		ptqn->iflag |= IFG_STOPED;
	}
	
	return 0;
	
}


int  timer_delete( int tid )
{
	tq_node_t * ptqn;

	/**/
	ptqn = (tq_node_t *)(intptr_t)tid;
	
	/**/
	if ( list_in_list( &(ptqn->node) ) )
	{
		timer_stop( tid );
		free( ptqn );

		/**/
		return 0;
	}
	
	/**/
	if ( ptqn->iflag & IFG_CALLBK )
	{
		ptqn->iflag |= IFG_DELETE;
	}
	else
	{
		free( ptqn );
	}
	
	return 0;
	
}




