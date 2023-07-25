
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "dlist.h"
#include "hcb_comn.h"
#include "pkt_api.h"
#include "sys_api.h"
#include "debug.h"
#include "atimer.h"
#include "phb1.h"


uint16_t adc_buf[256];
void phb_cmd_adc(phb_context_t *pctx, int argc, const char **argv)
{
	int iret, i;
	uint8_t tary[1];
	 uint32_t  temp;
	pbuf_t *ipkt;
	if(argc == 1)
	{
		printf("CMD_ERR_PARAMS_FORMAT\n");
		return;
	}
	if(!strcmp("read", argv[1]))
	{
		if(argc != 2)
		{
			printf("CMD_ERR_PARAMS_FORMAT\n");
			return;
		}
		read(pctx->afd,adc_buf,256);
		for(i = 0;i<256;i++)
		{
			printf("0x%x  ",adc_buf[i]);
			if(0 == (i&0xf))
				printf("\n");
		}
	}
	if(!strcmp("switch", argv[1]))
	{
		if(argc != 3)
		{
			printf("CMD_ERR_PARAMS_FORMAT\n");
			return;
		}
		iret = debug_str2uint( argv[2], &temp );
		if ( iret != 0 )
		{
			printf("CMD_ERR_PARAMS_FORMAT\n");
			return;
		}
		temp = temp&0xFF;
		tary[0] = temp;
		ioctl( pctx->afd, 0, sizeof(tary), tary );
		printf("adc switch to channel %d",tary[0]);
	}
	
}


void  test_dump_adc( phb_context_t * pctx)
{
    int iret;
    uint16_t  rbuf[128];

	
	iret = read( pctx->afd,rbuf,128);
	if ( iret <= 0 )
	{
		return;
	}
	
        //printf("adc  data:\n");
        //debug_dump_hex(rbuf, 128);
    
    
    /**/
    return;
}



