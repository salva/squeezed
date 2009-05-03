
#ifndef DIRENT_H
#define DIRENT_H

/**
 * \ingroup win32
 *@{
 */

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


#define DT_DIR FILE_ATTRIBUTE_DIRECTORY
#define DT_REG FILE_ATTRIBUTE_NORMAL


struct dirent
{
//	char d_name[FILENAME_MAX+1];
	char			*d_name;
	unsigned long	 d_type;
};

typedef struct DIR DIR;

/** Open a directory for listing */
DIR * opendir ( const char * dirname );

/** get a single entry of a directory */
struct dirent * readdir ( DIR * dir );

/** close the directory */
int closedir ( DIR * dir );

/** rewind to the beginning of the directory */
void rewinddir ( DIR * dir );


#ifdef __cplusplus
}
#endif 

/*@}
 *end of doxygen group
 */


#endif // DIRENT_H
