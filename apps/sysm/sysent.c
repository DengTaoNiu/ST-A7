

#include "compiler.h"
#include "dlist.h"

#include "arch.h"
#include "wait.h"
#include "thread.h"
#include "mutex.h"

#include <stdio.h>
#include "sys_api.h"
#include "debug.h"
#include "atimer.h"
#include "hcb_msg.h"

#include "sysent.h"
#include "sysmgr.h"
#include "sysrpc.h"
#include "sysres.h"
#include "resmgr.h"
#include "atbui.h"
#include "hcb_comn.h"
#include "phb1.h"

extern time_utc_t timccp_utc;

int dbg_cmd_dpram(void *parg, int argc, const char **argv)
{
	sysm_context_t *pctx;

	/**/
	pctx = (sysm_context_t *)parg;

	/**/
	ioctl(pctx->dfd, 0, 0, NULL);
	return 0;
}

typedef struct _tag_start_param
{
	uint8_t msp;
	uint8_t mdmb;
	uint8_t mumb;

	uint8_t role;
	uint8_t ntype;
	uint8_t mac[6];

	/**/
	uint8_t band;
	uint8_t power;
	uint16_t txpga;
	uint16_t rxpga;

} start_param_s;

void test_sttnet_cbk(void *parg, uint8_t *tbuf)
{
	int tid;
	hcb_msg_hdr *phdr;
	uint8_t *pcfg;

	/**/
	if (tbuf == NULL)
	{
		printf("stt net cbk, time out\n");
		return;
	}

	/* check result */
	phdr = (hcb_msg_hdr *)tbuf;
	if (phdr->flag != 0)
	{
		printf("stt net cbk, flag=%u\n", phdr->flag);
		return;
	}

	/**/
	return;
}

void test_setparam_cbk(void *parg, uint8_t *tbuf)
{
	hcb_msg_hdr *phdr;
	hcb_msg_sttNetwork *pnet;
	uint8_t tary[128];

	/**/
	if (tbuf == NULL)
	{
		printf("set param cbk, time out\n");
		return;
	}

	/* check result */
	phdr = (hcb_msg_hdr *)tbuf;
	if (phdr->flag != 0)
	{
		printf("set param cbk, flag=%u\n", phdr->flag);
		return;
	}

	/**/
	phdr = (hcb_msg_hdr *)tary;
	pnet = (hcb_msg_sttNetwork *)(phdr + 1);

	/**/
	phdr->type = HCB_MSG_STT_NETWORK;
	phdr->leng = sizeof(hcb_msg_hdr) + sizeof(hcb_msg_sttNetwork);
	phdr->flag = 0;
	phdr->info = 0;

	/**/
	pnet->rx_pga = 0;
	pnet->tx_pga = 0;

	/**/
	sysmgr_send(tary, test_sttnet_cbk, parg, 40000);
	return;
}

void test_setmac_cbk(void *parg, uint8_t *tbuf)
{
	start_param_s *parm;
	hcb_msg_hdr *phdr;
	hcb_msg_setParam *param;
	uint8_t tary[128];

	/**/
	parm = (start_param_s *)parg;
	if (tbuf == NULL)
	{
		printf("set mac cbk, time out\n");
		return;
	}

	/* check result */
	phdr = (hcb_msg_hdr *)tbuf;
	if (phdr->flag != 0)
	{
		printf("set mac cbk, flag=%u\n", phdr->flag);
		return;
	}
	/**/
	phdr = (hcb_msg_hdr *)tary;
	param = (hcb_msg_setParam *)(phdr + 1);

	/**/
	phdr->type = HCB_MSG_SET_PARAM;
	phdr->leng = sizeof(hcb_msg_hdr) + sizeof(hcb_msg_setParam);
	phdr->flag = 0;
	phdr->info = 0;

	/**/
	param->msp = parm->msp;
	param->mdmb = parm->mdmb;
	param->mumb = parm->mumb;
	param->power_ratio = parm->power;
	param->pilot_mode = 0;
	param->band = parm->band;
	param->resv0 = 0;
	param->resv1 = 0;

	/**/
	sysmgr_send(tary, test_setparam_cbk, parg, 40000);
	return;
}

