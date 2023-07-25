
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "dlist.h"
#include "hcb_comn.h"
#include "pkt_api.h"
#include "sys_api.h"
#include "debug.h"
#include "atimer.h"
#include "phb1.h"

int debug_cmd_esd(void *parg, int argc, const char **argv)
{
    uint32_t iret,inpnum,i,num1,num2; //i : ѭ����ֵ��ipnum ���͵����� ��num1 �������ݵ����� �� num2 �������ݵĳ���
    phb_context_t *pctx; 
    pbuf_t * ipkt; 

    /**/
    if(argc != 4){
        goto usage;
    } 
    /**/
    pctx = (phb_context_t *)parg;
    /**/
    ipkt = pkt_alloc(0);
    if(NULL == ipkt){
		printf( "reply err,alloc fail.\n");
		return -1;
	}
    /**/
    iret = debug_str2uint(argv[1],&inpnum);
    if(iret != 0){
        dprintf(0,"input num is error \r\n");
    }
    iret = debug_str2uint(argv[2],&num1);
    if(iret != 0){
        dprintf(0,"input num1 is error \r\n");
    }
    iret = debug_str2uint(argv[3],&num2);
    if(iret != 0){
        dprintf(0,"input num2 is error \r\n");
    }
    /**/
    for(i = 0; i < num2 + 4; i++){
        ipkt->buf[i] = inpnum;
    }
    ipkt->length = i - 4;
    ipkt->offset = 4;
   
    /**/
    xheader_t *phdr;
    phdr = (xheader_t *)pkt_prepend(ipkt, XHEADER_LEN);
    
    switch(num1)
    {
        case 1:
            phdr->id = XHEADER_ID + 0X01;
        break;
        case 2:
            phdr->id = XHEADER_ID + 0X02;
        break;
        case 3:
            phdr->id = XHEADER_ID + 0X03;
        break;
        case 4:
            phdr->id = XHEADER_ID + 0X04;
        break;
        case 5:
            phdr->id = XHEADER_ID + 0X05;
        break;
    }
    phdr->count = 0x1;
    phdr->length = ipkt->length + 8;

    /**/
    dprintf(0,"send eth pkt pack\r\n ");
    debug_dump_hex(pkt_to_addr(ipkt),ipkt->length);
    
    /**/
    iret = send(pctx->dfd,ipkt);
    if(iret != 1){
        pkt_free(ipkt);
        dprintf(0,"send error %d\r\n",iret);
    }else{
        dprintf(0,"send ok\r\n");
    }

usage:
    dprintf(0,"\r\nbplease input [esd]  [num 0]  [num 1] [num2],num means the number you want to sen\r\n [num 1] 1--eth ; 2--can ; 3--4850 ;4--4851 ;5--4852;[num2] length\r\n");
 
    return 0;
}

int phb_cmd_context(void *parg, int argc, const char **argv)
{
    phb_context_t *pctx;

    /**/
    pctx = (phb_context_t *)parg;

    /**/
    printf(" dfd : %d\n", pctx->dfd);
    printf(" efd : %d\n", pctx->efd);
    printf(" mfd : %d\n", pctx->mfd);
    return 0;
}

int phb_cmd_dptest(void *parg, int argc, const char **argv)
{
    uint8_t *ptr;

    int iret;
    int i;
    int fd;
    phb_context_t *pctx;
    pbuf_t *pkt[10];
    uint64_t curtime;
    /**/
    pctx = (phb_context_t *)parg;

    for (i = 0; i < 10; i++)
    {
        pkt[i] = pkt_alloc(0);
        if (NULL == pkt[i])
        {
            return -1;
        }
        ptr = pkt_append(pkt[i], 88);
        memset(ptr, 0x11, 88);
        curtime = arch_timer_get_current();
        memcpy(ptr, &curtime, sizeof(curtime));
        if (9 == i)
        {
            memcpy(ptr + sizeof(curtime), "rtchan", 6);
        }
        else
        {
            memcpy(ptr + sizeof(curtime), "normal", 6);
        }
    }
    for (i = 0; i < 10; i++)
    {
        if (9 == i)
        {
            iret = send(pctx->rtfd, pkt[i]);
        }
        else
        {
            iret = send(pctx->dfd, pkt[i]);
        }

        if (1 != iret)
        {
            pkt_free(pkt[i]);
        }
    }

    /**/

    return 0;
}
/**
 * @brief Set the gpio object
 *
 * @param gpio
 * @param level
 */
