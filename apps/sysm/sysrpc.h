

#pragma once


typedef int  (* rpc_cbk_f)( void * parg, int ilen, void * ibuf );
typedef int  (* rpc_srv_f)( void * parg, int ilen, void * ibuf );



/**/
int  rpc_init( int qfd );
int  rpc_proc_input( int qfd, uint8_t tnd );


/* request */
int  rpc_request_create( uint8_t dnode, uint32_t srvid, intptr_t * pret );
int  rpc_request_set_callbk( intptr_t req, rpc_cbk_f cbkf, void * parg );
int  rpc_request_set_retry( intptr_t req, int retry, int tmout );
int  rpc_request_send( intptr_t req, void * pmsg, int tlen );


/* service */
int  rpc_service_register( uint32_t srvid, rpc_srv_f cbkf, void * parg );
int  rpc_respond_send( int tlen, void * pmsg );

