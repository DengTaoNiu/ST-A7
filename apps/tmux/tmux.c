
#include "compiler.h"
#include "dlist.h"
#include "hcb_comn.h"

#include "arch.h"
#include "wait.h"
#include "thread.h"
#include "mutex.h"

#include "sys_api.h"
#include "debug.h"
#include "atimer.h"
#include "cmds.h"
#include "minip.h"
#include "tftp.h"
#include "resmgr.h"
#include "atbui.h"
#include "sysres.h"

/**/
#define LINE_LEN 129
#define HIST_NUM 4
static char his_strs[HIST_NUM][LINE_LEN + 1];
static uint32_t his_cur = 0;
static uint32_t his_next = 0;

static const char *history_prev(void)
{
	/**/
	his_cur = (his_cur - 1) % HIST_NUM;
	if (his_cur == his_next)
	{
		return "";
	}

	/**/
	if (his_strs[his_cur][0] == '\0')
	{
		return NULL;
	}

	/**/
	return his_strs[his_cur];
}

static const char *history_next(void)
{
	his_cur = (his_cur + 1) % HIST_NUM;
	if (his_cur == his_next)
	{
		return "";
	}

	if (his_strs[his_cur][0] == '\0')
	{
		return NULL;
	}

	/**/
	return his_strs[his_cur];
}

static void history_add(const char *str)
{
	uint32_t tidx;

	/* cur reset */
	his_cur = his_next;

	/**/
	tidx = (his_next - 1) % HIST_NUM;
	if (0 == strcmp(his_strs[tidx], str))
	{
		/* nothing */
		return;
	}

	/**/
	strcpy(his_strs[his_next], str);
	his_next = (his_next + 1) % HIST_NUM;
	his_cur = his_next;
	return;
}

/**/
static int sidx = -1;
static char spad[LINE_LEN + 1];
static char prmt[LINE_LEN + 1];

/**/
int con_prompt(const char *pmt)
{
	int iret;

	/**/
	iret = strlen(pmt);
	if (iret > LINE_LEN)
	{
		return 1;
	}

	/**/
	strcpy(prmt, pmt);
	return 0;
}

/*

http://ascii-table.com/ansi-escape-sequences-vt-100.php  esc : 0x1b

return :
	iret < 0 : 读文件失败， uart 确实没有输入字符了。
	iret = 0 : 识别了一个完整字符 (4 bytes)， 包括特殊的 删除 上翻 等特殊字符。
	iret > 0 : 发现一个 不合法 的特殊序列.

*/

static uint32_t esc_value = 0;

int con_getch(int ufd, uint32_t *pch)
{
	int iret;
	uint8_t tch;

	/**/
	iret = read(ufd, &tch, 1);
	if (iret <= 0)
	{
		return iret - 100;
	}

	if (esc_value == 0)
	{
		if (tch == 0x1b)
		{
			esc_value = 0x1b;
			return 2;
		}

		/**/
		*pch = tch;
		return 0;
	}
	else if (esc_value == 0x1b)
	{
		if (tch == 0x5b)
		{
			esc_value = 0x1b5b;
			return 3;
		}

		/* clear */
		esc_value = 0;
		*pch = tch;
		return 0;
	}
	else if (esc_value == 0x1b5b)
	{
		esc_value = 0;
		*pch = 0x1b5b00 | tch;
		return 0;
	}
	else
	{
		/* error */
		esc_value = 0;
		return 4;
	}
}

/*
console : histor, basic edit (back.)
*/

int con_getline(int ufd, char **pptr)
{
	int iret;
	const char *str;
	uint32_t temp;

	/**/
	if (sidx == -1)
	{
		printf("\n%s", prmt);
		sidx = 0;
	}

	while (1)
	{
		/* uart */
		iret = con_getch(ufd, &temp);
		if (iret < 0)
		{
			break;
		}

		if (iret > 0)
		{
			/* 识别到不符协议的序列， 暂时先忽略. */
			continue;
		}

		/* simple line edit.. */
		switch (temp)
		{
		case 0x1b5b41:
			/* up arrow */
			str = history_prev();
			if (str != NULL)
			{
				sidx = strlen(str);
				strcpy(spad, str);
				printf("\e[100D\e[K");
				printf(prmt);
				printf(spad);
			}
			break;

		case 0x1b5b42:
			/* down arrow */
			str = history_next();
			if (str != NULL)
			{
				sidx = strlen(str);
				strcpy(spad, str);
				printf("\e[100D\e[K");
				printf(prmt);
				printf(spad);
			}
			break;

		case 0x7f:
		case 0x08:
			/* back space */
			if (sidx > 0)
			{
				sidx--;
				printf("\b \b");
			}
			break;

		case 0x03:
			/* ctrl + c ： 特殊的返回值. */
			return 0;

		case 0x0A:
		case 0x0D:
			/* enter */
			if (sidx <= 0)
			{
				printf("\n%s", prmt);
				break;
			}

			spad[sidx] = '\0';
			history_add(spad);
			*pptr = spad;
			iret = sidx;
			sidx = -1;
			printf("\n");
			return iret;

		default:
			printf("%c", (int)temp);

			if (sidx < LINE_LEN)
			{
				spad[sidx] = (char)temp;
				sidx += 1;
			}
			else
			{
				sidx = -1;
				printf("\n\tstring is too long, try again\n");
			}
			break;
		}
	}

	/**/
	return -100;
}

int con_history(void)
{
	int i;

	/**/
	printf("his_next: %lu\n", his_next);
	printf("his_cur : %lu\n", his_cur);

	/**/
	for (i = 0; i < HIST_NUM; i++)
	{
		if (his_strs[i][0] != '\0')
		{
			printf("%d:%d:%s\n", i, strlen(his_strs[i]), his_strs[i]);
		}
		else
		{
			printf("%d:[NULL]\n", i);
		}
	}

	return 0;
}

