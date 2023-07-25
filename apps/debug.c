
#include <stdlib.h>
#include <ctype.h>

#include "compiler.h"
#include "dlist.h"

#include "hcb_comn.h"
#include "arch.h"
#include "sys_api.h"

#include "debug.h"


// Retrieve year info
#define OS_YEAR     ((((__DATE__ [7] - '0') * 10 + (__DATE__ [8] - '0')) * 10 \
                                    + (__DATE__ [9] - '0')) * 10 + (__DATE__ [10] - '0'))

// Retrieve month info
#define OS_MONTH    (__DATE__ [2] == 'n' ? (__DATE__ [1] == 'a' ? 1 : 6) \
                                : __DATE__ [2] == 'b' ? 2 \
                                : __DATE__ [2] == 'r' ? (__DATE__ [0] == 'M' ? 3 : 4) \
                                : __DATE__ [2] == 'y' ? 5 \
                                : __DATE__ [2] == 'l' ? 7 \
                                : __DATE__ [2] == 'g' ? 8 \
                                : __DATE__ [2] == 'p' ? 9 \
                                : __DATE__ [2] == 't' ? 10 \
                                : __DATE__ [2] == 'v' ? 11 : 12)

// Retrieve day info
#define OS_DAY      ((__DATE__ [4] == ' ' ? 0 : __DATE__ [4] - '0') * 10 \
                                + (__DATE__ [5] - '0'))

// Retrieve hour info
#define OS_HOUR     ((__TIME__ [0] - '0') * 10 + (__TIME__ [1] - '0'))

// Retrieve minute info
#define OS_MINUTE   ((__TIME__ [3] - '0') * 10 + (__TIME__ [4] - '0'))

// Retrieve second info
#define OS_SECOND   ((__TIME__ [6] - '0') * 10 + (__TIME__ [7] - '0'))

static char time_str[32]; //= "YYYYMMDDHHMMSSres";

uint8_t *get_build_time(void)
{
    sprintf(time_str, "%02d-%02d-%02d,%02d:%02d", OS_YEAR%100, OS_MONTH, OS_DAY, OS_HOUR, OS_MINUTE);
    return (uint8_t*)time_str;
}



static volatile int  _errno;
volatile int *__aeabi_errno_addr(void)
{
	return &_errno;
}


extern  int  con_history( void );


void  drv_debug_dump_hex( uint8_t * ptr, size_t len )
{
    int  i;
    int  nn;
    int  len2 = len;

    nn = 0;
    while ( (len2 - nn) >= 16 )
    {
	    dprintf(0, "%08lx: ", (uintptr_t)(ptr + nn) );
	    
        for ( i=0; i<16; i++ )
        {
            dprintf(0, "%02x ", ptr[nn + i] );
        }
        
        dprintf(0,"  |  ");

        for ( i=0; i<16; i++ )
        {
            int  c = ptr[nn + i];

            if ( (c < 32) || (c > 127) )
                c = '.';
                
            dprintf(0,"%c", c);
        }

        nn += 16;
        dprintf(0,"\n");
        
    }

    if ( len2 > nn )
    {
	    dprintf(0, "%08lx: ", (uintptr_t)(ptr + nn) );
	    
        for ( i = 0; i < (len2-nn); i++ )
        {
            dprintf(0,"%02x ", ptr[nn + i]);
        }

		/**/
        for ( ; i < 16; i++ )
        {
        	dprintf(0, "   " );
       	}
		
        /**/
        dprintf(0,"  |  ");

        for ( i = 0; i < (len2-nn); i++ )
        {
            int  c = ptr[nn + i];
            
            if (c < 32 || c > 127)
            {
                c = '.';
            }
            
            dprintf(0,"%c", c);
        }

        dprintf(0,"\n");
        
    }

	
	/**/
	return;
    
}
void  debug_dump_hex( uint8_t * ptr, size_t len )
{
    int  i;
    int  nn;
    int  len2 = len;

    nn = 0;
    while ( (len2 - nn) >= 16 )
    {
	    printf( "%08lx: ", (uintptr_t)(ptr + nn) );
	    
        for ( i=0; i<16; i++ )
        {
            printf( "%02x ", ptr[nn + i] );
        }
        
        printf("  |  ");

        for ( i=0; i<16; i++ )
        {
            int  c = ptr[nn + i];

            if ( (c < 32) || (c > 127) )
                c = '.';
                
            printf("%c", c);
        }

        nn += 16;
        printf("\n");
        
    }

    if ( len2 > nn )
    {
	    printf( "%08lx: ", (uintptr_t)(ptr + nn) );
	    
        for ( i = 0; i < (len2-nn); i++ )
        {
            printf("%02x ", ptr[nn + i]);
        }

		/**/
        for ( ; i < 16; i++ )
        {
        	printf( "   " );
       	}
		
        /**/
        printf("  |  ");

        for ( i = 0; i < (len2-nn); i++ )
        {
            int  c = ptr[nn + i];
            
            if (c < 32 || c > 127)
            {
                c = '.';
            }
            
            printf("%c", c);
        }

        printf("\n");
        
    }

	
	/**/
	return;
    
}



