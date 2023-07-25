

#pragma once



/**/
typedef int (* tftp_file_read_t)( int fd, void *buf, uint32_t tsiz );
typedef int (* tftp_file_write_t)( int fd, const void *buf, uint32_t tsiz );
typedef int (* tftp_file_close_t)( int fd );

/**/
typedef int (* tftp_wrq_open_t)( char * fname, tftp_file_write_t * ppwrite, tftp_file_close_t * ppclose );
typedef int (* tftp_rrq_open_t)( char * fname, tftp_file_read_t * ppread, tftp_file_close_t * ppclose );

/**/
int  tftp_init( uint16_t srvport, tftp_wrq_open_t pwrqop, tftp_rrq_open_t prrqop );


