

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "dlist.h"
#include "hcb_comn.h"
#include "pkt_api.h"
#include "sys_api.h"
#include "debug.h"
#include "phb1.h"
// NEW:
#include "../../drvs/drv_can.h"

void test_dump_dpdata_can(phb_context_t *pctx)
{
	int iret;
	pbuf_t *ipkt;
	uint8_t *ptr;

	/**/
	while (1)
	{
		iret = recv(pctx->dfd, &ipkt);
		if (iret <= 0)
		{
			break;
		}

// NEW:
#if 0
        printf( "Autbus recv:\n" );
        debug_dump_hex( pkt_to_addr(ipkt), ipkt->length );
#endif
		/**/
		iret = send(pctx->cfd[0], ipkt);
		if (iret != 1)
		{
			/**/
			pkt_free(ipkt);
		}
	}

	/**/
	return;
}

int test_can_recv(int tfd, pbuf_t **ppkt)
{
	int i;
	int iret;
	pbuf_t *pkt;

	/**/
	iret = recv(tfd, &pkt);
	if (iret != 1)
	{
		return iret;
	}

	/**/
	if (pkt->length < 4)
	{
		pkt_free(pkt);
		return 0;
	}

	*ppkt = pkt;
	return 1;
}

int phb_cmd_reset_can(void *parg, int argc, const char **argv)
{
	int i;
	int j;
	i = 0;
	write_reg(0x490004, 0);
	while (i < 100000)
	{
		++i;
		++j;
		++j;
		++j;
		++j;
		++j;
	}
	write_reg(0x490004, 0x10);
	return 0;
}
int phb_cmd_send_can(void *parg, int argc, const char **argv)
{
	int iret;
	uint32_t temp;
	uint32_t dat[2];
	int i, j;
	// num of can frames
	int cfnum;
	uint8_t *pdat;
	pbuf_t *ipkt;
	phb_context_t *pctx;
	struct can_frame cf;

	/**/
	pctx = (phb_context_t *)parg;
	/**/
	if (argc < 2)
	{
		goto usage;
	}
	/**/
	if (0 == strcmp(argv[1], "write"))
	{
		/**/
		if (argc < 4)
		{
			goto usage;
		}
		iret = debug_str2uint(argv[2], &dat[0]);
		if (iret != 0)
		{
			printf("addr fmt err\n");
			goto usage;
		}
		iret = debug_str2uint(argv[3], &dat[1]);
		if (iret != 0)
		{
			printf("dat fmt err\n");
			goto usage;
		}
		ioctl(pctx->cfd[1], 3, 2, dat);

		return 0;
	}
	else if (0 == strcmp(argv[1], "read"))
	{
		/**/
		if (argc < 4)
		{
			goto usage;
		}

		iret = debug_str2uint(argv[2], &dat[0]);
		if (iret != 0)
		{
			printf("addr fmt err\n");
			goto usage;
		}

		iret = debug_str2uint(argv[3], &dat[1]);
		if (iret != 0)
		{
			printf("cnt fmt err\n");
			goto usage;
		}

		ioctl(pctx->cfd[1], 2, 2, dat);

		return 0;
	}
	else if (0 == strcmp(argv[1], "reset"))
	{

		ioctl(pctx->cfd[1], 4, 0, dat);
		return 0;
	}
	else
		goto usage;

usage:
	printf("usage: %s [ read <addr> <cnt> | write <addr>  <val>| reset]\n",
		   argv[0]);
	return 0;
}

int phb_cmd_dbg_can(void *parg, int argc, const char **argv)
{
	int iret;
	uint32_t temp;
	uint8_t cnt;
	uint16_t addr;

	phb_context_t *pctx;

	uint32_t sbuf[20];

	/**/
	pctx = (phb_context_t *)parg;

	/**/
	if (argc < 3)
	{
		iret = debug_str2uint(argv[1], &temp);
		if (iret != 0)
		{
			printf("req, fmt err\n");
			goto usage;
		}
		iret = ioctl(pctx->cfd[1], temp, 0, sbuf);
		printf("read debug %d\n", iret);
		return 0;
	}

	iret = debug_str2uint(argv[1], &temp);
	if (iret != 0)
	{
		printf("addr, fmt err\n");
		goto usage;
	}
	addr = temp & 0xffff;
	iret = debug_str2uint(argv[2], &temp);
	if (iret != 0)
	{
		printf("cnt, fmt err\n");
		goto usage;
	}
	cnt = temp & 0xff;
	if (argc > 3)
	{
		iret = debug_str2uint(argv[3], &temp);
		if (iret != 0)
		{
			printf("data, fmt err\n");
			goto usage;
		}
		sbuf[0] = addr;
		sbuf[1] = cnt;
		sbuf[2] = temp;

		iret = ioctl(pctx->cfd[1], 3, 3, sbuf);
		printf("write debug %d\n", iret);
	}
	else
	{
		sbuf[0] = addr;
		sbuf[1] = cnt;
		iret = ioctl(pctx->cfd[1], 2, 2, sbuf);
		printf("read debug %d\n", iret);
	}

	return 0;
usage:
	printf("usage: %s  <addr>  <cnt> <data>", argv[0]);
	return 0;
}

#if 0
void  test_dump_can( int tfd )
{
	int  iret;
	uint32_t  tcnt;
	pbuf_t * ipkt;

	/**/
	while (1)
	{
		iret = can_recv( tfd, &ipkt );
		if ( iret < 0 )
		{
			break;
		}

		/**/
		printf( "%d frames :id(4) | dlc(1) resrv(3) | data(8)[bytes]\n", ipkt->length / 16  );
        if ( ipkt->length > 64 )
        {
		    debug_dump_hex( pkt_to_addr(ipkt), 64 );
        }
        else
        {
            debug_dump_hex( pkt_to_addr(ipkt), ipkt->length );
        }

        /**/
        pkt_free( ipkt );
    }
}
#endif

// void test_dump_can(phb_context_t *pctx, int fd)
// {
// 	int iret;
// 	// uint32_t  tcnt;
// 	pbuf_t *ipkt;

// 	while (1)
// 	{
// 		iret = can_recv(fd, &ipkt);
// 		if (iret < 0)
// 		{
// 			break;
// 		}

// #if 1
// 		printf("CAN recv:\n");
// 		debug_dump_hex(pkt_to_addr(ipkt), ipkt->length);
// #endif
// 		pkt_free(ipkt);

// #if 0
// 		iret = send(pctx->dfd, ipkt);
// 		if (iret != 1)
// 		{
// 			pkt_free(ipkt);
// 		}
// #endif
// 	}
// }
