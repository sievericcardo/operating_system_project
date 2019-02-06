/** 
 *  Library for custom error type that may be generated. More to come as
 *  the time pass by.
 *  As the error is encountered a custom error message and error number is
 *  displayed to the user.
 */

#ifndef ERROR_HANDLING_INCLUDED
#define ERROR_HANDLING_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

extern int errno;

void file_exception();
void scanf_exception();
void argc_exception();
void fork_exception();
void malloc_exception();
void pipe_exception();

#endif
