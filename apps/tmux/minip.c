
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "sys_api.h"
#include "dlist.h"
#include "bitmap.h"
#include "hcb_comn.h"
#include "debug.h"

#include "minip.h"







#pragma pack(push, 1)


struct udp_hdr {

	uint16_t src_port;
	uint16_t dst_port;
	uint16_t len;
	uint16_t chksum;
	
};


struct ipv4_hdr {
    uint8_t  ver_ihl;
    uint8_t  dscp_ecn;
    uint16_t len;
    uint16_t id;
    uint16_t flags_frags;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t chksum;
    uint32_t src_addr;
    uint32_t dst_addr;
    uint8_t  data[];
};


struct eth_hdr {
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t type;
};


struct icmp_pkt {
    uint8_t  type;
    uint8_t  code;
    uint16_t chksum;
    uint8_t  hdr_data[4];
    uint8_t  data[];
};


struct arp_pkt {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sha[6];
    uint32_t spa;
    uint8_t  tha[6];
    uint32_t tpa;
};


#pragma pack(pop)

enum {
    ICMP_ECHO_REPLY   = 0,
    ICMP_ECHO_REQUEST = 8,
};

enum {
    IP_PROTO_ICMP = 0x1,
    IP_PROTO_TCP  = 0x6,
    IP_PROTO_UDP  = 0x11,
};

enum {
    ETH_TYPE_IPV4 = 0x0800,
    ETH_TYPE_ARP  = 0x0806,
};

enum {
    ARP_OPER_REQUEST = 0x0001,
    ARP_OPER_REPLY   = 0x0002,
};



#define IPV4(a,b,c,d) (((a)&0xFF)|(((b)&0xFF)<<8)|(((c)&0xFF)<<16)|(((d)&0xFF)<<24))
#define IPV4_BCAST (0xFFFFFFFF)
#define IPV4_NONE (0)


/* IP 地址字节序, 例如 192.168.100.1 -->  0xC0, 0xA8, 0x64, 0x01 */
static int  eth_tfd = -1;
static uint32_t  inetadr = IPV4(192,168,100,2);
static uint32_t  netmask = IPV4(255,255,255,0);
static uint32_t  gateway = IPV4(192,168,100,1);

static uint8_t 	minip_mac[6] = { 0x00, 0x27, 0x13, 0xCC, 0xCC, 0xCC };
static const uint8_t broadcast_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };


/* todo : bitmap, udp src port, resource manage.. */
static bitmap_t * pbmp_ures = NULL;


/*
static uint32_t minip_netmask = IPV4_NONE;
static uint32_t minip_broadcast = IPV4_BCAST;
static uint32_t minip_gateway = IPV4_NONE;
*/

uint16_t  rfc1701_chksum( const uint8_t *buf, size_t len )
{
    uint32_t total = 0;
    uint16_t chksum = 0;
    const uint16_t *p = (const uint16_t *) buf;

    // Length is in bytes
    for (size_t i = 0; i < len / 2; i++ ) {
        total += p[i];
    }

    chksum = (total & 0xFFFF) + (total >> 16);
    chksum = ~chksum;

    return chksum;
}



static inline void mac_addr_copy(uint8_t *dest, const uint8_t *src)
{
	int  i;

	for ( i=0; i<6; i++ )
	{
		dest[i] = src[i];
	}

#if  0
    *(uint32_t *)dest = *(const uint32_t *)src;
    *(uint16_t *)(dest + 4) = *(const uint16_t *)(src + 4);
#endif

}



static inline int  minip_build_mac_hdr( pbuf_t * tpkt, const uint8_t *dst, uint16_t type )
{
	struct eth_hdr * ehdr;
	
	/**/
	ehdr = (struct eth_hdr *)pkt_prepend( tpkt, sizeof(struct eth_hdr) );
	if ( ehdr == NULL )
	{
		return -1;
	}
	
	/**/
    mac_addr_copy(ehdr->dst_mac, dst);
    mac_addr_copy(ehdr->src_mac, minip_mac);
    ehdr->type = __htons(type);

	/**/
    return 0;
}