static int debug_cmd_hist( void * parg, int argc, const char **argv )
{
	con_history();	
	return 0;
}


static int debug_cmd_help( void * parg, int argc, const char **argv )
{
	comn_context * pcmn;
	cmd_proc_t * p;

	/**/
	pcmn = (comn_context *)tls_get();
	p = pcmn->dbg_cmds;

	/**/
	printf( "All Commands:\n" );
  
	do {
		printf( "  %s\n", p->cmd );
		p = p->next;
	} while ( p );

	return 0;
}


static int debug_cmd_version( void * parg, int argc, const char **argv )
{
	uint32_t  temp;
	
	/**/
	temp = read_reg( BASE_AP_SYSCTL + 8 );
	printf( "\tfpga_ver: %04lx,%04lx\n", (temp >> 16), (temp & 0xffff) );
	printf( "\tsoft_ver: %s,%s\n", __DATE__, __TIME__ );
	return 0;
}




/**/
int  debug_add_cmd( const char * cmd, debug_func func, void * parg )
{
	comn_context * pcmn;
	cmd_proc_t * p;

	/**/
	pcmn = (comn_context *)tls_get();
	p = pcmn->dbg_cmds;

	/* 参数检查. */
	if ( (cmd == NULL) || (func == NULL) )
	{
		return 1;
	}
	
	/* 必须是英文字符开头. */
	if ( 0 == isalpha( (int)(cmd[0]) ) )
	{
		return 2;
	}
		
	/* 是否重复.. */
	while ( p )
	{
		if ( 0 == strcmp( cmd, p->cmd ) )
		{
			return 3;
		}
		
		/**/
		p = p->next;
	}
	
	/* alloc */
	p = (cmd_proc_t *)malloc( sizeof(cmd_proc_t) );
	if ( p == NULL )
	{
		return 4;
	}

	/**/
	p->cmd = cmd;
	p->func = func;
	p->parg = parg;
	p->next = pcmn->dbg_cmds;
	
	/**/
	pcmn->dbg_cmds = p;
	return 0;
	
}



static int  debug_process_cmd( int argc, const char ** argv )
{
	comn_context * pcmn;
	cmd_proc_t * p;

	/**/
	pcmn = (comn_context *)tls_get();
	p = pcmn->dbg_cmds;

	if ( 0 == argc )
	{
		return 0;
	}


	/**/
	while (p) 
	{
		/**/
		if ( 0 == strcmp( argv[0], p->cmd ) )
		{
			return p->func( p->parg, argc, argv);
		}

		/**/
		p = p->next;
	}

	printf( "Unknown command: %s\n", argv[0] );
	return 0;
	
}


int  hcb_debug_main( char * spad )
{
	int iret;
	int  argc;
	const char * argv[10];
	char * p;

	/* split */
	p = spad;
	argc = 0;
	
	while ( 1 )
	{
		if (argc >= 10)
		{
			break;
		}

		/* space, tab */
		while ((*p == 0x20) || (*p == 0x09))
		{
			*p++ = '\0';
		}

		if (*p == '\0')
		{
			break;
		}

		/**/
		argv[argc++] = p;

		/**/
		while ((*p != 0x20) && (*p != 0x09) && (*p != '\0'))
		{
			p++;
		}
	}

	/**/
	debug_process_cmd( argc, argv );
	return 0;
	
}



int  hcb_debug_pts( int tfd )
{
	int  iret;
	uint8_t  temp;
	char  spad[100];


	while ( 1 )
	{
		iret = read( tfd, spad, 99 );
		if ( iret <= 0 )
		{
			break;
		}

		spad[iret] = 0;
		
		/**/
		hcb_debug_main( spad );
	}
	
	return 0;
}



