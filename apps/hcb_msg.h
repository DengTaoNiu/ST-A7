
/*

定义  acpu <--> bcpu 之间的控制消息接口。
一般情况下, 分为两类消息:

(a)  acpu 发送请求消息 给 bcpu .
     然后 bcpu 回复响应消息给 acpu.

(b)  仅仅是 bcpu 发送通知事件给 acpu.

消息的 header 格式是统一的, type + length ,  每个字段都是一个字节.
后面是 分片信息, 凑成 4 个字节, ??

*/



#include <stdint.h>


/* set, start */
#define  HCB_MSG_SET_MAC			1
#define  HCB_MSG_SET_PARAM			2
#define  HCB_MSG_STT_NETWORK		3

/* get */
#define  HCB_MSG_GET_THIS			8
#define  HCB_MSG_GET_NODES			9

/* resource, acpu->bcpu */
#define  HCB_MSG_ADD_TRS_FIXS		10
#define  HCB_MSG_DEL_TRS_FIXS		11
#define  HCB_MSG_ADD_RCV_FIXS		12
#define  HCB_MSG_DEL_RCV_FIXS		13
#define  HCB_MSG_COMMIT_FIXS		14


/* report : only on CN */
#define  HCB_MSG_RPT_NWNODE			15				/* new node */
#define  HCB_MSG_RPT_LSNODE			16 				/* loss node */

/* report : CN and TN */
#define  HCB_MSG_RPT_ONLINE			17
#define  HCB_MSG_RPT_OFFLINE		18


#define  HCB_MSG_UPDATE_GAIN		23
#define HCB_MSG_RECORD_AUTBUS 		24 
#define HCB_MSG_MAX 				24 


/**/
typedef struct  _tag_hcb_msg_hdr 
{
	uint8_t  type;
	uint8_t  leng;
	
	uint8_t  flag;				/* NACK? SN? */
	uint8_t  info;				/* dst node, For Remote Message */
	
} hcb_msg_hdr;


/*
set mac address:
	acpu >> bcpu 
*/

typedef struct  _tag_hcb_setMac
{
	uint8_t  role;
	uint8_t  ntype;						/* node type, for TN */
	uint8_t  mac[6];

	uint16_t  tx_pga;
	uint16_t  rx_pga;
	
} hcb_msg_setMac;



/*
set network and resource :
	acpu >> bcpu 
*/

typedef struct  _tag_hcb_setParam
{
	uint16_t  msp:2;
	uint16_t  mumb:2;
	uint16_t  mdmb:2;
	uint16_t  power_ratio:2;
	uint16_t  pilot_mode:2;
	uint16_t  band:1;
	uint16_t  resv0:5;
	
	uint16_t  resv1;
	
} hcb_msg_setParam;



/*
CN start network,  CN prepare join network:
	acpu >> bcpu 
*/

typedef struct  _tag_hcb_sttNetwork
{
	uint8_t  tx_pga;
	uint8_t  rx_pga;
	
} hcb_msg_sttNetwork;



/*
Get Nodes : get current Nodes 
arrays
	bcpu >> acpu 
*/

typedef struct  _tag_hcb_node_info
{
	uint8_t  nodeid;
	uint8_t  state;
	uint8_t  info;
	uint8_t  ntype;					/* device type : custum */
	uint8_t  mac[6];

} hcb_node_info;



/*
Add fixres : add trans or recv fixres 
acpu >> bcpu
*/

typedef struct  _tag_hcb_trs_fixres
{
	uint8_t  chan;
	uint8_t  freq;
	uint8_t  pos;
	uint8_t  len;

} hcb_trs_fixres;


typedef struct  _tag_hcb_rcv_fixres
{
	uint8_t  snode;
	uint8_t  freq;
	uint8_t  pos;
	uint8_t  len;
	
} hcb_rcv_fixres;




