#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>

#include "types.h"

#define RANDOM_DEVICE "/dev/urandom"

void my_printf(const char* format,...) {
    va_list args;
    char buffer[1024]; // 1022 characters is enough for any logging message.
    buffer[1023] = '\0';
    buffer[1022] = '\0';

    va_start(args, format);
    int size = vsnprintf(buffer, 1022, format, args);
    va_end(args);
    buffer[size] = '\n';

    if (size >= 0)
        write(STDERR_FILENO,buffer,size+1);
}

canary_t canary_generate() {
    // I have to execute 3 syscalls to generate a single canary :(.
    // I can either do batching or just ignore this performance optimization if the mmap syscall executes way slower than the the following consecutive syscalls. (i can always do some benchmark to check that)
    canary_t canary;
    fd_t fd = open(RANDOM_DEVICE,O_RDONLY | O_CLOEXEC);
    read(fd,&canary,sizeof(canary_t)); // The generate canary might fail here in some edge cases.
    // Should i implement errno return values instead of just returning -1 when something went wrong ?
    close(fd);
    return canary;
}
