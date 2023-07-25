
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "dlist.h"
#include "hcb_comn.h"
#include "pkt_api.h"
#include "sys_api.h"
#include "debug.h"
#include "phb1.h"
#include "atimer.h"
#include "atbui.h"
#include "phb1_demo.h"
// #include "apps.h"
#include "compare.h"
#include "sysres.h"

// cn 1,tn 0
int g_bCn = 0;
extern uint8_t bpskey;
// NODEID
// TN:
//[0-3] jonitID: 0,1,2
//[4-7] legID:0,1,2,3
// CN: dont care
uint64_t timer_software_ccp;
uint64_t timer_next_ccp;
int next_flag;

time_utc_t timccp_utc;
typedef struct
{
	uint64_t	tv_msec; /*ms，64bit*/
	uint64_t  tv_usec; /*us,16bit*/
}timeval_t;

int dbg_cmd_dpdata(void *parg, int argc, const char **argv)
{
	sysm_context_t *pctx;

	/**/
	pctx = (sysm_context_t *)parg;

	/**/
	ioctl(pctx->dfd, 0, 0, NULL);
	return 0;
}

#if (INCLUDE_TIMESYNC == 1)

time_utc_t now_timer;
extern uint32_t cap_cnt[2];
extern uint8_t utc_flag;
#define year_flash 0x701f00
#define month_flash 0x701f04
#define day_flash 0x701f08
#define hour_flash 0x701f0C
#define minute_flash 0x701f10
#define second_flash 0x701f14
#define frmhighflash 0x701f1C
#define tim_cnt_flash 0x701f18//autbus延时count值ns单位
#define frmlow_flash 0x701f20
#define symb_flash 0x701f24
#define smp_flash 0x701f28
#define tn_flash 0x701f2C
#define cn_flash 0x701f30
#define software_flag 0x701f38	 
#define tn_msp 0x701f3c
#define tn_timer_write 0x701f40
#define tn_timer_read 0x701f44
#define cn_timer_write 0x701f48
#define cn_timer_read 0x701f4c
#define timer_flag 0x701f50
#define sm_reg 0xA00010
#define fm_reg 0xA00014
uint64_t time_to_ms(time_utc_t tim)
{
	uint64_t temp;
	uint64_t tep_day_ms;
	temp =28;
	if((tim.year%4)==0)
	{
		temp=29;
	}
	if((tim.year-1970)%4==3)
	{
		tep_day_ms=(uint64_t)((tim.year-1970)/4)+1;
	}
	else
	{
		tep_day_ms=(uint64_t)(tim.year-1970)/4;		
	}
	tep_day_ms=(uint64_t)((tim.year-1970)*365+tep_day_ms);
	
	switch ( tim.month-1)
	{
		case 0:
			tep_day_ms=0+tep_day_ms;
			break;
		case 1:
			tep_day_ms=31+tep_day_ms;
			break;
		case 2:
			tep_day_ms=31+temp+tep_day_ms;
			break;
		case 3:
			tep_day_ms=62+temp+tep_day_ms;
			break;
		case 4:
			tep_day_ms=92+temp+tep_day_ms;
			break;
		case 5:
			tep_day_ms=123+temp+tep_day_ms;
			break;
		case 6:
			tep_day_ms=153+temp+tep_day_ms;
			break;
		case 7:
			tep_day_ms=184+temp+tep_day_ms;
			break;
		case 8:
			tep_day_ms=215+temp+tep_day_ms;
			break;
		case 9:
			tep_day_ms=245+temp+tep_day_ms;
			break;
		case 10:
			tep_day_ms=276+temp+tep_day_ms;
			break;
		case 11:
			tep_day_ms=306+temp+tep_day_ms;
			break;
	default:
		break;
	}
	tep_day_ms=(tep_day_ms+tim.day-1)*86400000;
	tep_day_ms=tep_day_ms+tim.hour*3600000+tim.minute*60000+tim.second*1000;
	return tep_day_ms;
}

