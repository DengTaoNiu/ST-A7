/*********************************************************************************
*@file    : phb1_pwm.c
*@author  :  
*@date    : 2021-10-12
*@brief 
*@note 
*********************************************************************************/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "dlist.h"
#include "hcb_comn.h"
#include "pkt_api.h"
#include "sys_api.h"
#include "debug.h"
#include "phb1.h"
#include "compare.h"

int phb_cmd_pwm_config(void *parg, int argc, const char **argv)
{
	int iret;
	phb_context_t *pctx;
	uint32_t temp;
	dcomp_config_t param;

	/**/
	pctx = (phb_context_t *)parg;

	if (argc < 5)
	{
		/**/
		goto usage;
	}

	iret = debug_str2uint(argv[1], &temp);
	if (0 != iret)
	{
		printf("chan arg err\n");
		goto usage;
	}
	param.chan = temp&3;
	iret = debug_str2uint(argv[2], &temp);
	if (0 != iret)
	{
		printf("polarity arg err\n");
		goto usage;
	}
	param.polarity = temp&1;
	iret = debug_str2uint(argv[3], &temp);
	if (0 != iret)
	{
		printf("period arg err\n");
		goto usage;
	}
	param.period = temp;
	iret = debug_str2uint(argv[4], &temp);
	if (0 != iret)
	{
		printf("duty_cycle arg err\n");
		goto usage;
	}
	param.duty_cycle = temp;
	/**/
	iret = ioctl(pctx->pwmfd, 0, sizeof(dcomp_config_t), &param);
	if (0 != iret)
	{
		printf("ioctl fail = %d\n", iret);
		return 0;
	}

	/**/
	printf("write succ\n");
	return 0;

usage:
	printf("usage: %s chan|polarity|period|duty_cycle\n", argv[0]);
	return 0;
}

int phb_cmd_pwm_ctrl(void *parg, int argc, const char **argv)
{
	int iret;
	phb_context_t *pctx;
	uint32_t temp;
	dcomp_ctrl_t param;

	/**/
	pctx = (phb_context_t *)parg;

	if (argc < 3)
	{
		/**/
		goto usage;
	}

	iret = debug_str2uint(argv[1], &temp);
	if (0 != iret)
	{
		printf("chan arg err\n");
		goto usage;
	}
	param.chan = temp&3;
	iret = debug_str2uint(argv[2], &temp);
	if (0 != iret)
	{
		printf("polarity arg err\n");
		goto usage;
	}
	param.en = temp&1;
	/**/
	iret = ioctl(pctx->pwmfd, 1, sizeof(dcomp_ctrl_t), &param);
	if (0 != iret)
	{
		printf("ioctl fail = %d\n", iret);
		return 0;
	}

	/**/
	printf("write succ\n");
	return 0;

usage:
	printf("usage: %s chan|en\n", argv[0]);
	return 0;
}