void record_autbus_ctrl(uint32_t onoff)
{
	hcb_msg_hdr *phdr;
	uint8_t tary[128];
	uint8_t *ptr;

	phdr = (hcb_msg_hdr *)tary;
	ptr = (uint8_t *)(phdr + 1);

	/**/
	phdr->type = HCB_MSG_RECORD_AUTBUS;
	phdr->leng = sizeof(hcb_msg_hdr) + 4;
	phdr->flag = 0;
	phdr->info = 0;

	*ptr = onoff;

	/**/
	sysmgr_send(tary, NULL, NULL, 40000);
	return;
}

void update_power_ratio(uint8_t power, uint8_t txpga)
{
	hcb_msg_hdr *phdr;
	uint8_t tary[128];
	uint8_t *ptr;

	phdr = (hcb_msg_hdr *)tary;
	ptr = (uint8_t *)(phdr + 1);

	/**/
	phdr->type = HCB_MSG_UPDATE_GAIN;
	phdr->leng = sizeof(hcb_msg_hdr) + sizeof(hcb_msg_setParam);
	phdr->flag = 0;
	phdr->info = 0;

	*ptr = txpga;
	*(ptr + 1) = power;

	/**/
	sysmgr_send(tary, NULL, NULL, 40000);
	return;
}

int test_setmac_action(start_param_s *parm)
{
	int iret;
	uint8_t tary[128];
	// uint64_t temptimer;
	hcb_msg_hdr *phdr;
	hcb_msg_setMac *pmac;

	/* action, ctrl mac */
	phdr = (hcb_msg_hdr *)tary;
	pmac = (hcb_msg_setMac *)(phdr + 1);

	/**/
	phdr->type = HCB_MSG_SET_MAC;
	phdr->leng = sizeof(hcb_msg_hdr) + sizeof(hcb_msg_setMac);
	phdr->flag = 0;
	phdr->info = 0;

	/**/
	pmac->role = parm->role;
	pmac->ntype = parm->ntype;
	memcpy(pmac->mac, parm->mac, 6);

	/**/
	pmac->rx_pga = parm->rxpga;
	pmac->tx_pga = parm->txpga;

	/**/
	iret = sysmgr_send(tary, test_setmac_cbk, parm, 40000);
	// temptimer = arch_timer_get_current();
	// write_reg(0x701f5c,(temptimer&0xFFFFFFFF));	
	// write_reg(0x701f60,((temptimer>>32)&0xFFFFFFFF));



	return iret;
}

/* msp,  mdmb, mumb */
static int debug_cmd_start(void *pctx, int argc, const char **argv)
{
	int iret;
	uint32_t temp;
	uint8_t msp;
	uint8_t mdmb;
	uint8_t mumb;
	start_param_s *parm;

	/**/
	if (argc < 4)
	{
		goto usage;
	}

	/**/
	iret = debug_str2uint(argv[1], &temp);
	if (iret != 0)
	{
		printf("msp fmt err\n");
		goto usage;
	}

	if (temp > 3)
	{
		printf("msp range err, 0-3\n");
		goto usage;
	}

	msp = (uint8_t)temp;

	/**/
	iret = debug_str2uint(argv[2], &temp);
	if (iret != 0)
	{
		printf("mdmb fmt err\n");
		goto usage;
	}

	if (temp > 3)
	{
		printf("mdmb range err, 0-3\n");
		goto usage;
	}

	mdmb = (uint8_t)temp;

	/**/
	iret = debug_str2uint(argv[3], &temp);
	if (iret != 0)
	{
		printf("mumb fmt err\n");
		goto usage;
	}

	if (temp > 3)
	{
		printf("mumb range err, 0-3\n");
		goto usage;
	}

	mumb = (uint8_t)temp;

	/* start session */
	parm = (start_param_s *)malloc(sizeof(start_param_s));
	if (parm == NULL)
	{
		printf("malloc struct fail\n");
		return 0;
	}

	/**/
	sysinfo(1, parm->mac);
	parm->role = 0;
	parm->msp = msp;
	parm->mdmb = mdmb;
	parm->mumb = mumb;

	/**/
	iret = test_setmac_action(parm);
	if (0 != iret)
	{
		printf("send mac fail\n");
		return 0;
	}

	/**/
	return 0;

usage:
	printf("usage: %s <msp> <mdmb> <mumb>\n", argv[0]);
	return 0;
}