timeval_t timer_software(void)
{
	// portENTER_CRITICAL();
	time_utc_t tim;
	timeval_t timval;
	uint32_t tmp;
	uint64_t tep_day_ms;
	uint32_t tnyingjian;
	uint32_t temp;
	uint64_t now_frme;
	uint8_t now_symbol;
	uint16_t now_symp;
	uint64_t frmns;
	uint32_t frm_lo;
	uint32_t frm_hi;
	uint32_t cm_smb,cm_frm;
	uint8_t msp;

	uint64_t tim_next;
	uint64_t now_tim;
	atbcfg_t *rocfg = get_atbcfg();
	
	// printf(" timer_software ---\r\n");
	if(read_reg(tn_timer_write)!=0x00)	//TN
	{
		write_reg(tn_timer_read,0x00);
		// printf("tn\r\n");
		msp=(uint8_t)read_reg(tn_msp);
		switch (msp)
		{
			case 0:
				cm_smb = 62500;
				cm_frm =4000000;
				break;
			case 1:
				cm_smb =31250;
				cm_frm =2000000;
				break;
			case 2:
				cm_smb =15625;
				cm_frm =1000000;
				break;
			case 3:
				cm_smb =7812;
				cm_frm =512000;
				break;
			default:
				break;
		}
		//读取当前的时间（年月日）
		tim.year=read_reg(year_flash);
		tim.month=read_reg(month_flash);
		tim.day=read_reg(day_flash);
		tim.hour=read_reg(hour_flash);
		tim.minute=read_reg(minute_flash);
		tim.second=read_reg(second_flash);
		frm_lo=read_reg(frmlow_flash);
		frm_hi=read_reg(frmhighflash);
		tim.frame=(uint64_t)frm_hi;
		tim.frame=(uint64_t)((tim.frame <<32)|(frm_lo));
		tim.symbol=read_reg(symb_flash);
		tim.smpol=read_reg(smp_flash);
		// printf("tim.month %d tim.hour%d second %d \r\n",tim.month,tim.hour,tim.second);
		//年月日化成ms
		tep_day_ms=time_to_ms(tim);
		// tnyingjian =0;
		
		#if 1	/*读取当前autbus时间*/
		tmp=read_reg(tim_cnt_flash);
		temp = read_reg(fm_reg);
		write_reg(0x440000, (uint32_t)(0xb)); 
		now_frme =(uint64_t) temp;
		now_frme= now_frme<< 8;

		temp = read_reg(sm_reg);
		now_frme=(uint64_t)(now_frme| (temp >> 20));
		now_symbol= (uint8_t)((temp >> 12) & 0x3F);
		now_symp =(uint16_t)(temp & 0x7FF);
		// printf("now_frme %d now_symbol%d now_symp %d \r\n",tim.frame,tim.symbol,tim.smpol);
		// printf("now_frme %d now_symbol%d now_symp %d \r\n",now_frme,now_symbol,now_symp);
		
		#endif 
		// if(utc_flag == 0x06)
		// {
		// 	utc_flag=0x00;
		// 	cap_cnt[1]=read_reg(0x44004C);	
		// }
		frmns=0;
		/*根据计算获取当前ns*/
		frmns=tmp+((now_frme*cm_frm+now_symbol*cm_smb+now_symp*40)-(tim.frame*cm_frm+tim.symbol*cm_smb+tim.smpol*40));
		
		
		timer_next_ccp = 100000000 - ((frmns%1000000000)/10)+200;
		// printf(" ----------  %Ld %x \r\n",timer_next_ccp,now_tim);
		write_reg(0x440034,(uint32_t)timer_next_ccp);
		// printf("timer_next_ccp -----%Ld  frmns %Ld \r\n",timer_next_ccp,frmns);
		// printf("-------now : frme  %Ld symbol %d symp %d  old : frm %Ld symbol %d symp %d \r\n",now_frme,now_symbol,now_symp,tim.frame,tim.symbol,tim.smpol);
		
		if(cap_cnt[1] > cap_cnt[0])
		{
			tnyingjian = (cap_cnt[1]-cap_cnt[0])*10;
		}
		else
		{
			tnyingjian =(cap_cnt[1] + (0xFFFFFFEE - cap_cnt[0]))*10;
		}
		
		// printf("yingjian %d \r\n",cap_cnt[0]);
		// printf("frmns %Ld \r\n",frmns);
		// printf("hardware %Ld \r\n",(frmns-(uint64_t)tnyingjian));
		timval.tv_msec=tep_day_ms;
		timval.tv_usec =frmns;	
		timval.tv_usec=(uint64_t)(frmns/1000);
		if(timval.tv_usec > 1000)
		{
			timval.tv_msec = timval.tv_msec + timval.tv_usec/1000;
			timval.tv_usec = timval.tv_usec%1000;
		}
		// printf("timval %Lx %d\r\n",timval.tv_msec,timval.tv_usec);
		// printf("timval %Ld %d\r\n",timval.tv_msec,timval.tv_usec);
		write_reg(tn_timer_read,0x06);
	}
	if(read_reg(cn_timer_write) != 0) //cn 时间已经存在
	{
		write_reg(cn_timer_read,0x00);
		// printf("cn\r\n");
		switch (rocfg->msp)
		{
			case 0:
				cm_smb = 62500;
				cm_frm =4000000;
				break;
			case 1:
				cm_smb =31250;
				cm_frm =2000000;
				break;
			case 2:
				cm_smb =15625;
				cm_frm =1000000;
				break;
			case 3:
				cm_smb =7812;
				cm_frm =512000;
				break;
			default:
				break;
		}
		//读取当前的时间（年月日）
		tim.year=read_reg(year_flash);
		tim.month=read_reg(month_flash);
		tim.day=read_reg(day_flash);
		tim.hour=read_reg(hour_flash);
		tim.minute=read_reg(minute_flash);
		tim.second=read_reg(second_flash);
		//年月日化成ms
		tep_day_ms=time_to_ms(tim);
		//读取autbus的时间
		frm_lo=read_reg(frmlow_flash);
		frm_hi=read_reg(frmhighflash);
		tim.frame=(uint64_t)frm_hi;
		tim.frame=(uint64_t)((tim.frame <<32)|(frm_lo));
		tim.symbol=read_reg(symb_flash);
		tim.smpol=read_reg(smp_flash);

		frm_lo=read_reg(frmlow_flash);
		frm_hi=read_reg(frmhighflash);
		tim.frame=(uint64_t)frm_hi;
		tim.frame=(uint64_t)((tim.frame <<32)|(frm_lo));
		tim.symbol=read_reg(symb_flash);
		tim.smpol=read_reg(smp_flash);

		tmp=read_reg(tim_cnt_flash);
		temp = read_reg(fm_reg);
		now_frme = temp;
		now_frme= now_frme<< 8;
		/*读取当前autbus时间*/
		temp = read_reg(sm_reg);
		now_frme=(uint64_t)(now_frme| (temp >> 20));
		now_symbol= (uint8_t)((temp >> 12) & 0x3F);
		now_symp =(uint16_t)(temp & 0x7FF);
		
		// frmns=tmp+(now_frme-tim.frame)*cm_frm+((now_symp*40+now_symbol*cm_smb)-(tim.symbol*cm_smb+tim.smpol*40));
		frmns=tmp+((now_frme*cm_frm+now_symbol*cm_smb+now_symp*40)-(tim.frame*cm_frm+tim.symbol*cm_smb+tim.smpol*40));
		timval.tv_msec=tep_day_ms;
		timval.tv_usec=(uint64_t)(frmns/1000);
		// timval.tv_usec=frmns;
		if(timval.tv_usec > 1000)
		{
			timval.tv_msec = timval.tv_msec + timval.tv_usec/1000;
			timval.tv_usec = timval.tv_usec%1000;
		}
		// printf("timval %Lx %x\r\n",timval.tv_msec,timval.tv_usec);
		// printf("frmms %Lx \r\n",frmns);
		write_reg(cn_timer_read,0x06);
	}
	// portEXIT_CRITICAL();		
	return timval;
	
}