static inline  int  minip_build_ipv4_hdr( pbuf_t * tpkt, uint32_t dst, uint8_t proto )
{
	struct ipv4_hdr * ipv4;
	uint16_t  tlen;

	/**/
	tlen = tpkt->length;
	ipv4 = (struct ipv4_hdr *)pkt_prepend( tpkt, sizeof(struct ipv4_hdr) );
	if ( ipv4 == NULL )
	{
		return -1;
	}
	
    ipv4->ver_ihl       = 0x45;
    ipv4->dscp_ecn      = 0;
    ipv4->len           = __htons(20 + tlen); 	// 5 * 4 from ihl, plus payload length
    ipv4->id            = 0;
    ipv4->flags_frags   = 0;					// no offset, no fragments
    ipv4->ttl           = 64;
    ipv4->proto         = proto;
    ipv4->dst_addr      = dst;
    ipv4->src_addr      = inetadr;

    /* This may be unnecessary if the controller supports checksum offloading */
    // ipv4->chksum = rfc1701_chksum((uint8_t *) ipv4, sizeof(struct ipv4_hdr));
    ipv4->chksum = 0;

    /**/
    return 0;
    
}


static inline int  minip_build_udp_hdr( pbuf_t * tpkt, uint16_t srcport, uint16_t dstport )
{
	struct udp_hdr * pudp;
	uint16_t  tlen;

	/**/
	tlen = tpkt->length;

	/**/
	pudp = (struct udp_hdr *)pkt_prepend( tpkt, sizeof(struct udp_hdr) );
	if ( pudp == NULL )
	{
		return -1;
	}
	
	/* ignore checksum */
	pudp->src_port = srcport;
	pudp->dst_port = dstport;
	pudp->len = __htons( 8 + tlen );
	pudp->chksum = 0;
	return 0;
	
}



int  minip_send_eth( pbuf_t * tpkt,  const uint8_t * dmac, uint16_t type )
{
	int  iret;
	
	/**/
	iret = minip_build_mac_hdr( tpkt, dmac, type );
	if ( iret != 0 )
	{
		return -4;
	}
	
	/**/
	iret = send( eth_tfd, tpkt );
	if ( iret != 1 )
	{
		pkt_free( tpkt );
		return -5;
	}

	/**/
	return 0;
}


int  minip_send_arp( pbuf_t * tpkt,  uint32_t dstip )
{
	struct arp_pkt * rarp;
	
	/* reset to origin */
	tpkt->offset = 32;
	tpkt->length = 0;

	/* ARP packet */
	rarp = (struct arp_pkt *)pkt_append( tpkt, sizeof(struct arp_pkt) );
	rarp->oper = __htons(ARP_OPER_REQUEST);
	rarp->htype = __htons(0x0001);
	rarp->ptype = __htons(0x0800);
	rarp->hlen = 6;
	rarp->plen = 4;
	
	mac_addr_copy(rarp->sha, minip_mac);
	rarp->spa = inetadr;
	
	memset(rarp->tha, 0, 6 );
	rarp->tpa = dstip;
	
	/**/
	minip_send_eth( tpkt, broadcast_mac, ETH_TYPE_ARP );	
	return 0;
	
}



typedef struct _tag_arp_entry {
	
	struct list_node  node;
	
	uint32_t  ipadr;
	uint16_t  flag;

	union {
		pbuf_t * tpkt;
		uint8_t	 mac[6];
	} info;
	
} arp_entry;


/**/
#define  AFG_VALID  0x2
#define  AFG_INTRY  0x1


/**/
struct list_node  arp_list;
static arp_entry  tmp_arp;


int  minip_arp_init( int num )
{
	int  i;
	arp_entry * parp;

	/**/
	parp = (arp_entry *)malloc( sizeof(arp_entry) * num );
	if ( parp == NULL )
	{
		return 1;
	}

	/**/
	memset( parp, 0 , ( sizeof(arp_entry) * num ) );
	list_initialize( &arp_list );
	
	/**/
	for ( i=0; i<num; i++ )
	{
		/**/
		list_add_tail( &arp_list, &(parp[i].node) );
	}

	return 0;
	
}