int con_init(int tfd)
{
	int i;

	/**/
	strcpy(prmt, ">> ");

	/**/
	for (i = 0; i < HIST_NUM; i++)
	{
		his_strs[i][0] = '\0';
	}

	/**/
	his_cur = 0;
	his_next = 0;

	/**/
	return 0;
}

typedef struct _tag_tmux_context
{
	comn_context cmctx;

	/* ptm */
	int tfd;
	int index;

	/**/
	int efd;
	int ffd;
	int msguifd;

	/* upgrade.bin, tftp session */
	int inprog;
	uint32_t upload_ofs;
	uint32_t download_ofs;
	uint32_t download_max;

} tmux_context_t;

int debug_cmd_switch(void *parg, int argc, const char **argv)
{
	int i;
	int iret;
	uint32_t temp;
	tmux_context_t *pctx;
	char tary[32];

	/**/
	pctx = (tmux_context_t *)parg;

	/**/
	if (argc < 2)
	{
		/* list all pts */
		for (i = 0; i < 32; i++)
		{
			*(int32_t *)tary = i;
			iret = ioctl(pctx->tfd, 1, 32, tary);
			if (iret < 0)
			{
				break;
			}

			/**/
			printf("-%d- : %s\n", i, tary);
		}

		goto usage;
	}

	/**/
	iret = debug_str2uint(argv[1], &temp);
	if (iret != 0)
	{
		goto usage;
	}

	/**/
	iret = ioctl(pctx->tfd, 0, 4, &temp);
	if (iret != 0)
	{
		printf("ioctl, fail, %d\n", iret);
		goto usage;
	}

	/**/
	pctx->index = (int)temp;
	tary[0] = '0' + pctx->index;
	tary[1] = '>';
	tary[2] = ' ';
	tary[3] = '\0';
	con_prompt(tary);
	return 0;

usage:
	printf("%s <idx>\n", argv[0]);
	return 0;
}

static int dbg_cmd_sing(void *parg, int argc, const char **argv)
{
	int iret;
	uintptr_t pbase;
	uint32_t temp;

	if (argc < 2)
	{
		goto usage;
	}

	if (0 == strcmp(argv[1], "stop"))
	{
		pbase = BASE_BB_HCBMAC;

		/* stop */
		write_reg(pbase + 0x40, 2);
		write_reg(pbase + 0x3C, 0);
		return 0;
	}

	/**/
	if (argc < 3)
	{
		goto usage;
	}

	/**/
	if (0 == strcmp(argv[1], "single"))
	{
		/**/
		iret = debug_str2uint(argv[2], &temp);
		if (iret != 0)
		{
			printf("arg, mode, fmt err, %d\n", iret);
			goto usage;
		}

		/**/
		temp = temp & 0x3F;
		printf("single, mode = %u\n", temp);

		/**/
		pbase = BASE_BB_HCBMAC;

		/**/
		temp = (temp << 4) | 0x10000002;
		write_reg(pbase + 0x3C, temp);
		write_reg(pbase + 0x40, 0x1);
		return 0;
	}
	else if (0 == strcmp(argv[1], "multi"))
	{
		/**/
		iret = debug_str2uint(argv[2], &temp);
		if (iret != 0)
		{
			printf("arg, mode, fmt err, %d\n", iret);
			goto usage;
		}

		/**/
		temp = temp & 0x1;
		printf("multi, mode = %u\n", temp);

		/**/
		pbase = BASE_BB_HCBMAC;

		/* linedriver, manual-0 */
		temp = (temp << 10) | 0x1000000A;
		write_reg(pbase + 0x3C, temp);
		write_reg(pbase + 0x40, 0x1);
		return 0;
	}

usage:
	printf("%s stop\n", argv[0]);
	printf("%s single <mode-0-19>\n", argv[0]);
	printf("%s multi  <mode-0/1>\n", argv[0]);
	return 0;
}

static int dbg_cmd_capadc(void *parg, int argc, const char **argv)
{
	int iret;
	uintptr_t pbase;
	uint32_t temp;

	/**/
	pbase = BASE_BB_HCBMAC;

	/* buf, address */
	write_reg(pbase + 0x88, 0x07800000);

	/* 固定长度 64k */
	temp = read_reg(pbase + 0x3C);
	temp = temp & 0xF0000EFF;
	temp = temp | 0x8000004;
	write_reg(pbase + 0x3C, temp);
	write_reg(pbase + 0x40, 0x4);
	return 0;
}

static int dbg_cmd_gain(void *parg, int argc, const char **argv)
{
	int iret;
	uintptr_t pbase;
	uint32_t temp;
	uint32_t vval;

	/**/
	if (argc < 3)
	{
		goto usage;
	}

	if (0 == strcmp(argv[1], "tx"))
	{
		/**/
		iret = debug_str2uint(argv[2], &temp);
		if (iret != 0)
		{
			printf("arg, tx_gain, fmt err, %d\n", iret);
			goto usage;
		}

		/**/
		pbase = BASE_BB_AFE;

		/* TX_GAIN, 5 bits */
		temp = temp & 0x37;
		vval = ((temp & 0x30) >> 1) | (temp & 0x7);
		write_reg(pbase + 0x64, vval);

		/**/
		printf("tx : gain <<  0x%02X ( 0x%X )\n", temp, vval);
		return 0;
	}
	else if (0 == strcmp(argv[1], "rx"))
	{
		/**/
		iret = debug_str2uint(argv[2], &temp);
		if (iret != 0)
		{
			printf("arg, rx_lna_lpf_buf, fmt err, %d\n", iret);
			goto usage;
		}

		/**/
		pbase = BASE_BB_AFE;
		write_reg(pbase + 0x6C, (temp >> 8) & 0x3);
		write_reg(pbase + 0x68, (temp >> 4) & 0xF);
		write_reg(pbase + 0x70, (temp >> 0) & 0x7);
		temp = temp & 0x3F7;
		printf("rx : lna_lpf_buf <<  0x%03X\n", temp);
		return 0;
	}

usage:
	printf("%s tx <0xAB: 2:3>\n", argv[0]);
	printf("%s rx <0xABC: 2:4:3>\n", argv[0]);
	return 0;
}

