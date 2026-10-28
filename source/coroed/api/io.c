#include "io.h"

#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "coroed/sched/io.h"

ssize_t task_read(struct task* caller, int fd, void* buffer, size_t size) {
  for (;;) {
    ssize_t result = read(fd, buffer, size);
    if (result >= 0) {
      return result;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      io_wait(caller, fd, EPOLLIN);
      continue;
    }
    return -1;
  }
}

ssize_t task_write(struct task* caller, int fd, const void* buffer, size_t size) {
  for (;;) {
    ssize_t result = write(fd, buffer, size);
    if (result >= 0) {
      return result;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      io_wait(caller, fd, EPOLLOUT);
      continue;
    }
    return -1;
  }
}

