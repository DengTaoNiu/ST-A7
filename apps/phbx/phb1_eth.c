
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "dlist.h"
#include "hcb_comn.h"
#include "pkt_api.h"
#include "sys_api.h"
#include "debug.h"
#include "phb1.h"



int  phb_cmd_eth( void * parg, int argc, const char **argv )
{
	int  iret;
	phb_context_t * pctx;
    uint32_t  regofs;
    uint32_t  temp;
	uint32_t  tary[2];
	
	/**/
	pctx = (phb_context_t *)parg;

	if ( argc <= 1 )
	{
		ioctl( pctx->efd, 4, 0, NULL );

		/**/
		goto usage;
	}

	if ( 0 == strcmp( argv[1], "read") )
	{
		if ( argc < 3 )
		{
			goto usage;
		}
		
		iret = debug_str2uint( argv[2], &regofs );
		if ( 0 != iret )
		{
			printf( "ofs arg err\n" );
			goto usage;
		}
		
		/**/
		printf( "try read phy : ofs=%lu\n", regofs );

		/**/
		tary[0] = regofs;
		tary[1] = 0;

		/**/
		iret = ioctl( pctx->efd, 1, sizeof(tary), tary );
		if ( 0 != iret )
		{
			printf( "read fail = %d\n", iret );
			return 0;
		}

		/**/
		printf( "read succ = 0x%x\n", tary[1] );
		return 0;
		
	}

	if ( 0 == strcmp( argv[1], "write") )
	{
		if ( argc < 4 )
		{
			goto usage;
		}
		
		iret = debug_str2uint( argv[2], &regofs );
		if ( 0 != iret )
		{
			printf( "ofs arg err\n" );
			goto usage;
		}
		
		iret = debug_str2uint( argv[3], &temp );
		if ( 0 != iret )
		{
			printf( "val arg err\n" );
			goto usage;
		}

		/**/
		printf( "try write phy : ofs=%lu, val=0x%lx\n", regofs, temp );

		/**/
		tary[0] = regofs;
		tary[1] = temp;

		/**/
		iret = ioctl( pctx->efd, 2, sizeof(tary), tary );
		if ( 0 != iret )
		{
			printf( "write fail = %d\n", iret );
			return 0;
		}

		/**/
		printf( "write succ\n" );
		return 0;		
		
	}

usage:
	printf( "usage: %s [ read <ofs> | write <ofs> <val> ]\n", argv[0] );
	return 0;
}

