#pragma once

#include <stddef.h>
#include <sys/types.h>

struct task;

ssize_t task_read(struct task* caller, int fd, void* buffer, size_t size);

ssize_t task_write(struct task* caller, int fd, const void* buffer, size_t size);
