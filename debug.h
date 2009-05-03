
#ifndef _DEBUG_H
#define _DEBUG_H

#include <stdarg.h>
#include <stdio.h>

//static int dbg=10;
#define dbg 10



/*
void db_printf(int db, const char *fmt, ... )
{
    va_list vl;
    va_start(vl, fmt );


    if(dbg > db)
    {
        fprintf(stderr, fmt, vl);
    }

    va_end(vl);
}*/


#if 1
#define db_printf(lvl, ...)	\
	if (lvl < dbg )  {		\
		fprintf(stdout, __VA_ARGS__ ); \
	}
#else
#define db_printf(lvl, ...)
#endif


#endif
