#include "io.h"
#include <stdbool.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include "schedy.h"

#include <stdatomic.h>
#include "kthread.h"

static const int WAIT_PERIOD = 100;
static const int EVENT_COUNTER = 1;

struct io_waiter {
    struct task* task;
    int fd;
};

static int epoll_fd = -1;

static struct kthread io_thread;
static atomic_bool io_stopping;



void io_wait(struct task* caller, int fd, uint32_t events) {
    struct io_waiter waiter = {caller, fd}; 

    struct epoll_event event = {0};
    event.events = events | EPOLLONESHOT;
    event.data.ptr = &waiter;

    int s = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    assert(s == 0);

    task_park(caller);
}

void io_poll(void) {
    struct epoll_event event = {0};

    int count = epoll_wait(epoll_fd, &event, EVENT_COUNTER, WAIT_PERIOD);
    if (count == 0) {
        return;
    }
    if (count == -1 && errno == EINTR) {
        return;
    }
    assert(count == 1);


    struct io_waiter* waiter = event.data.ptr;
    int s = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, waiter->fd, NULL);
    assert(s == 0);
    task_wake(waiter->task);
}

int io_loop(void* argument) {
    (void)argument;

    while (!atomic_load(&io_stopping)) {
        io_poll();
    }
    return 0;
}

void io_init() {
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    assert(epoll_fd != -1);

    atomic_store(&io_stopping, false);
    
    enum kthread_status status = kthread_create(&io_thread, io_loop, NULL);
    assert(status == KTHREAD_SUCCESS);
}

void io_destroy() {
    atomic_store(&io_stopping, true);
    enum kthread_status status = kthread_join(&io_thread);
    assert(status == KTHREAD_SUCCESS);

    int s = close(epoll_fd);
    assert(s == 0);
    epoll_fd = -1;
}