timeval_t timer_api(void)
{
	time_utc_t tim;
	timeval_t timval;
	uint32_t tmp;
	uint64_t tep_day_ms;
	
	uint32_t temp;
	uint64_t now_frme;
	uint8_t now_symbol;
	uint16_t now_symp;
	uint64_t frmns;
	uint32_t frm_lo;
	uint32_t frm_hi;
	uint32_t cm_smb,cm_frm;
	atbcfg_t *rocfg = get_atbcfg();
	
	switch (rocfg->msp)
	{
		case 0:
			cm_smb = 62500;
			cm_frm =4000000;
			break;
		case 1:
			cm_smb =31250;
			cm_frm =2000000;
			break;
		case 2:
			cm_smb =15625;
			cm_frm =1000000;
			break;
		case 3:
			cm_smb =7812;
			cm_frm =512000;
			break;
		default:
			break;
	}

	if(read_reg(tn_timer_write)!=0x00)	//TN 时间读取
	{
		write_reg(tn_timer_read,0x00);
		//读取当前的时间（年月日）
		tim.year=read_reg(year_flash);
		tim.month=read_reg(month_flash);
		tim.day=read_reg(day_flash);
		tim.hour=read_reg(hour_flash);
		tim.minute=read_reg(minute_flash);
		tim.second=read_reg(second_flash);
		frm_lo=read_reg(frmlow_flash);
		frm_hi=read_reg(frmhighflash);
		tim.frame=(uint64_t)frm_hi;
		tim.frame=(uint64_t)((tim.frame <<32)|(frm_lo));
		tim.symbol=read_reg(symb_flash);
		tim.smpol=read_reg(smp_flash);
		//年月日化成ms
		tep_day_ms=time_to_ms(tim);
		// printf("minute %d second %d \r\n",tim.minute,tim.second);
		// printf("tim.frame %Ld tim.symbol %d tim.smpol %d \r\n",tim.frame,tim.symbol,tim.smpol);
		#if 1	/*读取当前autbus时间*/
		tmp=read_reg(tim_cnt_flash);
		temp = read_reg(fm_reg);
		now_frme =(uint64_t) temp;
		now_frme= now_frme<< 8;

		temp = read_reg(sm_reg);
		now_frme=(uint64_t)(now_frme| (temp >> 20));
		now_symbol= (uint8_t)((temp >> 12) & 0x3F);
		now_symp =(uint16_t)(temp & 0x7FF);
		// printf("---now.frame %Ld tim.symbol %d tim.smpol %d \r\n",now_frme,now_symbol,now_symp);
		#endif 
		// printf("yingjian   %d \r\n",cap_cnt[1]);
		/*根据计算获取当前ns*/
		frmns=tmp+((now_frme*cm_frm+now_symbol*cm_smb+now_symp*40)-(tim.frame*cm_frm+tim.symbol*cm_smb+tim.smpol*40));
		// printf("frmms %Ld \r\n",frmns);
		// printf("tmp %d \r\n",tmp);
		timval.tv_msec=tep_day_ms;
		
		
		timval.tv_usec=(uint64_t)(frmns/1000);
		if(timval.tv_usec > 1000)
		{
			timval.tv_msec = timval.tv_msec + (timval.tv_usec/1000);
			
			timval.tv_usec = timval.tv_usec%1000;
		}
		write_reg(tn_timer_read,0x06);
		// printf("timval %Ld %d\r\n",timval.tv_msec,timval.tv_usec);
	}
	if(read_reg(cn_timer_write) != 0) //cn时间写入
	{
		write_reg(cn_timer_read,0x00);
		//读取当前的时间（年月日）
		tim.year=read_reg(year_flash);
		tim.month=read_reg(month_flash);
		tim.day=read_reg(day_flash);
		tim.hour=read_reg(hour_flash);
		tim.minute=read_reg(minute_flash);
		tim.second=read_reg(second_flash);
		//年月日化成ms
		tep_day_ms=time_to_ms(tim);
		//读取autbus的时间
		frm_lo=read_reg(frmlow_flash);
		frm_hi=read_reg(frmhighflash);
		tim.frame=(uint64_t)frm_hi;
		tim.frame=(uint64_t)((tim.frame <<32)|(frm_lo));
		tim.symbol=read_reg(symb_flash);
		tim.smpol=read_reg(smp_flash);

		frm_lo=read_reg(frmlow_flash);
		frm_hi=read_reg(frmhighflash);
		tim.frame=(uint64_t)frm_hi;
		tim.frame=(uint64_t)((tim.frame <<32)|(frm_lo));
		tim.symbol=read_reg(symb_flash);
		tim.smpol=read_reg(smp_flash);

		tmp=read_reg(tim_cnt_flash);
		temp = read_reg(fm_reg);
		now_frme = temp;
		now_frme= now_frme<< 8;
		/*读取当前autbus时间*/
		temp = read_reg(sm_reg);
		now_frme=(uint64_t)(now_frme| (temp >> 20));
		now_symbol= (uint8_t)((temp >> 12) & 0x3F);
		now_symp =(uint16_t)(temp & 0x7FF);
		
		frmns=tmp+(now_frme-tim.frame)*4000000+((now_symp*40+now_symbol*62500)-(tim.symbol*62500+tim.smpol*40));
		
		timval.tv_msec=tep_day_ms;
		timval.tv_usec=(uint64_t)(frmns/1000);
		
		if(timval.tv_usec > 1000)
		{
			timval.tv_msec = timval.tv_msec + timval.tv_usec/1000;
			timval.tv_usec = timval.tv_usec%1000;
		}
		write_reg(cn_timer_read,0x06);
		
	}
			
	return timval;
	
}
void tim_simulatio(time_utc_t tim )
{
	
	if(read_reg(software_flag)==0x00)
	{
		
		write_reg(software_flag,0x06);
		write_reg(year_flash,tim.year);
		write_reg(month_flash,tim.month);
		write_reg(day_flash,tim.day);
		write_reg(hour_flash,tim.hour);
		write_reg(minute_flash,tim.minute);
		write_reg(second_flash,tim.second);
	}
	
	
}
time_utc_t tim_utc;
#define CN  0
#define TN  1
#define TEST 0
void timer_gpio(int tid, void *parg)
{
	phb_context_t *pctx;
	uint32_t gpio;
	pctx = (phb_context_t*)parg;
	gpio = 0;
	write(pctx->gpfd, (const void *)gpio,13);		//gpio13输出低电平
}