/*
???? 16 ?? ? 10 ??.
*/
int  debug_str2uint( const char * str, uint32_t * pu32 )
{
	char * pend;
	uint64_t  temp;

	/**/
	if ( str[0] == 0x30 )
	{
		if ( str[1] == 0 )
		{
			*pu32 = 0;
			return 0;
		}
		
		if ( (str[1] == 0x58) || (str[1] == 0x78) )
		{
			/**/
			temp = strtoull( str, &pend, 16 );
			if ( pend[0] != 0 )
			{
				return 1;
			}

			/**/
			if ( temp > 0xffffffffull )
			{
				return 2;
			}
		
			/**/
			*pu32 = (uint32_t)temp;
			return 0;
		}
		else
		{
			return 3;
		}
	}

	/**/
	if ( (str[0] >= 0x31) && (str[0] <= 0x39) )
	{
		/**/
		temp = strtoull( str, &pend, 10 );
		if ( pend[0] != 0 )
		{
			return 4;
		}

		/**/
		if ( temp > 0xffffffffull )
		{
			return 5;
		}
		
		/**/
		*pu32 = (uint32_t)temp;		
		return 0;
	}

	/**/
	return 6;
}





int  hcb_debug_init( void )
{
	comn_context * pctx;

	/**/
	pctx = (comn_context *)tls_get();	
	pctx->dbg_cmds = NULL;
	debug_add_cmd( "help", debug_cmd_help,NULL );
	
	/**/
	return 0;
}


void  soc_reboot()
{
     uint32_t reg_bak5;
     uint32_t reg_bak6;
     uint32_t reg_bak7;
     uint32_t reg_bak8;
     uint32_t reg_bak9;
     uint32_t reg_bak10;
    
     //bakeup relative registers
     reg_bak5 = read_reg(BASE_AP_GPIO0 + GPIOC_MODE);
     reg_bak6 = read_reg(BASE_AP_GPIO0 + GPIOC_DQE);
     reg_bak7 = read_reg(BASE_AP_GPIO6 + GPIOC_MODE);
     reg_bak8 = read_reg(BASE_AP_GPIO6 + GPIOC_DQE);
     reg_bak9 = read_reg(BASE_AP_GPIO7 + GPIOC_MODE);
     reg_bak10 = read_reg(BASE_AP_GPIO7 + GPIOC_DQE);
    
     //set pin mode mux register
     write_reg( 0x558000 + MUX_UART0_TX, 0x6 );//set to GPIO2 mode and enable internal pullup  PAD_UART0_TX_CFG
     write_reg( 0x558000 + MUX_UART1_TX, 0x6 );//set to GPIO4 mode and enable internal pullup  PAD_UART1_TX_CFG
     write_reg( 0x558000 + MUX_SPI0_TX, 0x6 );//set to GPIO53 mode and enable internal pullup  PAD_SPI0_TX_CFG
     write_reg( 0x558000 + MUX_SPI2_TX, 0x6 );//set to GPIO56 mode and enable internal pullup  PAD_SPI2_TX_CFG
    
     write_reg( BASE_AP_GPIO0 + GPIOC_MODE, (reg_bak5 & 0xEB) );//set to GPIO2 & GPIO4 input mode
     write_reg( BASE_AP_GPIO0 + GPIOC_DQE, (reg_bak6 & 0xEB) );//disable GPIO2 & GPIO4 output
     write_reg( BASE_AP_GPIO6 + GPIOC_MODE, (reg_bak7 & 0xDF) );//set to GPIO53 input mode
     write_reg( BASE_AP_GPIO6 + GPIOC_DQE, (reg_bak8 & 0xDF) );//disable GPIO53 output
     write_reg( BASE_AP_GPIO7 + GPIOC_MODE, (reg_bak9 & 0xFE) );//set to GPIO56 input mode
     write_reg( BASE_AP_GPIO7 + GPIOC_DQE, (reg_bak10 & 0xFE) );//disable GPIO56 output
	 write_reg((BASE_BB_DUALRAM + 8184), 0xA798EBE7);
    reboot(0);
}


#if 0

	/**/
	debug_add_cmd( "help", debug_cmd_help, 		NULL );
	debug_add_cmd( "hist", debug_cmd_hist, 		NULL );	
	debug_add_cmd( "ver",  debug_cmd_version, 	NULL );
	
	debug_add_cmd( "dump",  debug_cmd_dump,  	NULL );
	debug_add_cmd( "rreg", 	debug_cmd_rreg,		NULL );
	debug_add_cmd( "wreg", 	debug_cmd_wreg,		NULL );

	/**/
	return 0;
#endif




