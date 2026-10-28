#pragma once
#include <stdint.h>

struct task;

void io_wait(struct task* caller, int fd, uint32_t events);


void io_init();


void io_destroy();