void set_ap_gpio(uint32_t gpio, uint32_t level)
{
    uint32_t io_bit;
    uintptr_t pbase;
    uint32_t temp;
    io_bit = 1 << (gpio & 0x7U);
    pbase = BASE_AP_GPIO0 + 0x8000 * (gpio >> 3);
    write_reg(pbase + GPIOC_MODE, 0);
    temp = read_reg(pbase + GPIOC_MODE);
    write_reg(pbase + GPIOC_MODE, temp | io_bit);

    temp = read_reg(pbase + GPIOC_DQE);
    write_reg(pbase + GPIOC_DQE, temp | io_bit);
    temp = read_reg(pbase + GPIOC_DQ);
    if (level)
    {
        write_reg(pbase + GPIOC_DQ, temp | io_bit);
    }
    else
    {
        write_reg(pbase + GPIOC_DQ, temp & (~io_bit));
    }
}
/**
 * @brief Set the bb gpio object
 *
 * @param gpio
 * @param level
 */
void set_bb_gpio(uint32_t gpio, uint32_t level)
{
    uint32_t io_bit;
    uintptr_t pbase;
    uint32_t temp;
    io_bit = 1 << (gpio & 0x7U);
    pbase = BASE_BB_GPIO0 + 0x8000 * (gpio >> 3);
    write_reg(pbase + GPIOC_MODE, 0);
    temp = read_reg(pbase + GPIOC_MODE);
    write_reg(pbase + GPIOC_MODE, temp | io_bit);

    temp = read_reg(pbase + GPIOC_DQE);
    write_reg(pbase + GPIOC_DQE, temp | io_bit);
    temp = read_reg(pbase + GPIOC_DQ);
    if (level)
    {
        write_reg(pbase + GPIOC_DQ, temp | io_bit);
    }
    else
    {
        write_reg(pbase + GPIOC_DQ, temp & (~io_bit));
    }
}

int get_ap_gpio(uint32_t gpio)
{
    uint32_t io_bit;
    uintptr_t pbase;
    uint32_t temp;
    io_bit = 1 << (gpio & 0x7U);
    pbase = BASE_AP_GPIO0 + 0x8000 * (gpio >> 3);
    write_reg(pbase + GPIOC_MODE, 0);
    temp = read_reg(pbase + GPIOC_MODE);
    write_reg(pbase + GPIOC_MODE, temp & (~io_bit));

    temp = read_reg(pbase + GPIOC_DQE);
    write_reg(pbase + GPIOC_DQE, temp & (~io_bit));
    temp = read_reg(pbase + GPIOC_DR);
    if (temp & io_bit)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
int debug_cmd_gpio(void *parg, int argc, const char **argv)
{
    int iret;
    phb_context_t *pctx;
    uint32_t gpionum;
    uint32_t level;

    /**/

    if (argc != 4)
    {
        goto usage;
    }

    /**/
    pctx = (phb_context_t *)parg;
    /**/
    iret = debug_str2uint(argv[2], &gpionum);
    if (iret != 0)
    {
        printf("gpio num fmt error..%d.. \n", iret);
        goto usage;
    }

    /**/
    iret = debug_str2uint(argv[3], &level);
    if (iret != 0)
    {
        printf("vol level fmt error..%d.. \n", iret);
        goto usage;
    }
    if (0 == strcmp(argv[1], "ap"))
    {
        set_ap_gpio(gpionum, level);
    }
    else if (0 == strcmp(argv[1], "bb"))
    {
        set_bb_gpio(gpionum, level);
    }
    else
    {
        goto usage;
    }
    return 0;

usage:
    printf("usage: %s [ap|bb] gpionum [0|1].\n", argv[0]);
    return 0;
}
