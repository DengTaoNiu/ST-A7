
#include "stdint.h"
#include "stdlib.h"

#include "dlist.h"
#include "sys_api.h"
#include "debug.h"
#include "hcb_comn.h"

extern void test_invalid_cache(uintptr_t padr);

int debug_cmd_cache(void *pctx, int argc, const char **argv)
{
	int iret;
	uint32_t temp;

	/**/
	if (argc != 2)
	{
		goto usage;
	}

	/**/
	iret = debug_str2uint(argv[1], &temp);
	if (iret != 0)
	{
		printf("idx, fmt err, %d\n", iret);
		goto usage;
	}

	/**/
	test_invalid_cache((uintptr_t)temp);

usage:
	printf("usage: %s <padr>\n", argv[0]);
	return 0;
}

int debug_cmd_sysinfo(void *pctx, int argc, const char **argv)
{
	int iret;
	uint32_t temp;
	uint8_t u8_ary[32];

	/**/
	if (argc != 2)
	{
		goto usage;
	}

	/**/
	iret = debug_str2uint(argv[1], &temp);
	if (iret != 0)
	{
		printf("idx, fmt err, %d\n", iret);
		goto usage;
	}

	/**/
	iret = sysinfo((int)temp, u8_ary);
	printf("ret, %d\n", iret);
	if (iret <= 0)
	{
		return 0;
	}

	/**/
	debug_dump_hex(u8_ary, iret);
	return 0;

usage:
	printf("usage: %s <idx>\n", argv[0]);
	return 0;
}

int debug_cmd_reboot(void *pctx, int argc, const char **argv)
{
	//reboot(0);
	write_reg((BASE_BB_DUALRAM + 8184), 0xaa55aa00);
	soc_reboot();
	return 0;
}

int debug_cmd_uheap(void *pctx, int argc, const char **argv)
{
	size_t sused;
	size_t sfree;

	/**/
	uheap_info(&sused, &sfree);
	printf("used: %u, freed: %u\n", sused, sfree);
	return 0;
}

int debug_cmd_dump(void *pctx, int argc, const char **argv)
{
	int iret;
	uint32_t addr;
	uint32_t tcnt;

	/**/
	if (argc != 3)
	{
		goto usage;
	}

	/**/
	iret = debug_str2uint(argv[1], &addr);
	if (iret != 0)
	{
		printf("addr fmt error..%d.. \n", iret);
		goto usage;
	}

	/**/
	iret = debug_str2uint(argv[2], &tcnt);
	if (iret != 0)
	{
		printf("cnt fmt error..%d.. \n", iret);
		return 2;
	}

	/**/
	printf("dump memory, 0x%08lx, %lu\n", addr, tcnt);
	debug_dump_hex((uint8_t *)(uintptr_t)addr, (int)tcnt);

usage:
	printf("usage: %s <addr> <cnt>\n", argv[0]);
	return 0;
}

int debug_cmd_wreg(void *pctx, int argc, const char **argv)
{
	int iret;
	uint32_t addr;
	uint32_t value;

	if (argc != 3)
	{
		goto usage;
	}

	/**/
	iret = debug_str2uint(argv[1], &addr);
	if (iret != 0)
	{
		printf("addr fmt error..%d.. \n", iret);
		goto usage;
	}

	/**/
	iret = debug_str2uint(argv[2], &value);
	if (iret != 0)
	{
		printf("value fmt error..%d.. \n", iret);
		goto usage;
	}

	/**/
	printf("write reg : %08lx << %08lx\n", addr, value);
	write_reg((uintptr_t)addr, value);
	return 0;

usage:
	printf("usage: %s <addr> <value>\n", argv[0]);
	return 0;
}

