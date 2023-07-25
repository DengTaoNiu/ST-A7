
#include "compiler.h"
#include "tlsf.h"
#include "sys_api.h"


static tlsf_t  uhp;
static intptr_t  mtx;

int   uheap_init( void )
{
    uint32_t  tary[2];
    
    /**/
    sysinfo( 0, tary );
    uhp = tlsf_create_with_pool( (void *)(uintptr_t)tary[0], tary[1] );

    mtx = mutex_init();
    if ( mtx == 0 )
    {
        return 111;
    }

    /**/
    return 0;
}



void * malloc( size_t tsiz )
{
    void * ptr;

    /**/
    mutex_acquire( mtx, 0xffffffff );
    ptr = tlsf_malloc( uhp, tsiz );
    mutex_release( mtx );

    /**/
    return ptr;
}



void  free( void * ptr )
{
    /**/
    mutex_acquire( mtx, 0xffffffff );
    tlsf_free( uhp, ptr );
    mutex_release( mtx );

    /**/
    return;
}




static void  info_walker( void * ptr, size_t size, int used, void * user )
{
    size_t * pary;

    /**/
    pary = (size_t *)user;

    if ( used )
    {
        pary[0] += size;
    }
    else
    {
        pary[1] += size;
    }

    /**/
    return;
}



void  uheap_info( size_t * pused,  size_t * pfree )
{
    size_t  tary[2];

    /* 0 - used, 1 - free */
    tary[0] = tlsf_size();
    tary[1] = 0;

    /**/
    mutex_acquire( mtx, 0xffffffff );    
    tlsf_walk_pool( tlsf_get_pool(uhp), info_walker, (void*)tary );
    mutex_release( mtx );

    /**/
    *pused = tary[0];
    *pfree = tary[1];
    return;
}
