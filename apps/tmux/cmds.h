
#pragma  once


int debug_cmd_rreg( void * pctx, int argc, const char **argv );
int debug_cmd_wreg( void * pctx, int argc, const char **argv );

int debug_cmd_dump( void * pctx, int argc, const char **argv );
int debug_cmd_uheap( void * pctx, int argc, const char **argv );
int debug_cmd_sysinfo( void * pctx, int argc, const char **argv );
int debug_cmd_reboot( void * pctx, int argc, const char **argv );

int debug_cmd_cache( void * pctx, int argc, const char **argv );
int debug_cmd_version( void * parg, int argc, const char **argv );
int debug_cmd_bmem(void *parg, int argc, const char **argv);
int debug_cmd_cfg(void *parg, int argc, const char **argv);

int debug_cmd_ethpkcn(void *parg, int argc, const char **argv);