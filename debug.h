
#ifndef _DEBUG_H
#define _DEBUG_H

#include <stdarg.h>
#include <stdio.h>

//static int dbg=10;
#ifdef _DEBUG_
	#define dbg 10
#else
	#define dbg 2	//less verbose on release builds
#endif


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
		fprintf(stderr, __VA_ARGS__ ); \
	}
#else
#define db_printf(lvl, ...)
#endif


#endif
