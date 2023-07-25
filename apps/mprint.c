
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include "dlist.h"
#include "sys_api.h"
#include "debug.h"

typedef int (*puts_func)(intptr_t arg, const char *ptr, int tcnt);

static int mini_itoa(int value, unsigned int radix, unsigned int uppercase, unsigned int unsig, char *buffer, int zero_pad)
{
    char *pbuffer = buffer;
    int negative = 0;
    int i, len;

    /* No support for unusual radixes. */
    if (radix > 16)
        return 0;

    if (value < 0 && !unsig)
    {
        negative = 1;
        value = -value;
    }

    /* This builds the string back to front ... */
    do
    {
        int digit = value % radix;
        *(pbuffer++) = (digit < 10 ? '0' + digit : (uppercase ? 'A' : 'a') + digit - 10);
        value /= radix;
    } while (value > 0);

    for (i = (pbuffer - buffer); i < zero_pad; i++)
        *(pbuffer++) = '0';

    if (negative)
        *(pbuffer++) = '-';

    *(pbuffer) = '\0';

    /* ... now we reverse it (could do it recursively but will
	 * conserve the stack space) */
    len = (pbuffer - buffer);
    for (i = 0; i < len / 2; i++)
    {
        char j = buffer[i];
        buffer[i] = buffer[len - i - 1];
        buffer[len - i - 1] = j;
    }

    return len;
}

static int mini_itoa64(int64_t value, unsigned int radix, unsigned int uppercase, unsigned int unsig, char *buffer, int zero_pad)
{
    char *pbuffer = buffer;
    int negative = 0;
    int i, len;

    /* No support for unusual radixes. */
    if (radix > 16)
        return 0;

    if (value < 0 && !unsig)
    {
        negative = 1;
        value = -value;
    }

    /* This builds the string back to front ... */
    do
    {
        int digit = value % radix;
        *(pbuffer++) = (digit < 10 ? '0' + digit : (uppercase ? 'A' : 'a') + digit - 10);
        value /= radix;
    } while (value > 0);

    for (i = (pbuffer - buffer); i < zero_pad; i++)
        *(pbuffer++) = '0';

    if (negative)
        *(pbuffer++) = '-';

    *(pbuffer) = '\0';

    /* ... now we reverse it (could do it recursively but will
	 * conserve the stack space) */
    len = (pbuffer - buffer);
    for (i = 0; i < len / 2; i++)
    {
        char j = buffer[i];
        buffer[i] = buffer[len - i - 1];
        buffer[len - i - 1] = j;
    }

    return len;
}

