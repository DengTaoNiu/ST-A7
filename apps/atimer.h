
#pragma once

#include "dlist.h"
typedef void (*timer_cbk_t)( int tid, void * parg );


int  timer_queue_event( void );
int  timer_queue_init( void );


int  timer_create( timer_cbk_t cbkf, void * parg );
int  timer_delete( int tid );

int  timer_start( int tid, uint32_t itv, uint32_t irt );
int  timer_stop( int tid );


typedef struct _tag_tq_node
{
	/**/
	struct list_node  node;

	/**/
	uint32_t  iflag;
	uint32_t  irepeat;
	uint64_t  nxt_tick;
	
	/**/
	timer_cbk_t cbkf;
	void * parg;
	
} tq_node_t;
static inline bool  atimer_is_waiting( tq_node_t *  ptmr )
{
    return list_in_list( &ptmr->node );
}





