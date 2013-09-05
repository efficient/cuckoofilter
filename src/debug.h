#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdio.h>  /* for perror */

#ifndef DEBUG
//#define DEBUG
#endif

#define debug_level (DEBUG_ERRS | DEBUG_CUCKOO) 

#ifdef DEBUG
//extern unsigned int debug;

/*
 * a combination of DEBUG_ERRS, DEBUG_CUCKOO, DEBUG_TABLE, DEBUG_ENCODE
 */

#define DPRINTF(level, fmt, args...) \
    do { if (debug_level & (level)) fprintf(stdout, fmt , ##args ); } while(0)
#define DEBUG_PERROR(errmsg) \
    do { if (debug_level & DEBUG_ERRS) perror(errmsg); } while(0)

#else

#define DPRINTF(args...)
#define DEBUG_PERROR(args...)

#endif

/*
 * The format of this should be obvious.  Please add some explanatory
 * text if you add a debugging value.  This text will show up in
 * -d list
 */
#define DEBUG_NONE      0x00	// DBTEXT:  No debugging
#define DEBUG_ERRS      0x01	// DBTEXT:  Verbose error reporting
#define DEBUG_CUCKOO    0x02	// DBTEXT:  Messages for cuckoo hashing
#define DEBUG_TABLE     0x04	// DBTEXT:  Messages for table operations
#define DEBUG_ENCODE    0x08	// DBTEXT:  Messages for encoding

#define DEBUG_ALL       0xffffffff

//int set_debug(char *arg);  /* Returns 0 on success, -1 on failure */


#endif /* _DEBUG_H_ */