/**/
void  minip_arp_update( uint32_t addr, const uint8_t * pmac )
{
	arp_entry * parp;
	
	/**/
	list_for_every_entry( &arp_list, parp, arp_entry, node )
	{
		if ( parp->ipadr == addr )
		{
			/**/
			if ( parp->flag == AFG_INTRY )
			{
				minip_send_eth( parp->info.tpkt, pmac, ETH_TYPE_IPV4 );
			}
			
			/**/
			mac_addr_copy( parp->info.mac, pmac );
			parp->flag = AFG_VALID;
			
			/* move to list head */
			list_delete( &(parp->node) );
			list_add_head( &arp_list, &(parp->node) );
			break;
		}
	}

	/**/
	return;
	
}


arp_entry * minip_arp_lookup( uint32_t addr )
{
	arp_entry * parp;

	/* group or broadcast addr */
	if ( addr == 0xffffffff )
	{
		/* fake arp entry */
		tmp_arp.flag = AFG_VALID;
		memset( tmp_arp.info.mac, 0xff, 6 );
		return &tmp_arp;
	}

	/**/
	list_for_every_entry( &arp_list, parp, arp_entry, node )
	{
		if ( parp->ipadr == addr )
		{
			return parp;
		}
	}
	
	/**/
	return NULL;
	
}



static int minip_arp_display( void * parg, int argc, const char **argv )
{
	arp_entry * parp;

	/**/
	list_for_every_entry( &arp_list, parp, arp_entry, node )
	{
		printf( "f=%u, ip=%08x, ", parp->flag, parp->ipadr );
		printf( "mac=%02x:%02x:%02x-%02x:%02x:%02x\n", parp->info.mac[0], parp->info.mac[1], parp->info.mac[2], 
				parp->info.mac[3], parp->info.mac[4], parp->info.mac[5] );
	}
	
	return 0;
}



int  minip_send_iface( pbuf_t * tpkt, uint32_t dstip )
{
	arp_entry * parp;
	pbuf_t * rpkt;
	uint32_t  temp;
	
	/*  如果不是同一个网段, 找网关的 arp entry. */
	temp = inetadr & netmask;
	if ( (dstip & netmask) != temp )
	{
		dstip = gateway;
	}
	
	/* lookup arp table */
	parp = minip_arp_lookup( dstip );
	
	/**/
	if ( parp != NULL )
	{
		if ( parp->flag == AFG_VALID )
		{
			return minip_send_eth( tpkt, parp->info.mac, ETH_TYPE_IPV4 );
		}
		
		/* already retry state */
		if ( parp->flag == AFG_INTRY )
		{
			/* drop the last, send arp again */
			rpkt = parp->info.tpkt;
		}
		else
		{
			/* 虽然是个 error 情况, 也可以往下走.. */
			rpkt = pkt_alloc( 32 );
			if ( rpkt == NULL )
			{
				parp->flag = 0;			
				pkt_free( tpkt );
				return 1;
			}
		}
		
	}
	else
	{
		/* 用链表最后一个位置.. */
		parp = (arp_entry *)list_remove_tail( &arp_list );
		list_add_head( &arp_list, &(parp->node) );
		
		/* 如果抢占的位置, 原来是 TRY 状态, 使用他的 pkt 来发送 arp request. */
		if ( parp->flag == AFG_INTRY )
		{
			rpkt = parp->info.tpkt;
		}
		else
		{
			rpkt = pkt_alloc( 32 );
			if ( rpkt == NULL )
			{
				parp->flag = 0;
				pkt_free( tpkt );
				return 1;
			}
		}
		
	}

	/**/
	parp->ipadr = dstip;
	parp->flag = AFG_INTRY;
	parp->info.tpkt = tpkt;
	
	/**/	
	minip_send_arp( rpkt, dstip );
	return -100;

}


int  minip_send_ipv4( pbuf_t * tpkt, uint32_t dstip, uint8_t proto )
{
	int  iret;
	
	/**/
	iret = minip_build_ipv4_hdr( tpkt, dstip, proto );
	if ( iret != 0 )
	{
		return -2;
	}

	/**/
	return minip_send_iface( tpkt, dstip );
}