static void test_this_cbk(void *parg, uint8_t *tbuf)
{
	hcb_msg_hdr *phdr;
	hcb_node_info *pnds;

	/**/
	if (tbuf == NULL)
	{
		printf("this cbk, time out\n");
		return;
	}

	/* check result */
	phdr = (hcb_msg_hdr *)tbuf;
	pnds = (hcb_node_info *)(phdr + 1);
	if (phdr->flag != 0)
	{
		printf("this cbk, flag=%u\n", phdr->flag);
		return;
	}

	/**/
	printf("this: n=%u, s=%u, i=%u\n", pnds->nodeid, pnds->state, pnds->info);
	return;
}

static int debug_cmd_this(void *parg, int argc, const char **argv)
{
	sysm_context_t *pctx;
	hcb_msg_hdr thdr;

	/**/
	pctx = (sysm_context_t *)parg;
	printf("nodeid:%u\n", pctx->nodeid);

	/**/
	thdr.type = HCB_MSG_GET_THIS;
	thdr.leng = sizeof(hcb_msg_hdr);
	thdr.info = 0;
	thdr.flag = 0;

	/**/
	sysmgr_send((uint8_t *)&thdr, test_this_cbk, NULL, 1000000);
	return 0;
}

static int debug_cmd_rpc(void *parg, int argc, const char **argv)
{
	int iret;
	uint32_t temp;
	uint8_t tnd;
	int tlen;
	intptr_t req;
	uint8_t ptr[200];

	/**/
	if (argc < 3)
	{
		goto usage;
	}

	/**/
	iret = debug_str2uint(argv[1], &temp);
	if (iret != 0)
	{
		printf("dst node, fmt err\n");
		goto usage;
	}
	tnd = (uint8_t)temp;

	/**/
	iret = debug_str2uint(argv[2], &temp);
	if (iret != 0)
	{
		printf("msg length, fmt err\n");
		goto usage;
	}
	tlen = (int)temp;

	/**/
	iret = rpc_request_create(tnd, 0, &req);
	if (iret != 0)
	{
		printf("rpc req create, %d\n", iret);
		return 0;
	}

	/**/
	rpc_request_send(req, ptr, tlen);
	return 0;

usage:
	printf("%s <node> <length>\n", argv[0]);
	return 0;
}

int test_srv_echo(void *parg, int ilen, void *ibuf)
{
	rpc_respond_send(ilen, ibuf);
	printf("service, echo, called, %d\n", ilen);
	return 0;
}

int get_ap_gpio(uint32_t gpio);
/**
 * @brief 此函数为用户接口，可考虑回调函数，设备报名函数
 * @return int 非0为设备报名状态
 */
int report_check()
{
	static int old;
	int iret;
	iret = get_ap_gpio(30);
	iret ^= 1;
	if (iret != old)
	{
		printf("gpio level change:%d\n", iret);
		old = iret;
	}
	return iret;
}

/**
 * @brief 信道质量
 *
 * @return int
 */
typedef struct
{
	uint64_t errup;
	uint64_t errdn;
} atbsig_history_t;

static atbsig_history_t sighis[254];
/**
 * @brief 信道质量检测，错包有增加，返回1，否则返回0
 *
 * @return int
 */