extern int hcb_debug_main(char *spad);

void tmux_fwd_uart2ptm(tmux_context_t *pctx)
{
	int iret;
	char *ptr;

	/**/
	while (1)
	{
		iret = con_getline(pctx->cmctx.fd_stdio, &ptr);
		if (iret < 0)
		{
			break;
		}

		if (iret == 0)
		{
			/* ctrl + c */
			if (pctx->index >= 0)
			{
				int32_t tidx = -1;

				/**/
				pctx->index = tidx = -1;
				ioctl(pctx->tfd, 0, 4, &tidx);

				/* re print line */
				con_prompt(">> ");
			}

			/**/
			continue;
		}

		if (pctx->index < 0)
		{
			hcb_debug_main(ptr);
		}
		else
		{
			write(pctx->tfd, ptr, iret + 1);
		}
	}
}

void tmux_fwd_ptm2uart(tmux_context_t *pctx)
{
	int iret;
	uint8_t tary[32];

	/**/
	while (1)
	{
		iret = read(pctx->tfd, tary, 32);
		if (iret <= 0)
		{
			break;
		}

		/**/
		write(pctx->cmctx.fd_stdio, tary, iret);
	}
}

int upgrade_file_write(int fd, const void *buf, uint32_t tsiz)
{
	int iret;
	tmux_context_t *pctx;

	/**/
	pctx = (tmux_context_t *)(intptr_t)fd;

	/**/
	if ((pctx->upload_ofs + tsiz) > 524288)
	{
		return -11;
	}

	/**/
	iret = write(pctx->ffd, buf, tsiz);
	if (iret != tsiz)
	{
		return -12;
	}
	pctx->upload_ofs += tsiz;
	return 0;
}

int upgrade_file_close(int fd)
{
	tmux_context_t *pctx;

	/**/
	pctx = (tmux_context_t *)(intptr_t)fd;
	printf("upgrade, close, %u\n", pctx->upload_ofs);

	/* flush */
	ioctl(pctx->ffd, 1, 0, NULL);
	pctx->inprog = 0;
	pctx->download_max = pctx->upload_ofs;
	return 0;
}

static void test_reboot_timer_cbk(int tid, void *parg)
{
	// reboot(0);
	soc_reboot();
	return;
}

/**/
int upgrade_file_open(char *fname, tftp_file_write_t *ppwrite, tftp_file_close_t *ppclose)
{
	tmux_context_t *pctx;
	uint32_t uary[2];

	if (0 == strcmp(fname, "reboot.bin"))
	{
		int tid;

		tid = timer_create(test_reboot_timer_cbk, NULL);
		timer_start(tid, 50000, 0);
		return -12;
	}

	/**/
	if (0 != strcmp(fname, "upgrade.bin"))
	{
		return -18;
	}

	/**/
	pctx = (tmux_context_t *)tls_get();

	/**/
	if (pctx->inprog != 0)
	{
		return -11;
	}

	/* flash, unlock */
	uary[0] = 0;
	ioctl(pctx->ffd, 2, sizeof(uint32_t), &uary[0]);

	/* flash, erase, ofs = 1M, 256K */
	uary[0] = 0x100000;
	uary[1] = 0x040000;
	ioctl(pctx->ffd, 3, 2 * sizeof(uint32_t), &uary[0]);

	/* flash, seek, ofs = 1M. */
	uary[0] = 0x100000;
	ioctl(pctx->ffd, 0, sizeof(uint32_t), &uary[0]);
	pctx->upload_ofs = 0;
	pctx->inprog = 0x111;
	*ppwrite = upgrade_file_write;
	*ppclose = upgrade_file_close;
	return (int)(intptr_t)pctx;
}

int download_file_read(int fd, void *buf, uint32_t tsiz)
{
	tmux_context_t *pctx;
	uint32_t temp;
	uintptr_t sadr;

	/**/
	pctx = (tmux_context_t *)(intptr_t)fd;

	/**/
	temp = pctx->download_max - pctx->download_ofs;
	if (temp > tsiz)
	{
		temp = tsiz;
	}

	/**/
	if (temp > 0)
	{
		sadr = 0x800000 + pctx->download_ofs;
		memcpy(buf, (void *)sadr, temp);
		pctx->download_ofs += temp;
	}

	return temp;
}

int download_file_close(int fd)
{
	tmux_context_t *pctx;

	/**/
	pctx = (tmux_context_t *)(intptr_t)fd;

	/**/
	printf("download, close, %u\n", pctx->download_ofs);
	pctx->inprog = 0;
	return 0;
}

int download_file_open(char *fname, tftp_file_read_t *ppread, tftp_file_close_t *ppclose)
{
	tmux_context_t *pctx;
	uint32_t temp;

	/**/
	pctx = (tmux_context_t *)tls_get();

	/**/
	if (pctx->inprog != 0)
	{
		return -11;
	}

	/* 128k, bcpu sram */
	pctx->download_max = 0x20000;
	;
	pctx->download_ofs = 0;

	/**/
	pctx->inprog = 0x222;

	/**/
	*ppread = download_file_read;
	*ppclose = download_file_close;
	return (int)(intptr_t)pctx;
}

