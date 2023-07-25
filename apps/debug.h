

#ifndef  __DeBuG_API_H__
#define  __DeBuG_API_H__
#include "dlist.h"

/**/
extern  int  uheap_init( void );
extern  void  uheap_info( size_t * pused,  size_t * pfree );

extern  void * malloc( size_t size );
extern  void free( void *ptr );

extern  int  printf( const char * fmt, ... );
extern  int  sprintf( char * str, const char *fmt, ... );


typedef int (*debug_func)( void * pctx, int argc, const char **argv );


typedef struct _tag_cmd_proc {
	const char * cmd;
	debug_func  func;
	void * parg;
	
	struct _tag_cmd_proc * next;
	
} cmd_proc_t;

int  debug_add_cmd( const char * cmd, debug_func func, void * parg );

/**/
int  debug_str2uint( const char * str, uint32_t * pu32 );
void  debug_dump_hex( uint8_t * ptr, size_t len );
void  drv_debug_dump_hex( uint8_t * ptr, size_t len );

/**/
int  hcb_debug_init( void );
int  hcb_debug_main( char * spad );
int  hcb_debug_pts( int tfd );


typedef struct  _tag_comn_context
{
	int  fd_stdio;
	int  fd_timer;

	/* debug */
	cmd_proc_t * dbg_cmds;
	
	/* timer */
	struct list_node  tq_list;
	uint64_t  tq_nxt_tick;

} comn_context;




#define LOG_INFO 1
#define LOG_DBG 1
#define LOG_ERR 1

#define LOG_DRV(loglevel, ...)                       \
    do                                               \
    {                                                \
        if (loglevel)                                \
        {                                            \
            dprintf(0,"%s,%d\t: ", __FILE__, __LINE__,0); \
            dprintf(0,__VA_ARGS__,0);                     \
        }                                            \
    } while (0);
uint8_t *get_build_time(void);
#endif

void  soc_reboot();