int atbsig_check()
{
	int i;
	uintptr_t pbase;
	uint32_t temp0;
	uint32_t temp1;
	uint32_t temp2;
	uint32_t temp3;
	uint64_t cberr_dn;
	uint64_t cberr_up;
	uint8_t nhdvdn;
	uint8_t nhdvup;
	int sigerr;

	pbase = BASE_BB_HCBMAC;
	sigerr = 0;
	for (i = 0; i < 254; i++)
	{
		temp0 = read_reg(pbase + HCB_TAB_CBN_0(i));
		temp1 = read_reg(pbase + HCB_TAB_CBN_1(i));
		temp2 = read_reg(pbase + HCB_TAB_CBN_2(i));
		temp3 = read_reg(pbase + HCB_TAB_CBN_3(i));
		if ((temp0 == 0) && (temp1 == 0))
		{
			continue;
		}
		/*autbus 两线错包统计*/
		cberr_dn = temp2 & 0x3fffff;
		cberr_dn = (cberr_dn << 20) | (temp1 >> 12);
		cberr_up = temp3;
		cberr_up = (cberr_up << 10) | (temp2 >> 22);

		temp0 = read_reg(BASE_BB_HCBMAC + HCB_TAB_NHD_X(i));
		nhdvup = (temp0 >> 21) & 0x1;
		nhdvdn = (temp0 >> 10) & 0x1;
		if (sighis[i].errdn != cberr_dn)
		{
			sighis[i].errdn = cberr_dn;
			sigerr = 1;
			// LOG_OUT(LOG_DBG, "sig err:%d-0x%x--0x%x\n", i, sighis[i].errdn, cberr_dn);
		}
		if (sighis[i].errup != cberr_up)
		{
			sighis[i].errup = cberr_up;
			sigerr = 1;
			// LOG_OUT(LOG_DBG, "sig err:%d-0x%x--0x%x\n", i, sighis[i].cberr_up, cberr_up);
		}
	}
	return sigerr;
}
uint64_t systime;
int rpc_get_device(int nodeid, sysm_context_t *pctx);
void set_cn_sigerr();
void set_cn_report();
int report_check();

/**
 * @brief tn主动上报信息，信道质量错误增加
 * 		  节点主动报名
 */