//一秒一次回调，进入后删除
void timer_temp_cbk(int tid, void *parg)
{
	static int gptd_num = 0;
	phb_context_t *pctx;
	int ifd;
	pbuf_t *pkt;
	int td;
	pctx = (phb_context_t*)parg;

	static timeval_t tntim;
	memset(&tntim,0,sizeof(tntim));
	//CN TN 都会调用这个timer api
	tntim=timer_api();
	// printf("tntim %Ld %d\r\n",tntim.tv_msec,tntim.tv_usec);
	
	/* 重新设定回调函数的定时器时间 */
	td = timer_create(timer_temp_cbk, pctx);
	timer_start(td, 10000, 0);
	timer_delete(tid);
}
void timer_cbk(int tid, void *parg)
{
	static int gptd_num = 0;
	phb_context_t *pctx;
	int ifd;
	pbuf_t *pkt;
	int td;
	int delay=7000;
	uint32_t gpio;
	timeval_t tntim;
	uint64_t tim_next;
	uint64_t now_tim;
	int gptd;
	static int next_first= 0;
	pctx = (phb_context_t*)parg;

#if TN
	// memset(&tntim,0,sizeof(tntim));
	tntim.tv_msec = 0;
	tntim.tv_usec = 0;
	write_reg(0x44004c, (uint32_t)(0x0));
	tntim=timer_software();
	
	// write_reg(0x440000, (uint32_t)(0xb)); //enble ccp0
	// now_tim = ((tntim.tv_msec %1000)*1000+tntim.tv_usec)*100;
	// timer_next_ccp = 100000000 - now_tim;
	// // printf(" ----------  %Ld %x \r\n",timer_next_ccp,now_tim);
	// write_reg(0x440034,(uint32_t)timer_next_ccp);
	// if(next_first == 0x00)
	// {
	// 	next_first =0x06;
	// 	write_reg(0x44004c, (uint32_t)(0x0));
	// 	write_reg(0x440004,(uint32_t)(read_reg(0x440004)|0x1));//enble ccp0_pwm0_p irq
	// 	// printf(" ----------  %x \r\n",read_reg(0x440004));
	// 	write_reg(0x440000, (uint32_t)(0xb)); //enble ccp0
	// 	now_tim = ((tntim.tv_msec %1000)*1000+tntim.tv_usec)*100;
	// 	// tim_next = 1000000-(tntim.tv_msec %1000)*1000+tntim.tv_usec;
	// 	// timer_next_ccp =tim_next *100 ;
	// 	// write_reg(0x440034,(uint32_t)timer_next_ccp);
	// 	tim_next = 100000000-now_tim;
	// 	timer_next_ccp =tim_next + timer_next_ccp;
	// 	if(timer_next_ccp > 0xFFFFFFED)
	// 	{
	// 		timer_next_ccp = timer_next_ccp- 0xFFFFFFEE;
	// 	}
	// 	next_flag = 0x06;
	// 	write_reg(0x440034,(uint32_t)timer_next_ccp);
	// }
	// else
	// {
		
		
	// 	// printf("tntim ms  %Ld   us %Ld    %d\r\n",tntim.tv_msec ,tntim.tv_usec,tim_next);
	// 	// write_reg(0x440034,(uint32_t)tim_next);
	// 	if(next_flag == 0x00)
	// 	{
	// 		now_tim = ((tntim.tv_msec %1000)*1000+tntim.tv_usec)*100;
	// 		timer_next_ccp =timer_next_ccp+now_tim;
	// 		if(timer_next_ccp > 0xFFFFFFED)
	// 		{
	// 			timer_next_ccp = timer_next_ccp- 0xFFFFFFEE;
	// 		}
	// 		write_reg(0x440038,(uint32_t)timer_next_ccp);
	// 		next_flag =0x08;
	// 	}
		
	// 	if(next_flag == 0x02)
	// 	{
	// 		tim_next = 100000000-now_tim;
	// 		timer_next_ccp =tim_next + timer_next_ccp;
	// 		if(timer_next_ccp > 0xFFFFFFED)
	// 		{
	// 			timer_next_ccp = timer_next_ccp- 0xFFFFFFEE;
	// 		}
	// 		write_reg(0x440034,(uint32_t)timer_next_ccp);
	// 		next_flag =0x06;
	// 	}
		
	// 	dprintf(0,"ccp0 pwm2   %d  %d\n",(uint32_t)timer_next_ccp,read_reg(0x44004c));

	// }
		
		// dprintf(0,"evt1  %d %d \n",(uint32_t)timer_software_ccp,read_reg(dctx->pbase + COMP_COUNT_REG));

	// printf("tntim ms  %d \n", tim_next);
#endif

#if CN		//cn 用来模拟测试
#if TEST

	tim_utc.year=(uint16_t)2022;
	tim_utc.month=(uint8_t)05;
	tim_utc.day=(uint8_t)26;
	tim_utc.hour=(uint8_t)19;
	tim_utc.minute=(uint8_t)12;
 #if ccp
	if (gptd_num == 0)
	{
		gpio = 1;
		//gpio13用来模拟秒脉冲信号,硬件gpio13和ccp_cap0;
		write(pctx->gpfd, (const void *)gpio,13);		//gpio13输出高电平
		tim_simulatio(tim_utc);	

				gptd_num =0;
				gptd = timer_create(timer_gpio, pctx);
				timer_start(gptd, 10000, 0);
			}
		#endif

		/*- Writes the initialization information setting to a register */
		tim_simulatio(tim_utc);
		if (gptd_num == 0)
		{
			gpio = 1;
			//gpio13用来模拟秒脉冲信号,硬件gpio13和ccp_cap0;
			// write(pctx->gpfd, (const void *)gpio,13);		//gpio13输出高电平

			/* 标志位置为06 */
			gptd_num =06;
			timer_software_ccp = 100000000;
			
			write_reg(0x440000, (uint32_t)(read_reg(0x440000)&(0xf7)));
			write_reg(0x44004c, (uint32_t)0);
			write_reg(0x440034,(uint32_t)timer_software_ccp);
			// printf("--------------timer_software %d \r\n",read_reg(0x44004c));
			write_reg(0x440000,0xb);
			// gptd = timer_create(timer_gpio, pctx);
			// timer_start(gptd, 10000, 0);
			
		}
		
		tim_utc.second = tim_utc.second+1;
		if(tim_utc.second >= 60)
		{
			tim_utc.second = 0;
		}
	#else
			tntim=timer_api();
	#endif
#endif

	tim_utc.ccp++;
	td = timer_create(timer_cbk, pctx);
	timer_start(td, 1000000, 0);
	timer_delete(tid);

#if CN
	tntim=timer_software();
#endif
}