/* listen udp sock, dlist  */
struct list_node  udp_listen_list;

int  minip_handle_udp( uint32_t srcip, pbuf_t * ipkt )
{
	int  i;
	int  tlen;
	minip_ip_sock_t * psock;
	minip_ip_sock_t * ptemp;
	struct udp_hdr * pudp;
	
	/**/
	pudp = (struct udp_hdr *)pkt_to_addr( ipkt );
	tlen = __ntohs( pudp->len );
	if ( tlen > ipkt->length )
	{
		return 1;
	}

	/* trim pading */
	pkt_trim( ipkt, (ipkt->length - tlen) );
	
	/**/
	list_for_every_entry_safe( &udp_listen_list, psock, ptemp, minip_ip_sock_t, node )
	{
		/**/
		if ( pudp->dst_port != psock->src_port )
		{
			continue;
		}

		/**/
		if ( (psock->dst_ip != 0) && (psock->dst_ip != srcip) )
		{
			continue;
		}

		/**/
		if ( (psock->dst_port != 0) && (psock->dst_port != pudp->src_port) )
		{
			continue;
		}

		/**/
		if ( NULL != psock->cbfunc )
		{
			pkt_clip( ipkt, sizeof(struct udp_hdr) );
			psock->cbfunc( (int)(intptr_t)psock, ipkt, __ntohl(srcip), __ntohs(pudp->src_port), psock->parg );
		}
	}
	
	return 0;
	
}



int  minip_handle_icmp( uint32_t src, pbuf_t * ipkt )
{
	int  iret;
	pbuf_t * pkt;
	struct icmp_pkt * icmp;
	struct icmp_pkt * ricmp;

	/**/
	if ( ipkt->length < sizeof(struct icmp_pkt) )
	{
		return 1;
	}

	/**/
	icmp = (struct icmp_pkt *)pkt_to_addr( ipkt );

	if ( icmp->type != ICMP_ECHO_REQUEST )
	{
		return 0;
	}

	/* alloc */
	pkt = pkt_alloc( sizeof(struct eth_hdr) + sizeof(struct ipv4_hdr) );
	if ( pkt == NULL )
	{
		return 2;
	}

	/**/
	ricmp = (struct icmp_pkt *)pkt_append( pkt, ipkt->length );

	/**/
    memcpy( ricmp, icmp, ipkt->length );
    ricmp->type = ICMP_ECHO_REPLY;
    ricmp->code = 0;
    ricmp->chksum = 0;
    ricmp->chksum = rfc1701_chksum( (uint8_t *)ricmp, ipkt->length );

	/**/
	iret = minip_send_ipv4( pkt, src, IP_PROTO_ICMP );
	if ( iret != 0 )
	{
		return (iret << 4) + 3;
	}

	/**/
	return 0;
	
}


static inline int  minip_handle_ipv4( uint8_t * smac, pbuf_t * ipkt )
{
    struct ipv4_hdr * ip;
	size_t  hlen;
	uint16_t  frags;
	
	/**/
	ip = (struct ipv4_hdr *)pkt_to_addr( ipkt );

	/**/
	if ( ipkt->length < sizeof(struct ipv4_hdr) )
	{
		return 1;
	}

    /* reject bad packets */
    if ( ((ip->ver_ihl >> 4) & 0xf) != 4 ) 
	{
        /* not version 4 */
        return 2;
    }

    /* do we have enough buffer to hold the full header + options? */
    hlen = (ip->ver_ihl & 0xf) * 4;
	if ( ipkt->length < ( sizeof(struct eth_hdr) + hlen) )
	{
        return 3;
    }

    /* frags is not support: MF(1bit), Offset(13bits) */
    frags = __ntohs( ip->flags_frags );
    if ( (frags & 0x3FFF) != 0 )
    {
    	return 4;
    }

#if 0	
	/* 如果 ip 是 0 , 可以让任意 目的 ip 进来, DHCP?? */
	if ( (inetadr != 0 ) && (ip->dst_addr != inetadr) )
	{
		return 5;
	}
	
    /* the packet is good, we can use it to populate our arp cache */
    minip_arp_update( ip->src_addr, smac );
#endif

    /* We only handle UDP and ECHO REQUEST */
    pkt_clip( ipkt, hlen );
	
	/**/
    switch (ip->proto) 
	{
    case IP_PROTO_ICMP: 

		/* 如果没有配置 ip 地址, 那么不处理 icmp ping */
    	if ( inetadr != 0 )
    	{
			if ( ip->dst_addr == inetadr )
			{
	        	minip_handle_icmp( ip->src_addr, ipkt );
	        }
		}
        break;
		
    case IP_PROTO_UDP:
    
    	if ( (inetadr == 0) || (ip->dst_addr == inetadr) )
    	{
        	minip_handle_udp( ip->src_addr, ipkt );
        }
        
        break;
		
	default:
		break;
		
	}
	
	return 0;
	
}


