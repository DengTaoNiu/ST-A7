

#pragma once


#define  __htons(x)    __builtin_bswap16( x )
#define  __ntohs(x)    __builtin_bswap16( x )
#define  __ntohl(x)    __builtin_bswap32( x )
#define  __htonl(x)    __builtin_bswap32( x )



/**/
int  minip_init( int tfd );
int  minip_input( pbuf_t * ipkt );


/**/
void  minip_get_macaddr( uint8_t * padr );
void  minip_set_ipaddr( uint32_t thip, uint32_t mask, uint32_t gtway );




/**/
typedef void (*udp_cbk_t)( int sock, pbuf_t * ipkt, uint32_t srcip, uint16_t srcport, void * parg );

/**/
int  minsock_udp_new( uint16_t srcport );
void  minsock_udp_free( int sock );

int  minsock_udp_bind( int sock, uint16_t srcport );
int  minsock_udp_connect( int sock, uint32_t dstip, uint16_t dstport );
int  minsock_udp_listen( int sock, udp_cbk_t cbf, void * parg );

int  minsock_udp_send_to( int sock, uint32_t dstip, uint16_t dstport, pbuf_t * tpkt );
int  minsock_udp_send( int sock, pbuf_t * tpkt );


typedef struct _tag_sock_t 
{
	struct list_node  node;

	uint16_t  src_port;
	uint16_t  dst_port;
	uint32_t  dst_ip;
	
	udp_cbk_t  cbfunc;
	void * parg;
	
} minip_ip_sock_t;




