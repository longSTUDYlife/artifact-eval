//
// Syscalls for newlib.
//
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "usbio.h"



// TODO: This file overrides I/O related syscalls from the default-syscalls
// because we need them for compatibility reasons. Isn't there a better way of
// performing this?


// We are not dealing with reentrant functions here, so use the errno variable
// instead Newlib's macro. See https://sourceware.org/newlib/libc.html#Stubs.
#undef errno
extern int errno;


#define ERR_FILENO STDERR_FILENO
#define OUT_FILENO STDOUT_FILENO

#define UNUSED(x) ((void)(x))




int _close(int fd)
{
    if (OUT_FILENO == fd || ERR_FILENO == fd)
    {
        // We do not support closing our I/O streams. EIO has been chosen as
        // best match.
        errno = EIO;
        return -1;
    }

    errno = EBADF;
    return -1;
}


int _fstat(int fd, struct stat *sb)
{
    if (OUT_FILENO == fd || ERR_FILENO == fd)
    {
        sb->st_mode = S_IFCHR;
        return 0;
    }

    errno = EBADF;
    return -1;
}


int _isatty(int fd)
{
    if (OUT_FILENO == fd || ERR_FILENO == fd)
    {
        return 1;
    }

    errno = EBADF;
    return 0;
}


int _lseek(int fd, int offset, int whence)
{
    UNUSED(offset);
    UNUSED(whence);

    if (OUT_FILENO == fd || ERR_FILENO == fd)
    {
        // Seeking is not supported
        errno = ESPIPE;
        return -1;
    }

    errno = EBADF;
    return -1;
}


int _read(int fd, char *buffer, int length)
{
    UNUSED(fd);
    UNUSED(buffer);
    UNUSED(length);

    // There is currently no input file descriptor.
    //
    // TODO: Back _read by an appropriate uread function or ugetc.
    errno = EBADF;
    return -1;
}


int _write(int fd, char *buffer, int length)
{
    if (NULL == buffer)
    {
        errno = EFAULT;
        return -1;
    }

    if (OUT_FILENO == fd || ERR_FILENO == fd)
    {
        // Pass data to USB serial interface allowing empty writes.
        return uwrite(buffer, length);
    }

    errno = EBADF;
    return -1;
}
