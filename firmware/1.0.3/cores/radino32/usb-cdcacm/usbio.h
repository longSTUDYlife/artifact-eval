#ifndef USBIO_H
#define USBIO_H


#ifdef __cplusplus
extern "C"
{
#endif


#include <stddef.h>
#include <stdio.h>


size_t uavailable(void);
int uflush(void);
int upeekc(void);
int ugetc(void);
int uputc(int c);
int uputs(const char *str);
int uwrite(const void *buffer, size_t length);


#ifdef __cplusplus
}
#endif


#endif
