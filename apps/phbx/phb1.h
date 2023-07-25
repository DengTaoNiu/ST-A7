
#pragma once
#include "arch.h"
#include "timer.h"
#include "../../drvs/drv_can.h"
#include "../tmux/minip.h"
#include "mutex.h"
//#define LOG_DLY(format, args...)

typedef struct
{
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint8_t day;
	uint8_t month;
	uint16_t year;
	uint32_t ccp;
	uint64_t frame;
	uint8_t symbol;
	uint16_t smpol;
}time_utc_t;

typedef struct _tag_phb_context
{
	comn_context cmctx;

	/**/
	int dfd;		/* dpram, data */
	int rtfd;
	int cfd[3];		/* can and spi2can  */
	int mfd;		/* timer */
	int sfd;		/*dpram sync, timer*/
	int efd;		/* eth */
	int rs485fd[3]; /*rs485 handle*/
	
	// can txtimer
	int ttmr; /* timer handle */
	pbuf_list_t txlist[3];

	int u1fd;  /*uart1 handle */
	int pwmfd; /*pwm handle */
	int afd;   /*adc handle */
	int gpfd;  /*gpio handle */
	int i2cfd; /*i2c handle */
	int capfd;
	int capfd1;
	//int sramfd;
	int packed;/*小包功能标志位*/
    void *ethpkt;
    void *dpdatpkt;
    int timer_id;
    mutex_t sem;
	
	// socket for udp
	int psock;
	uint8_t role;
} phb_context_t;

typedef struct _EP_CAN_OBJ_MULTI_
{
	unsigned int ch;		  // can channel
	unsigned int ID;		  // id
	unsigned int TimeStamp;	  //?���յ���Ϣ֡ʱ��ʱ���ʾ����CAN��������ʼ����ʼ��ʱ
	unsigned char TimeFlag;	  //?�Ƿ�ʹ��ʱ���ʶ��Ϊ1ʱTimeStamp��Ч��TimeFlag��TimeStampֻ�ڴ�֡Ϊ����֡ʱ�����塣
	unsigned char RemoteFlag; //?�Ƿ���Զ��֡??0:����;CAN����������������֡,1:Զ��;CAN������������Զ��֡
	unsigned char ExternFlag; //?�Ƿ�����չ֡?0����׼֡??1����չ֡
	unsigned char DataLen;	  //?���ݳ���(<=8)����Data�ĳ��ȡ�
	unsigned char Data[8];	  //?���ĵ�����????
} EP_CAN_MULTI_OBJ, *EP_CAN_MULTI_OBJ_PTR;

typedef enum
{
	IDLE = 0,
	MODE,
	EN
} em_dr_st;

typedef struct
{
	em_dr_st st;
	/* last current set, via aut*/
	float scur;
	/* current of drv dog got via can*/
	float gcur;
	/* pos of drv dog got via can*/
	int gpos;
	/*heart beat for can of driver and pc, based on current.*/
	uint32_t cntping;
	/* heart beat for downlink from PC=>aut=>driver*/
	uint32_t cntdlping;
	/*driver enable*/
	bool enable;
} dog_joint_t;

typedef enum
{
	JOINT_0,
	JOINT_1,
	JOINT_2,
	JOINT_CNT,
} E_LEG_JOINTID;

typedef struct
{
	uint8_t legid;
	dog_joint_t joint[JOINT_CNT];
} dog_leg_t;

extern dog_leg_t leg;


typedef struct _tag_bps_
{
	uint64_t tx_totalpkts;
	uint64_t tx_totalbytes;
	uint64_t tx_fulldrop;
	uint64_t tx_fulldropbytes;
	uint64_t rx_totalpkts;
	uint64_t rx_totalbytes;
	uint64_t tx_cursecbytes;
	uint64_t rx_cursecbytes;
	uint64_t rx_lastsecbytes;
	uint64_t tx_lastsecbytes;
	uint64_t tx_lasttime;
	uint64_t tx_delay;
	uint8_t nodeid;
} bps_t;
extern bps_t bpsdata;

#define UDP_sPORT 8000
#define UDP_dPORT 8000

/**/
int phb_cmd_context(void *parg, int argc, const char **argv);

/* eth */
int dbg_cmd_eth(void *parg, int argc, const char **argv);

/* dpram, data channel */
// void test_dump_dpdata(phb_context_t *pctx);

/* can */
// void test_dump_can(phb_context_t *pctx, int fd);
int phb_cmd_send_can(void *parg, int argc, const char **argv);
void test_dump_dpdata_rt(phb_context_t *pctx);
int phb_cmd_dptest(void *parg, int argc, const char **argv);
int dbg_cmd_dpdata(void *parg, int argc, const char **argv);