int dbg_cmd_eth(void *parg, int argc, const char **argv)
{
	int iret;
	tmux_context_t *pctx;
	uint32_t regofs;
	uint32_t temp;
	uint32_t tary[2];
	pbuf_t *ipkt;
	uint8_t *ptr;

	/**/
	pctx = (tmux_context_t *)parg;

	/**/
	if (argc < 2)
	{
		goto usage;
	}

	/**/
	if (0 == strcmp(argv[1], "info"))
	{
		ioctl(pctx->efd, 4, 0, NULL);
		return 0;
	}

	if (0 == strcmp(argv[1], "send"))
	{
		/**/
		ipkt = pkt_alloc(64);
		ptr = pkt_append(ipkt, 64);
		memset(ptr, 0xFF, 6);
		memset(&(ptr[6]), 0x11, 60);

		/**/
		send(pctx->efd, ipkt);
		return 0;
	}

	/**/
	if (0 == strcmp(argv[1], "read"))
	{
		if (argc < 3)
		{
			goto usage;
		}

		iret = debug_str2uint(argv[2], &regofs);
		if (0 != iret)
		{
			printf("ofs arg err\n");
			goto usage;
		}

		/**/
		printf("try read phy : ofs=%lu\n", regofs);

		/**/
		tary[0] = regofs;
		tary[1] = 0;

		/**/
		iret = ioctl(pctx->efd, 1, sizeof(tary), tary);
		if (0 != iret)
		{
			printf("read fail = %d\n", iret);
			return 0;
		}

		/**/
		printf("read succ = 0x%x\n", tary[1]);
		return 0;
	}

	if (0 == strcmp(argv[1], "write"))
	{
		if (argc < 4)
		{
			goto usage;
		}

		iret = debug_str2uint(argv[2], &regofs);
		if (0 != iret)
		{
			printf("ofs arg err\n");
			goto usage;
		}

		iret = debug_str2uint(argv[3], &temp);
		if (0 != iret)
		{
			printf("val arg err\n");
			goto usage;
		}

		/**/
		printf("try write phy : ofs=%lu, val=0x%lx\n", regofs, temp);

		/**/
		tary[0] = regofs;
		tary[1] = temp;

		/**/
		iret = ioctl(pctx->efd, 2, sizeof(tary), tary);
		if (0 != iret)
		{
			printf("write fail = %d\n", iret);
			return 0;
		}

		/**/
		printf("write succ\n");
		return 0;
	}

usage:
	printf("usage: %s [ info | read <ofs> | write <ofs> <val> ]\n", argv[0]);
	return 0;
}

int debug_cmd_flash(void *parg, int argc, const char **argv)
{
	int iret;
	uint32_t addr;
	uint32_t tlen;
	uint32_t pttn;
	uint8_t *pdat;
	uint32_t uary[2];
	tmux_context_t *pctx;

	/**/
	pctx = (tmux_context_t *)parg;
	if (argc < 2)
	{
		goto usage;
	}

	if (0 == strcmp(argv[1], "read"))
	{
		if (argc < 4)
		{
			goto usage;
		}

		/**/
		iret = debug_str2uint(argv[2], &addr);
		if (iret != 0)
		{
			printf("arg, addr, fmt err\n");
			goto usage;
		}

		iret = debug_str2uint(argv[3], &tlen);
		if (iret != 0)
		{
			printf("arg, tlen, fmt err\n");
			goto usage;
		}

		/**/
		pdat = (uint8_t *)malloc(tlen);
		if (pdat == NULL)
		{
			printf("alloc fail, s=%u\n", tlen);
			goto usage;
		}

		/**/
		ioctl(pctx->ffd, 0, sizeof(addr), &addr);
		iret = read(pctx->ffd, pdat, tlen);
		if (iret != tlen)
		{
			printf("read fail, s=%u\n", tlen);
			free(pdat);
			return 0;
		}

		/**/
		debug_dump_hex(pdat, tlen);
		free(pdat);
		return 0;
	}
	else if (0 == strcmp(argv[1], "write"))
	{
		if (argc < 5)
		{
			goto usage;
		}

		/**/
		iret = debug_str2uint(argv[2], &addr);
		if (iret != 0)
		{
			printf("arg, addr, fmt err\n");
			goto usage;
		}

		iret = debug_str2uint(argv[3], &tlen);
		if (iret != 0)
		{
			printf("arg, tlen, fmt err\n");
			goto usage;
		}

		iret = debug_str2uint(argv[4], &pttn);
		if (iret != 0)
		{
			printf("arg, pattern, fmt err\n");
			goto usage;
		}

		/**/
		pdat = (uint8_t *)malloc(tlen);
		if (pdat == NULL)
		{
			printf("alloc fail, s=%u\n", tlen);
			goto usage;
		}

		memset(pdat, (int)pttn, tlen);

		/**/
		ioctl(pctx->ffd, 0, sizeof(addr), &addr);
		iret = write(pctx->ffd, pdat, tlen);
		if (iret != tlen)
		{
			printf("write fail, s=%u\n", tlen);
			free(pdat);
			return 0;
		}

		/* flush */
		ioctl(pctx->ffd, 1, 0, NULL);
		free(pdat);
		return 0;
	}
	else if (0 == strcmp(argv[1], "erase"))
	{
		if (argc < 4)
		{
			goto usage;
		}

		/**/
		iret = debug_str2uint(argv[2], &uary[0]);
		if (iret != 0)
		{
			printf("arg, addr, fmt err\n");
			goto usage;
		}

		iret = debug_str2uint(argv[3], &uary[1]);
		if (iret != 0)
		{
			printf("arg, tlen, fmt err\n");
			goto usage;
		}

		/**/
		ioctl(pctx->ffd, 3, 2 * sizeof(uint32_t), &uary[0]);
		return 0;
	}
	else if (0 == strcmp(argv[1], "unlock"))
	{
		uary[0] = 0;
		ioctl(pctx->ffd, 2, sizeof(uint32_t), &uary[0]);
		return 0;
	}
	else if (0 == strcmp(argv[1], "uid"))
	{
		pdat = (uint8_t *)malloc(16);
		if (pdat == NULL)
		{
			printf("alloc fail, s=%u\n", tlen);
			goto usage;
		}

		iret = ioctl(pctx->ffd, 4, 16, pdat);
		printf("uid, ret = %d\n", iret);
		debug_dump_hex(pdat, 16);
		free(pdat);
		return 0;
	}

usage:
	printf("%s read  <addr> <size>\n", argv[0]);
	printf("%s write <addr> <size> <pattern>\n", argv[0]);
	printf("%s erase <addr> <size>\n", argv[0]);
	printf("%s unlock \n", argv[0]);
	printf("%s uid \n", argv[0]);
	return 0;
}