int phb_tmcfg_context(void *parg, int argc, const char **argv)
{
    phb_context_t *pctx;

    /**/
    pctx = (phb_context_t *)parg;
	printf("phb_tmcfg_context %d \n",tim_utc.ccp);
    return 0;
}


#endif
extern uint8_t tnnodeid;
void timer_tick(int tid, void *parg)
{
	int iret;
	const char *pflag = "atbtimedelaytick";
	phb_context_t *pctx;
	pbuf_t *pkt;
	uint8_t *ptr;
	xheader_t *phdr;
	pctx = (phb_context_t *)parg;
	/**
	 * @brief 读取流量数据
	 */
	if(bpskey == 1){
		ioctl(pctx->dfd, 1, sizeof(bps_t) - 16, &bpsdata);
		bpsdata.tx_cursecbytes = bpsdata.tx_totalbytes - bpsdata.tx_lastsecbytes;
		bpsdata.tx_lastsecbytes = bpsdata.tx_totalbytes;
		bpsdata.rx_cursecbytes = bpsdata.rx_totalbytes - bpsdata.rx_lastsecbytes;
		bpsdata.rx_lastsecbytes = bpsdata.rx_totalbytes;
		/*tn每秒发送一次数据获取时间*/
		if (0 != pctx->role)
		{
			pkt = pkt_alloc(8);
			if (NULL == pkt)
			{
				timer_start(tid, 1000000, 0);
				return;
			}
			bpsdata.tx_lasttime = arch_timer_get_current();
			ptr = pkt_append(pkt, strlen(pflag) + 2);
			memcpy(ptr, pflag, strlen(pflag));
			ptr += strlen(pflag);
			*ptr = tnnodeid;

			phdr = (xheader_t *)pkt_prepend(pkt, XHEADER_LEN);
			phdr->id = XHEADER_ID;
			phdr->count = 1;
			phdr->length = pkt->length - XHEADER_LEN;
			iret = send(pctx->dfd, pkt);
			if (1 != iret)
			{
				pkt_free(pkt);
			}
		}
	}
	// printf("txtotal pkts = %d rxtotal pkts = %d \n", bpsdata.tx_totalpkts, bpsdata.rx_totalpkts);//yjn
	timer_start(tid, 1000000, 0);
}
int debug_cmd_esd(void *parg, int argc, const char **argv);
int debug_cmd_gpio(void *parg, int argc, const char **argv);
int entry_phbx(void *arg)
{
	int iret;
	int tfd; /*pts print io handle */
	int dfd; /*dpram autbus data handle */
	int mfd; /*timer handle */
	int tid;
	int rtfd;
	int capfd;
	int capfd1;
	// int sfd;        /*autbus sync handle */
	int cfd[3];		/*can fd with 2*spi->can handle */
	int efd;		/*eth handle */
	int rs485fd[3]; /*rs485 handle*/
	// int u1fd;        /*uart1 handle */
	// int pwmfd;   /*pwm handle */
	// int afd;        /*adc handle */
	int gpfd;   /*gpio handle */
	// int i2cfd;        /*i2c handle */
#if WDT
	int wfd = 0;
#endif	

	int gpioint;
	gpioint = 0;

	uint32_t irs;
	uint32_t rfds;
	uint32_t wfds;
	phb_context_t ctx;
	// get config from flashs
	atbcfg_t *rcfg = get_atbcfg();
	g_bCn = rcfg->role == 0 ? 1 : 0;
	ctx.role = rcfg->role;
	
	/*init*/
	tls_set(&ctx);

	//* pts
	tfd = open("/pts", 0);
	dprintf(0, "phb pts %d\n", tfd);
	ctx.cmctx.fd_stdio = tfd;
	epoll_add(tfd, -1);

	//* timer
	mfd = open("/timer", 0);
	printf("phb timer %d\n", mfd);
	ctx.cmctx.fd_timer = mfd;
	timer_queue_init();
	epoll_add(mfd, -1);

	//* Autbus
	dfd = open("/dpdata/normal", 0);
	printf("phb dpram normal data, %d\n", dfd);
	ctx.dfd = dfd;
	epoll_add(dfd, -1);

	//* Autbus
	rtfd = open("/dpdata/rt", 0);
	printf("phb dpram rt data, %d\n", rtfd);
	ctx.rtfd = rtfd;

	hcb_debug_init();
	debug_add_cmd("ctx", phb_cmd_context, &ctx);
	debug_add_cmd("dptest", phb_cmd_dptest, &ctx);
	debug_add_cmd("dpdata", dbg_cmd_dpdata, &ctx);
	debug_add_cmd("gpio", debug_cmd_gpio, &ctx);
	debug_add_cmd("esd", debug_cmd_esd, &ctx);

	write_reg(0x701f50,0x00);
#if (INCLUDE_TIMESYNC == 1)
	write_reg(timer_flag,0x66);
	debug_add_cmd("pcfg", phb_tmcfg_context, &ctx);
#endif

	tid = timer_create(timer_tick, &ctx);
	// timer_start(tid, 1000000, 0);

    ctx.efd = -1;
#if (INCLUDE_PHBETHXMIT == 1)
	//* ETH
	efd = open("/eth/sw", 0);
	ctx.efd = efd;
	printf("eth sw fd, %d\n", efd);
	epoll_add(efd, -1);
#endif

#if (INCLUDE_TIMESYNC == 1)
	capfd = open("/comp0", 0);
	ctx.capfd = capfd;
	printf("comp0 fd, %d\n", capfd);
	epoll_add(capfd, -1);

	capfd1 = open("/comp1", 0);
	ctx.capfd1 = capfd1;
	printf("comp1 fd, %d\n", capfd1);
	epoll_add(capfd1, -1);

#if TN
	write_reg(tn_flash,0x00);
	write_reg(cn_timer_write,0x00);
	write_reg(cn_timer_read,0x00);
	tid = timer_create(timer_cbk, &ctx);
	timer_start(tid, 1000000, 0);
	timer_next_ccp =0;
#if TEST
	write_reg(0x440004,(uint32_t)(read_reg(0x440004)|0x1));//enble ccp0_pwm0_p irq
	// write_reg(0x440000, (uint32_t)(0xb)); //enble ccp0
#endif
#endif
#endif

#if (INCLUDE_TIMESYNC == 1)
#if CN
	write_reg(0x440004,(uint32_t)(read_reg(0x440004)&0xFFFFFFFE));//disable ccp0 irq
	write_reg(cn_flash,0x00);
	write_reg(tn_timer_write,0x00);
	write_reg(tn_timer_read,0x00);
	write_reg(0x440000, (uint32_t)(0xb));//enble ccp0

	gpfd = open("/gpio/13",0);
	ioctl(gpfd,13,1,&gpioint);
	printf("phb gpfd 13 %d\r\n",gpfd);
	ctx.gpfd = gpfd;
	arch_irq_disable(32);

	tid = timer_create(timer_cbk, &ctx);
	timer_start(tid, 1000000, 0);
#endif

#if TEST
	write_reg(software_flag,0x00);
#else
	write_reg(software_flag,0x04);

#endif

#endif

    ctx.rs485fd[0] = -1;
#if (INCLUDE_PHB485XMIT0 == 1)
	//* 485-0
	rs485fd[0] = open("/suart/0", 0);
	ctx.rs485fd[0] = rs485fd[0];
	printf("phb rs485 0 fd %d\n", rs485fd[0]);
 	ioctl(ctx.rs485fd[0],0,sizeof(rs485cfg_t),&devcfg.rs485cfg[0]);
	epoll_add(rs485fd[0], -1);
  
#endif

    ctx.rs485fd[1] = -1;
#if (INCLUDE_PHB485XMIT1 == 1)
	//* 485-1
	rs485fd[1] = open("/suart/1", 0);
	ctx.rs485fd[1] = rs485fd[1];
	printf("phb rs485 1 fd %d\n", rs485fd[1]);
	ioctl(ctx.rs485fd[1],0,sizeof(rs485cfg_t),&devcfg.rs485cfg[1]);
	epoll_add(rs485fd[1], -1);
#endif

    ctx.rs485fd[2] = -1;
#if (INCLUDE_PHB485XMIT2 == 1)
	//* 485-2
	rs485fd[2] = open("/suart/2", 0);
	ctx.rs485fd[2] = rs485fd[2];
	printf("phb rs485 2 fd %d\n", rs485fd[2]);
	ioctl(ctx.rs485fd[2],0,sizeof(rs485cfg_t),&devcfg.rs485cfg[2]);
	epoll_add(rs485fd[2], -1);
#endif

    ctx.cfd[0] = -1;
#if (INCLUDE_PHBCANXMIT == 1)
	//* CAN
	cfd[0] = open("/can", 0);
	ctx.cfd[0] = cfd[0];
	printf("phb can fd %d\n", cfd[0]);
	epoll_add(cfd[0], -1);
	ioctl(ctx.cfd[0],0,sizeof(cancfg_t),&devcfg.cancfg[0]);
#endif
#if WDT
	wfd = open("/wdt", 1);
	printf("open wdt, %d\n", wfd);
#endif

	ctx.packed = !!configPHB_PackPacket;
	ctx.ethpkt = NULL;
	ctx.dpdatpkt = NULL;
	if (ctx.packed == 1)
	{
		ctx.timer_id = timer_create(timer_send_eth, &ctx);
		sys_mutex_init(&ctx.sem);
	}
	else
	{
		ctx.timer_id = -1;
	}

	while (1)
	{
		epoll_wait(&rfds, &wfds);
#if WDT
		write(wfd,0,0);
#endif
		/* pts */
		if (rfds & (1 << tfd))
		{
			hcb_debug_pts(tfd);
		}

		/* timer */
		if (rfds & (1 << mfd))
		{
			timer_queue_event();
		}

		/* dpram data channel */
		if (rfds & (1 << dfd))
		{
			/* atb->eth only */
			test_dump_dpdata(&ctx);
		}

#if (INCLUDE_PHBETHXMIT == 1)
		//* ETH
		if (rfds & (1 << efd))
		{
			/* eth->atb only */
			test_dump_eth(&ctx);
		}
#endif

#if (INCLUDE_PHB485XMIT0 == 1)
		//* 485-0
		if (rfds & (1 << rs485fd[0]))
		{
			/* suart0->atb only */
			test_suart485(&ctx, SUART_PORT_0);
		}
#endif

#if (INCLUDE_PHB485XMIT1 == 1)
		//* 485-1
		if (rfds & (1 << rs485fd[1]))
		{
			/* suart1->atb only */
			test_suart485(&ctx, SUART_PORT_1);
		}
#endif

#if (INCLUDE_PHB485XMIT2 == 1)
		//* 485-2
		if (rfds & (1 << rs485fd[2]))
		{
			/* suart2->atb only */
			test_suart485(&ctx, SUART_PORT_2);
		}
#endif

#if (INCLUDE_PHBCANXMIT == 1)
		//* CAN
		if (rfds & (1 << cfd[0]))
		{
			/* can->atb only */
			test_dump_can(&ctx);
		}
#endif
	}
}