static int mx_vprintf(puts_func pfunc, intptr_t arg, const char *fmt, va_list vst)
{
    char ch;
    int tcnt = 0;
    char tch;
    const char *ptr;
    int ccnt;
    int zero_pad;
    char bf[28];

    while (1)
    {
        ch = *(fmt++);
        if (ch == '\0')
        {
            break;
        }

        /**/
        if (ch != '%')
        {
            ptr = fmt - 1;
            ccnt = 1;
            tcnt += 1;

            /**/
            while (1)
            {
                ch = *(fmt++);
                if (ch == '\0')
                {
                    /* flush */
                    pfunc(arg, ptr, ccnt);
                    goto _final_rtn;
                }

                if (ch == '%')
                {
                    /* flush */
                    pfunc(arg, ptr, ccnt);
                    goto _fmt_swi;
                }

                /**/
                ccnt += 1;
                tcnt += 1;
            }
        }
        else
        {
        _fmt_swi:
            ptr = bf;
            zero_pad = 0;
            ch = *(fmt++);
            if (ch == '\0')
            {
                /* nothing */
                goto _final_rtn;
            }

            if (ch == '0')
            {
                ch = *(fmt++);
                if (ch == '\0')
                {
                    /* nothing */
                    goto _final_rtn;
                }

                if (ch >= '0' && ch <= '9')
                {
                    zero_pad = ch - '0';

                    ch = *(fmt++);
                    if (ch == '\0')
                    {
                        /* nothing */
                        goto _final_rtn;
                    }
                }
            }

            /**/
            if (ch == 'L' )
            {
                ch = *(fmt++);

                /**/
                switch (ch)
                {
                case 'x':
                    ccnt = mini_itoa64(va_arg(vst,uint64_t), 16, 0, 1, bf, zero_pad);
                    break;

                case 'X':
                    ccnt = mini_itoa64(va_arg(vst,uint64_t), 16, 1, 1, bf, zero_pad);
                    break;

                case 'd':
                    ccnt = mini_itoa64(va_arg(vst, int64_t), 10, 0, 0, bf, zero_pad);
                    break;

                case 'u':
                    ccnt = mini_itoa64(va_arg(vst, uint64_t), 10, 0, 1, bf, zero_pad);
                    break;
                default:
                    bf[0] = ch;
                    ccnt = 1;
                    break;
                }
            }else if (ch == 'l' || ch == 'L' || ch == 'h' || ch == 'H')
            {
                ch = *(fmt++);

                /**/
                switch (ch)
                {
                case 'p':
                case 'x':
                    ccnt = mini_itoa(va_arg(vst, unsigned int), 16, 0, 1, bf, zero_pad);
                    break;

                case 'X':
                    ccnt = mini_itoa(va_arg(vst, unsigned int), 16, 1, 1, bf, zero_pad);
                    break;

                case 'd':
                    ccnt = mini_itoa(va_arg(vst, int), 10, 0, 0, bf, zero_pad);
                    break;

                case 'u':
                    ccnt = mini_itoa(va_arg(vst, unsigned int), 10, 0, 1, bf, zero_pad);
                    break;
                default:
                    bf[0] = ch;
                    ccnt = 1;
                    break;
                }
            }
            else
            {

                /**/
                switch (ch)
                {
                case 'p':
                case 'x':
                    ccnt = mini_itoa(va_arg(vst, unsigned int), 16, 0, 1, bf, zero_pad);
                    break;

                case 'X':
                    ccnt = mini_itoa(va_arg(vst, unsigned int), 16, 1, 1, bf, zero_pad);
                    break;

                case 'd':
                    ccnt = mini_itoa(va_arg(vst, int), 10, 0, 0, bf, zero_pad);
                    break;

                case 'u':
                    ccnt = mini_itoa(va_arg(vst, unsigned int), 10, 0, 1, bf, zero_pad);
                    break;
                case 'U':
                    ccnt = mini_itoa64(va_arg(vst, uint64_t), 16, 0, 1, bf, zero_pad);
                    break;
                case 'c':
                    bf[0] = va_arg(vst, int);
                    ccnt = 1;
                    break;

                case 'f':
                case 'F':
                case 'g':
                case 'G':
                    va_arg(vst, double);
                    bf[0] = '.';
                    ccnt = 1;
                    break;

                case 's':
                    ptr = va_arg(vst, char *);
                    ccnt = strlen(ptr);
                    break;

                default:
                    bf[0] = ch;
                    ccnt = 1;
                    break;
                }
            }

            /**/
            pfunc(arg, ptr, ccnt);
            tcnt += ccnt;
        }
    }

_final_rtn:
    return tcnt;
}

typedef struct _tag_string_blk
{
    char *ptr;
    int ofs;
} string_blk;

static int string_puts(intptr_t arg, const char *ptr, int tcnt)
{
    string_blk *pblk;

    /**/
    pblk = (string_blk *)arg;
    memcpy(pblk->ptr + pblk->ofs, ptr, tcnt);
    pblk->ofs += tcnt;
    return tcnt;
}

int sprintf(char *str, const char *fmt, ...)
{
    int iret;
    string_blk tblk;
    va_list vst;

    /**/
    tblk.ptr = str;
    tblk.ofs = 0;

    /**/
    va_start(vst, fmt);
    iret = mx_vprintf(string_puts, (intptr_t)&tblk, fmt, vst);
    va_end(vst);

    /**/
    return iret;
}

static int stdio_puts(intptr_t arg, const char *ptr, int tcnt)
{
    write((int)arg, ptr, tcnt);
    return tcnt;
}

int printf(const char *fmt, ...)
{
    int iret;
    comn_context *pctx;
    va_list vst;

    /**/
    pctx = (comn_context *)tls_get();

    /**/
    va_start(vst, fmt);
    iret = mx_vprintf(stdio_puts, (intptr_t)(pctx->fd_stdio), fmt, vst);
    va_end(vst);

    /**/
    return iret;
}