static tnreport_t treport;
static void sys_timer_tick(int tid, void *parg)
{
	static uint32_t cnt;
	uint8_t tary[16];
	uint8_t upld_flag;
	atbcfg_t *pcfg;
	sysm_context_t *pctx;
	pctx = (sysm_context_t *)parg;
	cnt++;
	// int i = 0;
	if (0 == (cnt % 20))
	{
		systime++;
		pctx->systime++;
		if (atbui_cmd.reboot_delay > 16)
		{
			atbui_cmd.reboot_delay = 0;
			write_reg(0x540010, 0);
		}
		if (atbui_cmd.reboot_delay > 14)
		{
			atbui_cmd.reboot_delay++;
		}
	}
	/**
	 * @brief 上报信息检查，每200ms检查一次，
	 *
	 */
	if (0 == (cnt % 4))
	{
		/**
		 * @brief 上次信息还未发走，先不检查，避免丢信息
		 *
		 */
		if (0 == pctx->sigerr)
		{
			pctx->sigerr = atbsig_check();
		}
		if (0 == pctx->report)
		{
			pctx->report = report_check();
		}
	}

	/**
	 * @brief tn上报信息，每2秒检查一次，有就上报
	 *
	 */
	if (0 == (cnt % 40))
	{
		upld_flag = 0;
		if ((0 != pctx->sigerr) || (0 != pctx->report))
		{
			if (0 != pctx->nodeid)
			{
				treport.nodeid = pctx->nodeid;
				upld_flag = 1;
			}
			else
			{
				if (0 != pctx->sigerr)
				{
					set_cn_sigerr();
				}
				if (0 != pctx->report)
				{
					set_cn_report();
				}
			}
			treport.sigerr = pctx->sigerr;
			treport.report = pctx->report;
			pctx->sigerr = 0;
			pctx->report = 0;
		}

		if (0 != upld_flag)
		{
			rpc_tn_send(0, &treport, sizeof(tnreport_t));
			upld_flag = 0;
		}
	}

	switch (atbui_cmd.atbui_cmd)
	{
	case E_GET_ATB_SIG:
		if (atbui_cmd.atb_sig_nodeid == 0)
		{
			sysres_srv_bcpu_rtdata(NULL, 0, 0);
		}
		else
		{
			rpc_get_atbsig(atbui_cmd.atb_sig_nodeid);
		}
		break;
	case E_GET_DEBUG_DATA:
		//  printf("###E_GET_DEBUG_DATA \n");
		if (atbui_cmd.dbgget_nodeid == 0)
		{
			sysres_srv_debug_get(parg, 0, 0);
		}
		else
		{
			rpc_get_debug(atbui_cmd.dbgget_nodeid);
		}
		break;
	case E_GET_BPS_DATA:
		if (atbui_cmd.dbgget_nodeid == 0)
		{
			sysres_srv_bps_get(parg, 0, 0);
		}
		else
		{
			rpc_get_bps(atbui_cmd.dbgget_nodeid);
		}
		break;
	case E_GET_DEVICE_DATA:
		if (atbui_cmd.devget_nodeid == 0)
		{
			sysres_srv_device_get(parg, 0, 0);
		}
		else
		{
			rpc_get_device(atbui_cmd.devget_nodeid, pctx);
		}
		break;
	case E_SET_DEBUG_DATA:
		if (atbui_cmd.dbgset_nodeid == 0)
		{
			sysres_srv_debug_set(parg, 0, 0);
		}
		else
		{
			rpc_set_debug(atbui_cmd.dbgset_nodeid);
		}
		break;
	case E_SET_DEVICE_DATA:
		if (atbui_cmd.devset_nodeid == 0)
		{
			sysres_srv_device_set(parg, 0, 0);
		}
		else
		{
			rpc_set_device(atbui_cmd.devset_nodeid);
			if (atbui_cmd.devset_nodeid == 255)
			{
				sysres_srv_device_set(parg, 0, 0);
			}
		}
		// LOG_OUT(LOG_DBG,"set device:%d\n",atbui_cmd.devset_nodeid);
		break;
	case E_UPDATE_TXGAIN:
		pcfg = get_atbcfg();
		update_power_ratio(pcfg->power, pcfg->txpga);
		break;
	case E_OTA_START:
		sysres_srv_ota_start(pctx, 0, NULL);
		rpc_ota_start();
		if (0 != atbui_cmd.uiforward_nodeid)
		{
			ui_eth_send(pctx->efd, atbui_cmd.ota_reply_buf, atbui_cmd.ota_reply_len);
		}
		else
		{
			memcpy(tary, "otareply", 9);
			tary[9] = E_OTA_START;
			write(pctx->msguifd, tary, 10);
		}

		break;
	case E_OTA_DATA:
		if (atbui_cmd.ota_flag != 0)
		{
			sysres_srv_ota_data(pctx, 0, NULL);
		}
		rpc_ota_data();

		if (0 != atbui_cmd.uiforward_nodeid)
		{
			ui_eth_send(pctx->efd, atbui_cmd.ota_reply_buf, atbui_cmd.ota_reply_len);
		}
		else
		{
			memcpy(tary, "otareply", 9);
			tary[9] = E_OTA_DATA;
			write(pctx->msguifd, tary, 10);
		}

		break;
	case E_OTA_END:
		if (atbui_cmd.ota_flag != 0)
		{
			sysres_srv_ota_end(pctx, 0, NULL);
		}
		rpc_ota_end();

		if (0 != atbui_cmd.uiforward_nodeid)
		{
			ui_eth_send(pctx->efd, atbui_cmd.ota_reply_buf, atbui_cmd.ota_reply_len);
		}
		else
		{
			memcpy(tary, "otareply", 9);
			tary[9] = E_OTA_END;
			write(pctx->msguifd, tary, 10);
		}

		break;
	// case E_SET_DYN:
	// 	if(dyn == 0){
	// 		dyn = 1;
	// 		write_reg(0x701ff8,0xaa55aa00);
	// 		write_reg(0x701ffc,0x0);
	// 		printf("DYN CFG OK\n");
	// 	}
	// 	rpc_set_dyn(0xff);
	// 	break;
	default:
		break;
	}
	atbui_cmd.atbui_cmd = E_UI_CMD_NONE;
	
    // timer_start(tid, 1000000, 0);
	return;
}