static inline int  minip_handle_arp( uint8_t * smac, pbuf_t * ipkt )
{
	pbuf_t * tpkt;
	struct arp_pkt * iarp;
	struct arp_pkt * rarp;

	/**/
	iarp = (struct arp_pkt *)pkt_to_addr( ipkt );

	/**/
	switch ( __ntohs( iarp->oper ) )
	{
	case ARP_OPER_REQUEST:
		if ( memcmp( &iarp->tpa, &inetadr, sizeof(inetadr)) != 0 )
		{
			break;
		}
		
		/* alloc */
		tpkt = pkt_alloc( sizeof(struct eth_hdr) );
		if ( tpkt == NULL )
		{
			break;
		}

		/* ARP packet */
		rarp = (struct arp_pkt *)pkt_append( tpkt, sizeof(struct arp_pkt) );
		rarp->oper = __htons(ARP_OPER_REPLY);
		rarp->htype = __htons(0x0001);
		rarp->ptype = __htons(0x0800);
		rarp->hlen = 6;
		rarp->plen = 4;
		mac_addr_copy(rarp->sha, minip_mac);
		rarp->spa = inetadr;
		mac_addr_copy(rarp->tha, iarp->sha);
		rarp->tpa = iarp->spa;
		
		/* Eth header */
		minip_send_eth( tpkt, smac, ETH_TYPE_ARP );
		break;
		
	case ARP_OPER_REPLY:
		if ( memcmp( &iarp->tpa, &inetadr, sizeof(inetadr)) != 0 )
		{
			break;
		}
		
		minip_arp_update( iarp->spa, iarp->sha );
		break;
		
	default:
		/* nothing */
		break;
		
	}
	
	/**/
	return 0;
	
}


int  minip_input( pbuf_t * ipkt )
{
	struct eth_hdr * peth;
	uint16_t  etype;
	
	/**/
	peth = (struct eth_hdr *)pkt_to_addr( ipkt );
	etype = __ntohs( peth->type );
	/**/
    if ( 0 == memcmp(peth->dst_mac, minip_mac, 6) )
	{        
		if ( etype == ETH_TYPE_IPV4 )
		{
			/**/
			pkt_clip( ipkt, sizeof(struct eth_hdr) );
			minip_handle_ipv4( peth->src_mac, ipkt );
		}
		
		if ( etype == ETH_TYPE_ARP )
		{
			/**/
			pkt_clip( ipkt, sizeof(struct eth_hdr) );
			minip_handle_arp( peth->src_mac, ipkt );
		}
		
	}
	
	
	if ( 0 == memcmp(peth->dst_mac, broadcast_mac, 6) ) 
	{	
		if ( etype == ETH_TYPE_ARP )
		{
			/**/
			pkt_clip( ipkt, sizeof(struct eth_hdr) );
			minip_handle_arp( peth->src_mac, ipkt );
		}
		
	}
	
	/* drop, free */
	pkt_free( ipkt );
	return 0;
	
}

