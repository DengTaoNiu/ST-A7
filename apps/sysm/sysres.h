

#pragma once
#include "sysent.h"

/**/
int sysres_init(void);

/* sysmgr event */
void test_dump_sysm(void *parg, uint8_t *tbuf);

int sysres_srv_add_res_id(void *parg, int ilen, void *ibuf);
int sysres_srv_add_trs_res(void *parg, int ilen, void *ibuf);
int sysres_srv_add_rcv_res(void *parg, int ilen, void *ibuf);
int sysres_srv_del_res(void *parg, int ilen, void *ibuf);
int sysres_srv_bcpu_rtdata(void *parg, int ilen, void *ibuf);
int sysres_srv_debug_get(void *parg, int ilen, void *ibuf);
int sysres_srv_device_get(void *parg, int ilen, void *ibuf);
int sysres_srv_debug_set(void *parg, int ilen, void *ibuf);
int sysres_srv_ota_start(void *parg, int ilen, void *ibuf);
int sysres_srv_ota_data(void *parg, int ilen, void *ibuf);
int sysres_srv_ota_end(void *parg, int ilen, void *ibuf);
int sysres_srv_device_set(void *parg, int ilen, void *ibuf);
int sysres_srv_bps_get(void *parg, int ilen, void *ibuf);
int sysres_srv_dyn_set(void *parg, int ilen, void *ibuf);
int sysres_srv_ui_data(void *parg, int ilen, void *ibuf);
int sysres_srv_tn_info(void *parg, int ilen, void *ibuf);

int rpc_ota_start();
int rpc_ota_data();
int rpc_ota_end();
int rpc_get_atbsig(int nodeid);
int rpc_get_debug(int nodeid);
int rpc_get_device(int nodeid, sysm_context_t *pctx);
int rpc_set_device(int nodeid);
int rpc_get_bps(int nodeid);
int rpc_set_dyn(int nodeid, uint8_t dynband);
int rpc_ui_send(int nodeid,uint8_t *pbuf,uint32_t tlen);
int rpc_tn_send(int nodeid, uint8_t *pbuf, uint32_t tlen);


/**/
int debug_cmd_res(void *parg, int argc, const char **argv);
int debug_cmd_nodes(void *parg, int argc, const char **argv);

int debug_cmd_add_res(void *parg, int argc, const char **argv);
int debug_cmd_del_res(void *parg, int argc, const char **argv);
int debug_cmd_add_trs_res(void *parg, int argc, const char **argv);
int debug_cmd_add_rcv_res(void *parg, int argc, const char **argv);
int debug_cmd_del_trs_res(void *parg, int argc, const char **argv);
int debug_cmd_del_rcv_res(void *parg, int argc, const char **argv);

int debug_cmd_del_node_config(void *parg, int argc, const char **argv);

int debug_cmd_test(void *parg, int argc, const char **argv);