/**
 * @brief 消息，resmgr ---> sysmgr单向的
 *      主要是resmgr通知sysmgr发送rpc消息和ctrl消息
 * @param fd
 */
void resmsg_proc(int fd)
{
	int iret;
	uint8_t msg[128];
	intptr_t irpc;
	hcb_msg_hdr *phdr;
	while (1)
	{
		iret = read(fd, msg, 128);
		if (iret < 0)
		{
			break;
		}
		switch (msg[0])
		{
		case 'c':
			sysmgr_send(&msg[1], NULL, NULL, 0);
			phdr = (hcb_msg_hdr *)&msg[1];
			break;
		case 'r':
			memcpy(&irpc, &msg[4], 4);
			rpc_request_send(irpc, (void *)&msg[8], 120);
			break;
		default:
			debug_dump_hex(msg, 16);
			break;
		}
	}
}
/*flash 读取设备参数 */

int entry_sysmgr(void *parg)
{
	int iret;
	int tfd; /* pty, printf, debug */
	int mfd; /* timer */
	int dfd; /* dpram, ctrl queue */
	int qfd; /* queue, ipc */
	int efd; /* eth, interface */
	int ffd; /* flash */
	int tid;
	int msgresfd;
	int msguifd;
	uint32_t rfds;
	uint32_t wfds;
	sysm_context_t ctx;
	start_param_s tarm;
	atbcfg_t *patbcfg;

	memset((void *)&ctx, 0, sizeof(sysm_context_t));

	/**/
	ctx.nodeid = 254;
	tls_set(&ctx);// tls机制，为了避免被不同的线程访问的资源发生同步问题 

	/* pts */
	tfd = open("/pts", 0);
	ctx.cmctx.fd_stdio = tfd;
	hcb_debug_init();

	/* timer */
	mfd = open("/timer", 0);
	printf("open timer, %d\n", mfd);
	ctx.cmctx.fd_timer = mfd;
	timer_queue_init();

	/**/
	if (INCLUDE_DrvFlash)
	{
		ffd = open("/flash", 0);
		printf("open flash %d\n", ffd);
		ctx.ffd = ffd;
	}
	if (INCLUDE_DrvNandFlash)
	{
		ffd = open("/nandflash", 0);
		printf("open nandflash %d\n", ffd);
		ctx.ffd = ffd;
	}
	/* dpram - 两线信息交互的文件描述符*/
	dfd = open("/dpctrl", 0);
	printf("open dpctrl, %d\n", dfd);
	ctx.dfd = dfd;

	/* dpram - ui资源交互的文件描述符*/
	msgresfd = open("/mesg/resmsg", 0);
	ctx.msgresfd = msgresfd;
	LOG_OUT(LOG_INFO, "open resmsg, %d\n", msgresfd);

	/* dpram - ui消息交互的文件描述符*/
	msguifd = open("/mesg/uimsg", 0);
	ctx.msguifd = msguifd;
	LOG_OUT(LOG_INFO, "open uimsg, %d\n", msguifd);
	
	/*定时器 - ui界面刷新的定时器*/
	tid = timer_create(sys_timer_tick, &ctx);
	timer_start(tid, 1000000, 50000); //定时器计时。第一次超时时间为1000000us，重复计时器超时间隔50000us
	
	/*B -> A*/
	sysmgr_reg_cbk(test_dump_sysm, &ctx);
	sysmgr_init(dfd); 
	sysres_init();

	/* remote rpc */
	qfd = open("/dprrpc", 0);
	printf("open dprrpc, %d\n", qfd);
	ctx.qfd = qfd;

	/**/
	rpc_init(qfd);
	rpc_service_register(0, test_srv_echo, NULL);

	rpc_service_register(0x12, sysres_srv_add_res_id, &ctx);
	rpc_service_register(0x13, sysres_srv_add_trs_res, &ctx);
	rpc_service_register(0x14, sysres_srv_add_rcv_res, &ctx);
	rpc_service_register(0x15, sysres_srv_del_res, &ctx);
	rpc_service_register(0x16, sysres_srv_bcpu_rtdata, &ctx);
	rpc_service_register(0x17, sysres_srv_debug_get, &ctx);
	rpc_service_register(0x18, sysres_srv_debug_set, &ctx);
	rpc_service_register(0x19, sysres_srv_ota_start, &ctx);
	rpc_service_register(0x20, sysres_srv_ota_data, &ctx);
	rpc_service_register(0x21, sysres_srv_ota_end, &ctx);
	rpc_service_register(0x22, sysres_srv_device_get, &ctx);
	rpc_service_register(0x23, sysres_srv_device_set, &ctx);
	rpc_service_register(0x24, sysres_srv_bps_get, &ctx);
	rpc_service_register(0x25, sysres_srv_ui_data, &ctx);
	rpc_service_register(0x26, sysres_srv_dyn_set, &ctx);
	rpc_service_register(0x27, sysres_srv_tn_info, &ctx);
	/**/
	debug_add_cmd("dpram", dbg_cmd_dpram, &ctx);
	debug_add_cmd("this", debug_cmd_this, &ctx);
	debug_add_cmd("rpc_ctrl", dbg_cmd_rrpc_ctrl, &ctx);
	debug_add_cmd("rpc", debug_cmd_rpc, &ctx);
	debug_add_cmd("res", debug_cmd_res, &ctx);
	debug_add_cmd("test", debug_cmd_test, &ctx);

	/**/
	epoll_add(tfd, -1);
	epoll_add(mfd, -1);
	epoll_add(dfd, -1);
	epoll_add(qfd, -1);
	epoll_add(msgresfd, -1);

	/**
	 * @brief msguifd单向，不用接收消息
	 *
	 */
	// epoll_add(msguifd, -1);
	sysinfo(1, ctx.mac);
	ctx.nodeid = 254;
	patbcfg = get_atbcfg();
	if (patbcfg->role != 0)
	{
		efd = open("/eth/if", 0);
		LOG_OUT(LOG_INFO, "open efd %d\n", efd);
		// printf("----------------open efd%d\n", efd);
		ctx.efd = efd;
		epoll_add(efd, -1);
		int dbg_cmd_eth(void *parg, int argc, const char **argv);
		debug_add_cmd("eth", dbg_cmd_eth, &ctx);
	}

	// sysinfo(1, ctx.mac);
	// patbcfg = get_atbcfg();
	/* other is test mode, not start autbus */
	if ((patbcfg->role == 0) || (patbcfg->role == 2))
	{

		tarm.role = patbcfg->role;
		tarm.ntype = 0;
		tarm.msp = patbcfg->msp & 0x3;
		tarm.mdmb = patbcfg->dmdb & 0x3;
		tarm.mumb = patbcfg->umdb & 0x3;
		tarm.band = patbcfg->bandsel;
		tarm.power = patbcfg->power;
		tarm.txpga = patbcfg->txpga;
		tarm.rxpga = 0x0;
	
		memcpy(tarm.mac, ctx.mac, MAC_LEN);

		/**/
		thread_sleep(200000);
		
		test_setmac_action(&tarm);
	}

	/**/
	while (1)
	{
		epoll_wait(&rfds, &wfds);

		/**/
		if (rfds & (1 << tfd))
		{
			hcb_debug_pts(tfd);
		}
		if (patbcfg->role != 0)
		{
			if (rfds & (1 << efd))
			{
				if (ctx.nodeid != 254)
				{
					atb_ui_tn_process(efd, ffd);
				}else{
					atb_ui_process(efd, ffd);
				}
			}
		}
		/**/
		if (rfds & (1 << mfd))
		{
			timer_queue_event();
		}

		/* dpctrl */
		if (rfds & (1 << dfd))
		{
			sysmgr_proc_event();
		}

		if (rfds & (1 << qfd))
		{
			rpc_proc_input(qfd, ctx.nodeid);
		}

		if (rfds & (1 << msgresfd))
		{
			resmsg_proc(msgresfd); //ui资源消息任务
		}
	}

	/**/
	return 0;
}