int debug_cmd_tftp(void *parg, int argc, const char **argv)
{
	int iret;
	uint32_t temp;
	tmux_context_t *pctx;

	/**/
	pctx = (tmux_context_t *)parg;
	printf("old download size : %u\n", pctx->download_max);

	/**/
	if (argc < 2)
	{
		goto usage;
	}

	/**/
	iret = debug_str2uint(argv[1], &temp);
	if (iret != 0)
	{
		printf("arg, size, fmt err\n");
		goto usage;
	}

	/**/
	printf("new download size : %u\n", temp);
	pctx->download_max = temp;
	return 0;

usage:
	printf("%s <size>\n", argv[0]);
	return 0;
}

void test_dump_eth_if(int tfd)
{
	int iret;
	uint32_t tcnt;
	pbuf_t *ipkt;
	uint32_t cpsr;

	/**/
	while (1)
	{
		iret = recv(tfd, &ipkt);
		if (iret <= 0)
		{
			break;
		}

		// printf( "if:::recv, %d\n", ipkt->length );
		minip_input(ipkt);
	}
}

static int debug_cmd_role(void *parg, int argc, const char **argv)
{
	int iret;
	tmux_context_t *pctx;
	uint32_t temp;
	atbcfg_t *pcfg;

	/**/
	pctx = (tmux_context_t *)parg;

	iret = flash_load_atbcfg(pctx->ffd);
	pcfg = get_atbcfg();

	/**/
	if (argc < 2)
	{
		/* check valid */
		if (pcfg->role == 0)
		{
			printf("cur: role=cn, mode=0x%01x%01x%01x\n", pcfg->msp, pcfg->dmdb, pcfg->umdb);
			printf("txpga=0x%x,power=0x%x,bandsel:0x%x\n", pcfg->txpga, pcfg->power, pcfg->bandsel);
		}
		else if (pcfg->role == 2)
		{
			printf("cur: role=tn\n");
		}
		else
		{
			printf("cur: role=test(%u)\n", pcfg->role);
		}

		goto usage;
	}

	/**/
	if (0 == strcmp(argv[1], "cn"))
	{
		pcfg->role = 0;

		if (argc >= 3)
		{
			iret = debug_str2uint(argv[2], &temp);
			if (0 != iret)
			{
				printf("mode, fmt err, %d\n", iret);
				goto usage;
			}
			pcfg->msp = (temp >> 8) & 0x3;
			pcfg->dmdb = (temp >> 4) & 0x3;
			pcfg->umdb = (temp)&0x3;
		}

		/**/
		if (argc >= 4)
		{
			iret = debug_str2uint(argv[3], &temp);
			if (0 != iret)
			{
				printf("txgain, fmt err, %d\n", iret);
				goto usage;
			}
			pcfg->txpga = temp & 0x1f;
			if (pcfg->txpga > 17)
			{
				pcfg->txpga = 17;
			}
		}

		if (argc >= 5)
		{
			iret = debug_str2uint(argv[4], &temp);
			if (0 != iret)
			{
				printf("power, fmt err, %d\n", iret);
				goto usage;
			}

			/* power : 0,1,2 */
			pcfg->power = temp & 0x3;
		}
		if (argc >= 6)
		{
			iret = debug_str2uint(argv[5], &temp);
			if (0 != iret)
			{
				printf("bandsel, fmt err, %d\n", iret);
				goto usage;
			}
			/* bandsel : 0,1,2 */
			pcfg->bandsel = temp & 0x1;
		}
		flash_save_atbcfg(pctx->ffd);

		/* write to flash */

		return 0;
	}

	if (0 == strcmp(argv[1], "tn"))
	{
		pcfg->role = 2;
		/* write to flash */
		flash_save_atbcfg(pctx->ffd);
		return 0;
	}

usage:
	printf("usage:\n");
	printf("%s cn <mode> <tx-gain> <power> <bandsel>\n", argv[0]);
	printf("%s tn\n", argv[0]);
	return 0;
}

extern int entry_sysmgr(void *arg);
extern int entry_phbx(void *arg);
extern int entry_resmgr(void *parg);
//串口命令保存资源配置
static int debug_cmd_save_res_cfg(void *parg, int argc, const char **argv)
{
	int iret;
	uint32_t temp;
	uint32_t uary[2];

	tmux_context_t *pctx;
	pctx = (tmux_context_t *)parg;

	flash_save_rescfg(pctx->ffd);
	return 0;
}

