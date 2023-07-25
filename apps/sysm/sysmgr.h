
#pragma once

typedef void (*sysm_cbk_f)(void *parg, uint8_t *tbuf);

int sysmgr_init(int tfd);
int sysmgr_send(uint8_t *tbuf, sysm_cbk_f scbk, void *parg, int tcnt);
int sysmgr_proc_event(void);
void update_power_ratio(uint8_t power, uint8_t txpga);

/* �ǽ���ʽ�ĵ�����Ϣ, HCB --> ACPU  */
int sysmgr_reg_cbk(sysm_cbk_f scbk, void *parg);