/* port range : 20000 - 20127 */
static int  minip_port_alloc( uint16_t * ppt )
{
	uint32_t  temp;

	if ( false == bitmap_ffs( pbmp_ures, &temp ) )
	{
		return -1;
	}

	/**/
	bitmap_clr( pbmp_ures, temp );
	*ppt = (uint16_t)( temp + 20000 );
	return 0;
}


static int  minip_port_free( uint16_t spt )
{
	uint32_t  temp;

	/**/
	if ( spt < 20000 )
	{
		return 1;
	}

	/**/
	temp = spt - 20000;
	bitmap_set( pbmp_ures, temp );
	return 0;
}



static int minip_arp_ipconfig( void * parg, int argc, const char **argv )
{
	int  iret;
	uint32_t  temp;

	/**/
	if ( argc < 2 )
	{
		/**/
		printf( "\t   mac: %x:%x:%x:%x:%x:%x\n", minip_mac[0], minip_mac[1], minip_mac[2], minip_mac[3], minip_mac[4], minip_mac[5] );

		/**/
		temp = __ntohl( inetadr );
		printf( "\t    ip: %u.%u.%u.%u\n", (temp >> 24) & 0xff,  (temp >> 16) & 0xff, (temp >> 8) & 0xff, temp & 0xff );

		temp = __ntohl( netmask );
		printf( "\t  mask: %u.%u.%u.%u\n", (temp >> 24) & 0xff,  (temp >> 16) & 0xff, (temp >> 8) & 0xff, temp & 0xff );

		temp = __ntohl( gateway );
		printf( "\t gtway: %u.%u.%u.%u\n", (temp >> 24) & 0xff,  (temp >> 16) & 0xff, (temp >> 8) & 0xff, temp & 0xff );
		
		/**/
		goto usage;
	}

	/**/
	iret = debug_str2uint( argv[1], &temp );
	if ( iret != 0 )
	{
		printf( "id arg fmt err\n" );
		goto usage;
	}
	
	/* change IP addr, change MAC addr */
	inetadr = IPV4( 192, 168, 100, (temp & 0xFF) );
	minip_mac[5] = temp & 0xFF;
	return 0;
	
usage:
	printf( "%s [id]\n", argv[0] );
	return 0;
	
}


/**/
int  minip_init( int tfd )
{
	int  iret;
	
	/* local ip, local mac */
	eth_tfd = tfd;
	ioctl( tfd, 0x10, 6, minip_mac );
	ioctl( tfd, 0x11, 4, &inetadr );

	/**/
	list_initialize( &udp_listen_list );

	/* arp entry num = 8 */
	iret = minip_arp_init( 8 );
	if ( iret != 0 )
	{
		return iret;
	}

	/* 4 uint32_t,  128bits,  128 port */
	pbmp_ures = malloc( sizeof(bitmap_t)  );
	if ( pbmp_ures == NULL )
	{
		return 4;
	}

	/**/
	pbmp_ures->tsiz = 128;
	pbmp_ures->data[0] = 0xFFFFFFFF;
	pbmp_ures->data[1] = 0xFFFFFFFF;
	pbmp_ures->data[2] = 0xFFFFFFFF;
	pbmp_ures->data[3] = 0xFFFFFFFF;

	/**/
	debug_add_cmd( "ipcfg", minip_arp_ipconfig, NULL );	
	debug_add_cmd( "arp", minip_arp_display, NULL );
	
	/**/
	return iret;
	
}



int  minsock_udp_new( uint16_t srcport )
{
	int  iret;
	minip_ip_sock_t * psock;

	/**/
	if ( srcport == 0 )
	{
		/* alloc port */
		iret = minip_port_alloc( &srcport );
		if ( iret != 0 )
		{
			return -8;
		}
	}

	/**/
	psock = (minip_ip_sock_t *)malloc( sizeof(minip_ip_sock_t) );
	if ( psock == NULL )
	{
		minip_port_free( srcport );
		return -1;
	}

	/**/
	list_clear_node( &psock->node );
	psock->src_port = __htons( srcport );
	psock->dst_port = 0;
	psock->dst_ip = 0;
	psock->cbfunc = NULL;

	/**/
	return (int)(intptr_t)psock;
	
}