int debug_cmd_cfg_pre_res(void *parg, int argc, const char **argv)
{
    int i;
    int iret;
    uint32_t temp;
    uint8_t nodeid;
    uint8_t num;

    tmux_context_t *pctx;
    pctx = (tmux_context_t *)parg;

    /**/
    if (argc < 5)
    {
        goto usage;
    }

    /**/
    iret = debug_str2uint(argv[1], &temp);
    if (iret != 0)
    {
        printf("resid fmt err\n");
        goto usage;
    }
    pre_res.num = (uint8_t)temp;

    /**/
    iret = debug_str2uint(argv[2], &temp);
    if (iret != 0)
    {
        printf("chan fmt err\n");
        goto usage;
    }
    pre_res.chan = (uint8_t)temp;

    iret = debug_str2uint(argv[3], &temp);
    if (iret != 0)
    {
        printf("freq fmt err\n");
        goto usage;
    }
    pre_res.freq = (uint8_t)temp;
    iret = debug_str2uint(argv[4], &temp);
    if (iret != 0)
    {
        printf("len fmt err\n");
        goto usage;
    }
    pre_res.len = (uint8_t)temp;
    if((pre_res.len)*(pre_res.num)/(pre_res.freq) > 56)
    {
    	printf("***************************************************************set failed \n");
       	printf("***************************************************************set failed \n");
		printf("***************************************************************set failed \n");
    	return 0;
    }
    flash_save_pre_res(pctx->ffd);
    return 0;
usage:
    printf("usage: %s <node_num>  <chan> <freq> <len> \n", argv[0]);
    return 0;
}

int debug_cmd_cfg_clr_pre_res(void *parg, int argc, const char **argv)
{
    int i;
    int iret;
    uint32_t temp;
    uint8_t nodeid;
    uint8_t num;

    tmux_context_t *pctx;
    pctx = (tmux_context_t *)parg;

    /**/
    if (argc != 1)
    {
        goto usage;
    }

    /**/
  
    flash_erase_pre_res(pctx->ffd);
    return 0;
usage:
    printf("usage: %s \n", argv[0]);
    return 0;
}
/**
 * @brief 擦除程序并重启，可以通过rz f命令烧写程序
 *
 * @param parg
 * @param argc
 * @param argv
 * @return int
 */

static int debug_cmd_reburn(void *parg, int argc, const char **argv)
{

	uint32_t uary[2];
	tmux_context_t *pctx;
	pctx = (tmux_context_t *)parg;

	/* flash, unlock */
	uary[0] = 0;
	ioctl(pctx->ffd, 2, sizeof(uint32_t), &uary[0]);
	/* flash, erase, ofs = 2M, 4K */
	uary[0] = 0;
	uary[1] = 0x1000;
	ioctl(pctx->ffd, 3, 2 * sizeof(uint32_t), &uary[0]);
	uary[0] = 0x80000;
	uary[1] = 0x1000;
	ioctl(pctx->ffd, 3, 2 * sizeof(uint32_t), &uary[0]);
	// reboot(0);
	soc_reboot();

	return 0;
}
static int debug_cmd_smac(void *parg, int argc, const char **argv)
{
	int iret;
	tmux_context_t *pctx;
	uint32_t temp;
	uint8_t *pcfg;
	uint8_t mac[MAC_LEN] = {0};
	int i = 0;
	/**/
	pctx = (tmux_context_t *)parg;
	pcfg = get_maccfg();
	/**/
	if( argc != 7 )
	{
		goto usage;
	}
	for (i = 0; i < MAC_LEN; i++)
	{
		iret = debug_str2uint(argv[1 + i], &temp);
		mac[i] = (uint8_t)temp;
		if (0 != iret)
		{
			printf("set mac err, %d\n", iret);
			goto usage;	
		}
	}
	memset(pcfg, 0, MAC_LEN);
	memcpy(pcfg, mac, MAC_LEN);
	/* write to flash */
	flash_save_maccfg(pctx->ffd, pcfg, MAC_LEN);
	return 0;

usage:
	printf("usage:\n");
	printf("%s <MAC>(6bytes)\n", argv[0]);
	return 0;
}
void set_ap_gpio(uint32_t gpio, uint32_t level);
void uimsg_proc(tmux_context_t *pctx)
{
	int iret;
	uint32_t farg[2];
	uint8_t msg[128];
	uint32_t uary[2];
	atbcfg_t *pcfg;
	while (1)
	{
		iret = read(pctx->msguifd, msg, 128);
		if (iret < 0)
		{
			break;
		}
		if (0 == memcmp("cfg", msg, 3))
		{
			flash_save_debugcfg(pctx->ffd, &msg[4], 4);
		}
		if (0 == memcmp("svd", msg, 3))
		{
			switch (atbui_cmd.dev_set_buf[0])
			{
			case E_CFG_RS4850:
				memcpy(&devcfg.rs485cfg[0], &(atbui_cmd.dev_set_buf[4]), sizeof(rs485cfg_t));
				break;
			case E_CFG_RS4851:
				memcpy(&devcfg.rs485cfg[1], &(atbui_cmd.dev_set_buf[4]), sizeof(rs485cfg_t));
				break;
			case E_CFG_RS4852:
				memcpy(&devcfg.rs485cfg[2], &(atbui_cmd.dev_set_buf[4]), sizeof(rs485cfg_t));
				break;
			case E_CFG_CAN0:
				memcpy(&devcfg.cancfg[0], &(atbui_cmd.dev_set_buf[4]), sizeof(cancfg_t));
				break;
			case E_CFG_CAN1:
				memcpy(&devcfg.cancfg[1], &(atbui_cmd.dev_set_buf[4]), sizeof(cancfg_t));
				break;
			case E_CFG_CAN2:
				memcpy(&devcfg.cancfg[2], &(atbui_cmd.dev_set_buf[4]), sizeof(cancfg_t));
				break;
			case E_CFG_IOM:
				memcpy(&devcfg.uiuocfg[0], &(atbui_cmd.dev_set_buf[4]), sizeof(uiuocfg_t) * 12);
				break;
			case E_CFG_NODE:
				memcpy(&devcfg.nodecfg, &(atbui_cmd.dev_set_buf[4]), sizeof(nodeusercfg_t));
				break;
				/**
				 * @brief 端口号36代表重启设备
				 *
				 */
			case E_CFG_REBOOT:
				soc_reboot();
				break;
			case E_CFG_LED_ON: 
				set_ap_gpio(14,0);
				break;
			case E_CFG_LED_OFF: 
				set_ap_gpio(14,1);
				break;
			default:
				break;
			}
			flash_save_devcfg(pctx->ffd, &devcfg, sizeof(devcfg_t));
		}
		else if (0 == memcmp("otastart", msg, 8))
		{
			farg[0] = 0;
			ioctl(pctx->ffd, 2, sizeof(uint32_t), farg);
			LOG_OUT(LOG_DBG, "erase start.\n");
			farg[0] = APP_FLASH_UPGRADE_ADDR;
			farg[1] = APP_FLASH_UPGRADE_SIZE;
			iret = ioctl(pctx->ffd, 3, 8, farg);
			LOG_OUT(LOG_DBG, "erase finish %d.\n", iret);
		}
		else if (0 == memcmp("otadata", msg, 7))
		{
			farg[0] = APP_FLASH_UPGRADE_ADDR + (atbui_cmd.ota_data_sn - 2) * 1024;
			ioctl(pctx->ffd, 0, sizeof(uint32_t), farg);
			iret = write(pctx->ffd, &atbui_cmd.ui_dat_buf[4], 1024);
			if (iret != 1024)
			{
				LOG_OUT(LOG_DBG, "ota write flash err.\n");
			}
		}
		else if (0 == memcmp("otareply", msg, 9))
		{
			if (msg[9] == atbui_cmd.ota_reply_cmd)
			{
				ui_eth_send(pctx->efd, atbui_cmd.ota_reply_buf, atbui_cmd.ota_reply_len);
			}
		}
		else if (0 == memcmp("devreply", msg, 8))
		{
			// debug_dump_hex(atbui_cmd.dev_set_buf,DEV_SET_SIZE);
			// memcpy(&atbui_cmd.ota_reply_buf[2], atbui_cmd.dev_set_buf, DEV_SET_SIZE);
			// ui_eth_send(pctx->efd, atbui_cmd.ota_reply_buf, DEV_SET_SIZE);
		}
		else if (0 == memcmp("dynband", msg, 7))
		{
			flash_save_atbcfg(pctx->ffd);
			// debug_dump_hex(atbui_cmd.dev_set_buf,DEV_SET_SIZE);
			// memcpy(&atbui_cmd.ota_reply_buf[2], atbui_cmd.dev_set_buf, DEV_SET_SIZE);
			// ui_eth_send(pctx->efd, atbui_cmd.ota_reply_buf, DEV_SET_SIZE);
		}
		else
		{
			LOG_OUT(LOG_INFO, "unknown ui msg.\n");
		}
	}
}


