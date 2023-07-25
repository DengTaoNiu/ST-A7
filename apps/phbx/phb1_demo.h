#ifndef PHB1_DEMO_H
#define PHB1_DEMO_H

#define WDT		0

typedef enum eDevIdx
{
	DEV_ALL 	= 0X00,
	DEV_ETH 	= 0x01,
	DEV_CAN		= 0x02,
	DEV_RS485_0	= 0x03,
	DEV_RS485_1 = 0x04,
	DEV_RS485_2 = 0x05,
	DEV_NUM,
}DevIdx;

//* Autbus
void test_dump_dpdata(phb_context_t *pctx);

//* ETH
void test_dump_eth(phb_context_t *pctx);

//* rs485 
typedef enum
{
	SUART_PORT_0,
	SUART_PORT_1,
	SUART_PORT_2,
} RS485;

void test_suart485(phb_context_t *pctx, int portid);

//* CAN
void test_dump_can(phb_context_t *pctx);

void timer_send_eth( int tid, void * parg );

// traffic statistics  流量统计
void traffic_statistics(pbuf_t * ipkt,phb_context_t *pctx);

#endif //PHB1_DEMO_H