int debug_cmd_rreg(void *pctx, int argc, const char **argv)
{
	int iret;
	uint32_t temp;
	uintptr_t addr;
	int line;
	int cnt;
	int i;

	/**/
	if (argc < 2)
	{
		goto usage;
	}

	/**/
	iret = debug_str2uint(argv[1], &temp);
	if (iret != 0)
	{
		printf("addr fmt error..%d.. \n", iret);
		goto usage;
	}

	addr = temp;
	//addr = addr & 0xFFFFfffc;
	addr = addr & 0xFFFFfffc;

	/**/
	cnt = 1;
	if (argc >= 3)
	{

		iret = debug_str2uint(argv[2], &temp);
		if (iret != 0)
		{
			printf("num fmt error..%d.. \n", iret);
			goto usage;
		}

		cnt = (int)temp;
	}

	/**/
	printf("dump register, 0x%08lx, %d\n", addr, cnt);

	line = cnt >> 2;
	for (i = 0; i < line; i++)
	{
		temp = read_reg(addr);
		printf("0x%08lx:  %08lX", addr, temp);
		addr += 4;

		temp = read_reg(addr);
		printf("  %08lX", temp);
		addr += 4;

		temp = read_reg(addr);
		printf("  %08lX", temp);
		addr += 4;

		temp = read_reg(addr);
		printf("  %08lX\n", temp);
		addr += 4;
	}

	/**/
	cnt = cnt & 0x3;
	if (cnt > 0)
	{
		printf("0x%08lx:", addr);

		for (i = 0; i < cnt; i++)
		{
			temp = read_reg(addr);
			printf("  %08lX", temp);
			addr += 4;
		}

		printf("\n");
	}

usage:
	printf("usage: %s <addr> <num>\n", argv[0]);
	return 0;
}

int debug_cmd_version(void *parg, int argc, const char **argv)
{
	/**/
	printf("\tsoft_ver: %s,%s\n", __DATE__, __TIME__);
	return 0;
}

void bootmem_info(intptr_t *pptr, size_t *psiz);
int debug_cmd_bmem(void *parg, int argc, const char **argv)
{
	uint32_t ptr;
	uint32_t siz;
	bootmem_info(&ptr, &siz);
	/**/
	printf("\tbmem: 0x%08x,0x%08x\n", ptr, siz);
	return 0;
}

int debug_cmd_cfg(void *parg, int argc, const char **argv){

	write_reg(0x701ff8,0xaa55aa00);
	write_reg(0x701ffc,0x0);
	printf("DYN CFG OK\n");
	return 0;
}

static uint32_t ethpkcnarr[18] = 
{
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0
};
uint32_t *ethpkcn = ethpkcnarr;

int debug_cmd_ethpkcn(void *parg, int argc, const char **argv)
{
	printf("eth in total:drop+if+sw  %d : %d\n", ethpkcnarr[0], ethpkcnarr[1]+ethpkcnarr[2]+ethpkcnarr[3]);
	printf("eth in drop              %d\n", ethpkcnarr[1]);
	printf("eth in if                %d\n", ethpkcnarr[2]);
	printf("eth in sw                %d\n\n", ethpkcnarr[3]);

	printf("pack in  total           %d\n", ethpkcnarr[4]);
	printf("pack out total:send+drop %d : %d\n", ethpkcnarr[5], ethpkcnarr[6]+ethpkcnarr[7]);
	printf("pack out send            %d\n", ethpkcnarr[6]);
	printf("pack out drop            %d\n", ethpkcnarr[7]);
	printf("pack out send big        %d\n\n", ethpkcnarr[8]);

	printf("unpack in big            %d\n", ethpkcnarr[9]);
	printf("unpack in big drop       %d\n", ethpkcnarr[10]);
	printf("unpack in little         %d\n", ethpkcnarr[11]);
	printf("unpack out send          %d\n", ethpkcnarr[12]);
	printf("unpack out drop          %d\n\n", ethpkcnarr[13]);

	printf("eth out in               %d\n", ethpkcnarr[14]);
	printf("eth out send             %d\n", ethpkcnarr[15]);
	printf("eth out send drop        %d\n", ethpkcnarr[16]);
	printf("eth out send retry       %d\n", ethpkcnarr[17]);
	return 0;
}