int tty_mux(void *parg)
{
	int iret;
	int ufd; /* uart 	*/
	int tfd; /* ptm 		*/
	int efd; /* /eth/if	*/
	int ffd; /* /flash 	*/
	int mfd; /* /timer 	*/
	int msguifd;

	uint32_t rfds;
	uint32_t wfds;
	tmux_context_t ctx;
	atbcfg_t *pcfg;
	uint32_t legid;

	/* sysinfo, malloc init */
	uheap_init();

	/**/
	tls_set(&ctx);
	ufd = open("/uart/0", 0);
	ctx.cmctx.fd_stdio = ufd;
	con_init(ufd);

	/**/
	if (INCLUDE_DrvFlash)
	{
		ffd = open("/flash", 0);
		printf("mux open flash %d\n", ffd);
		ctx.ffd = ffd;
	}
	if (INCLUDE_DrvNandFlash)
	{
		ffd = open("/nandflash", 0);
		printf("mux open nandflash %d\n", ffd);
		ctx.ffd = ffd;
		flash_load_maccfg(ctx.ffd);
	}

	flash_load_debugcfg(ctx.ffd, &legid, 4); //从flash里面读到测试配置
	flash_load_devcfg(ctx.ffd); //从flash里面读到设备配置
	flash_load_pre_res(ctx.ffd); //从flash里面读到静态带宽资源分配的资源
	pcfg = get_atbcfg();
	/**/
	iret = flash_load_atbcfg(ctx.ffd);//flash里面读到atb配置
	flash_load_rescfg(ctx.ffd); //flash读取资源配置
	if (iret != 0)
	{
		/* default */
		pcfg->role = 2;
		/**/
	}
	
	/* check valid */
	if (pcfg->role == 0)
	{
		printf("cur: role=cn, mode=0x%01x%01x%01x\n", pcfg->msp, pcfg->dmdb, pcfg->umdb);
		printf("txpga=0x%x,power=0x%x,bandsel:0x%x\n", pcfg->txpga, pcfg->power, pcfg->bandsel);
	}
	else if (pcfg->role == 2)
	{
		printf("cur: role=tn\n");
	}
	else
	{
		printf("cur: role=test(%u)\n", pcfg->role);
		/* set bcpu to reset state */
		write_reg(0x540000 + 0x18, 0);
	}

	if ((pcfg->role == 0) || (pcfg->role == 2))
	{
		// create a thread to complete system initialization
		thread_t *t1;
		t1 = thread_create("sysm", entry_sysmgr, &legid, DEFAULT_PRIORITY, 8192);
		thread_detach(t1);
		thread_resume(t1);

		//Create a thread for application layer use
		thread_t *t2;
		t2 = thread_create("phbx", entry_phbx, &legid, DEFAULT_PRIORITY, 8192);
		thread_detach(t2);
		thread_resume(t2);
	}
	if (pcfg->role == 0)
	{
		/**
		 * @brief CN开启资源管理线程
		 *
		 */
		thread_t *t3;
		t3 = thread_create("resm", entry_resmgr, pcfg, LOW_PRIORITY, 8192);
		thread_detach(t3);
		thread_resume(t3);
	}

	/**/
	hcb_debug_init();
	debug_add_cmd("ver", debug_cmd_version, NULL);
	debug_add_cmd("rreg", debug_cmd_rreg, NULL);
	debug_add_cmd("wreg", debug_cmd_wreg, NULL);
	debug_add_cmd("dump", debug_cmd_dump, NULL);
	debug_add_cmd("heap", debug_cmd_uheap, NULL);
	debug_add_cmd("sysinfo", debug_cmd_sysinfo, NULL);
	debug_add_cmd("reboot", debug_cmd_reboot, NULL);
	debug_add_cmd("cache", debug_cmd_cache, &ctx);
	debug_add_cmd("sw", debug_cmd_switch, &ctx);
	debug_add_cmd("role", debug_cmd_role, &ctx);
	debug_add_cmd("bmem", debug_cmd_bmem, &ctx);
	debug_add_cmd("cfg", debug_cmd_cfg, &ctx);
	debug_add_cmd("ethpkcn", debug_cmd_ethpkcn, &ctx);
#if (INCLUDE_DrvNandFlash == 1)
	debug_add_cmd("smac", debug_cmd_smac, &ctx);
#endif
	if (pcfg->role == 0)
	{
		efd = open("/eth/if", 0);
		LOG_OUT(LOG_INFO, "open efd %d\n", efd);
		ctx.efd = efd;
		epoll_add(efd, -1);

		debug_add_cmd("saveres", debug_cmd_save_res_cfg, &ctx);
		debug_add_cmd("addres", debug_cmd_add_res, &ctx);
		debug_add_cmd("delres", debug_cmd_del_res, &ctx);
		debug_add_cmd("addtrs", debug_cmd_add_trs_res, &ctx);
		debug_add_cmd("addrcv", debug_cmd_add_rcv_res, &ctx);
		debug_add_cmd("deltrs", debug_cmd_del_trs_res, &ctx);
		debug_add_cmd("delrcv", debug_cmd_del_rcv_res, &ctx);
		debug_add_cmd("delcfg", debug_cmd_del_node_config, &ctx);
		debug_add_cmd("rescfg", debug_cmd_res_config, &ctx);
		debug_add_cmd("resclr", debug_cmd_res_config_clear, &ctx);
		debug_add_cmd("nodes", debug_cmd_nodes, &ctx);
		debug_add_cmd("setpre", debug_cmd_cfg_pre_res, &ctx);
		debug_add_cmd("clrpre", debug_cmd_cfg_clr_pre_res, &ctx);
		debug_add_cmd("eth", dbg_cmd_eth, &ctx);
	}

	debug_add_cmd("reburn", debug_cmd_reburn, &ctx);
	/**/
	debug_add_cmd("flash", debug_cmd_flash, &ctx);
	debug_add_cmd("tftp", debug_cmd_tftp, &ctx);

	if (pcfg->role == 0xFF)
	{
		debug_add_cmd("sing", dbg_cmd_sing, &ctx);
		debug_add_cmd("capadc", dbg_cmd_capadc, &ctx);
		debug_add_cmd("gain", dbg_cmd_gain, &ctx);
	}

	/**/
	tfd = open("/ptm", 0);
	printf("open ptm, %d\n", tfd);
	ctx.tfd = tfd;
	ctx.index = -1;

	/* timer */
	mfd = open("/timer", 0);
	printf("open timer, %d\n", mfd);
	ctx.cmctx.fd_timer = mfd;
	timer_queue_init();

	/* dpram */
	msguifd = open("/mesg/uimsg", 0);
	ctx.msguifd = msguifd;
	LOG_OUT(LOG_INFO, "open uimsg, %d\n", msguifd);
	/**/
#ifdef WDT
	wfd = open("/wdt", 0);
	printf("open wdt, %d\n", wfd);
#endif // WDT
	ctx.inprog = 0;

	/**/
	epoll_add(ufd, -1);
	epoll_add(tfd, -1);
	epoll_add(mfd, -1);
	epoll_add(msguifd, -1);

	
	/*Wait for the queue to receive processing*/
	while (1)
	{
		epoll_wait(&rfds, &wfds);

		/**/
		if (rfds & (1 << ufd))
		{
			tmux_fwd_uart2ptm(&ctx);
		}

		/**/
		if (rfds & (1 << tfd))
		{
			/* forward : ptm >> uart */
			tmux_fwd_ptm2uart(&ctx);
		}

		/* timer */
		if (rfds & (1 << mfd))
		{
			timer_queue_event();
		}
		if (pcfg->role == 0)
		{
			if (rfds & (1 << efd))
			{
				atb_ui_process(efd, ffd);
			}
		}

		if (rfds & (1 << msguifd))
		{
			uimsg_proc(&ctx);
		}

#if 0
		/* /eth/if */
		if (rfds & (1 << efd))
		{
			test_dump_eth_if(efd);
		}
#endif
	}

	/**/
	return 0;
}