void  minsock_udp_free( int sock )
{
	minip_ip_sock_t * psock;

	/**/
	psock = (minip_ip_sock_t *)(intptr_t)sock;

	/**/
	if ( list_in_list( &psock->node ) )
	{
		list_delete( &psock->node );
	}

	/**/
	minip_port_free( __ntohs( psock->src_port ) );
	psock->cbfunc = NULL;
	psock->src_port = 0;
	psock->dst_port = 0;

	/**/
	free( psock );
	return;
}


/* bind to local address : udp port */
int  minsock_udp_bind( int sock, uint16_t srcport )
{
	minip_ip_sock_t * psock;

	/**/
	psock = (minip_ip_sock_t *)(intptr_t)sock;
	psock->src_port = __htons( srcport );
	
	/**/
	return 0;
}


/* bind to remote address : ip and udp port */
int  minsock_udp_connect( int sock, uint32_t dstip, uint16_t dstport )
{
	minip_ip_sock_t * psock;

	/**/
	psock = (minip_ip_sock_t *)(intptr_t)sock;
	psock->dst_port = __htons( dstport );
	psock->dst_ip = __htonl( dstip );
	
	/* raise match prioty */
	if ( list_in_list( &psock->node ) ) 
	{
		list_delete( &psock->node );
		list_add_head( &udp_listen_list, &psock->node );
	}

	/**/
	return 0;
}



/* local callback function */
int  minsock_udp_listen( int sock, udp_cbk_t cbf, void * parg )
{
	minip_ip_sock_t * psock;
	minip_ip_sock_t * ptemp;

	/**/
	psock = (minip_ip_sock_t *)(intptr_t)sock;
	psock->cbfunc = cbf;
	psock->parg = parg;

	/* check list, src_port is exist? */
	list_for_every_entry( &udp_listen_list, ptemp, minip_ip_sock_t, node )
	{
		if ( (ptemp->src_port == psock->src_port) && (ptemp->dst_port == psock->dst_port) && (ptemp->dst_ip == psock->dst_ip) )
		{
			return 1;
		}
	}

	/**/
	if ( psock->dst_port != 0 )
	{
		list_add_head( &udp_listen_list, &psock->node );
	}
	else
	{
		list_add_tail( &udp_listen_list, &psock->node );
	}
	
	return 0;
}



int  minsock_udp_send_to( int sock, uint32_t dstip, uint16_t dstport, pbuf_t * tpkt )
{
	int  iret;
	minip_ip_sock_t * psock;

	/**/
	psock = (minip_ip_sock_t *)(intptr_t)sock;
	dstport = __htons( dstport );
	dstip = __htonl( dstip );
	
	/**/
	iret = minip_build_udp_hdr( tpkt, psock->src_port, dstport );
	if ( iret != 0 )
	{
		return -1;
	}
	
	/**/
	iret = minip_send_ipv4( tpkt, dstip, IP_PROTO_UDP );
	if ( iret != 0 )
	{
		return (iret - 10);
	}
	
	/**/
	return 0;
	
}



/* shortcut send , sock must binded and connected */
int  minsock_udp_send( int sock, pbuf_t * tpkt )
{
	int  iret;
	minip_ip_sock_t * psock;

	/**/
	psock = (minip_ip_sock_t *)(intptr_t)sock;

	/**/
	iret = minip_build_udp_hdr( tpkt, psock->src_port, psock->dst_port );
	if ( iret != 0 )
	{
		return -1;
	}
	
	/**/
	iret = minip_send_ipv4( tpkt, psock->dst_ip, IP_PROTO_UDP );
	if ( iret != 0 )
	{
		return (iret - 10);
	}	

	/**/
	return 0;
	
}

void  minip_get_macaddr( uint8_t * padr )
{
	memcpy( padr, minip_mac, 6 );
	return;
}


void  minip_set_ipaddr( uint32_t thip, uint32_t mask, uint32_t gtway )
{
	inetadr = __htonl( thip );
	netmask = __htonl( mask );
	gateway = __htonl( gtway );
	return;
}


