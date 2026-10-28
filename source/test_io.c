#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "coroed/api/io.h"
#include "coroed/api/task.h"

enum { IO_ROUNDS = 16 };

struct io_test_context {
  int fds[2];
  size_t received;
};

TASK_DEFINE(io_writer, struct io_test_context, context) {
  const char byte = 'X';
  ssize_t written = task_write(__self, context->fds[1], &byte, sizeof(byte));
  assert(written == 1);
}

TASK_DEFINE(io_reader, struct io_test_context, context) {
  for (size_t round = 0; round < IO_ROUNDS; ++round) {
    char byte = 0;
    ssize_t received;

    // Before starting the writer, the nonblocking pipe must be empty.
    do {
      received = read(context->fds[0], &byte, sizeof(byte));
    } while (received == -1 && errno == EINTR);
    assert(received == -1);
    assert(errno == EAGAIN || errno == EWOULDBLOCK);

    GO(io_writer, context);

    // task_read handles waiting and retrying internally.
    received = task_read(__self, context->fds[0], &byte, sizeof(byte));
    assert(received == 1);
    assert(byte == 'X');
    context->received += 1;
  }
}

enum { IO_PAYLOAD_SIZE = 64 * 1024 };

struct io_write_context {
  int fds[2];
  size_t prefilled;
  size_t written;
  size_t received;
};

TASK_DEFINE(io_bulk_writer, struct io_write_context, context) {
  char payload[IO_PAYLOAD_SIZE];
  memset(payload, 'W', sizeof(payload));

  ssize_t result = task_write(__self, -1, payload, 1);
  assert(result == -1);
  assert(errno == EBADF);

  result = task_write(__self, context->fds[1], payload, 0);
  assert(result == 0);

  // A successful write may transfer only part of the requested buffer.
  while (context->written < sizeof(payload)) {
    size_t remaining = sizeof(payload) - context->written;
    result = task_write(__self, context->fds[1], payload + context->written, remaining);
    assert(result > 0);
    assert((size_t)result <= remaining);
    context->written += (size_t)result;
  }

  // Closing the writer also checks that task_read eventually returns EOF.
  int status = close(context->fds[1]);
  assert(status == 0);
}

TASK_DEFINE(io_bulk_reader, struct io_write_context, context) {
  char buffer[4096];
  ssize_t result = task_read(__self, -1, buffer, sizeof(buffer));
  assert(result == -1);
  assert(errno == EBADF);

  for (;;) {
    result = task_read(__self, context->fds[0], buffer, sizeof(buffer));
    assert(result >= 0);
    if (result == 0) {
      break;
    }
    for (size_t i = 0; i < (size_t)result; ++i) {
      char expected = context->received < context->prefilled ? 'F' : 'W';
      assert(buffer[i] == expected);
      context->received += 1;
    }
  }
  assert(context->received == context->prefilled + IO_PAYLOAD_SIZE);
}

static void test_io_write(void) {
  struct io_write_context context = {0};
  int status = pipe(context.fds);
  assert(status == 0);
  for (size_t i = 0; i < 2; ++i) {
    int flags = fcntl(context.fds[i], F_GETFL);
    assert(flags != -1);
    status = fcntl(context.fds[i], F_SETFL, flags | O_NONBLOCK);
    assert(status == 0);
  }

  // Start with a full pipe, so writing requires the reader to make room.
  char filler[4096];
  memset(filler, 'F', sizeof(filler));
  for (;;) {
    ssize_t result = write(context.fds[1], filler, sizeof(filler));
    if (result == -1 && errno == EINTR) {
      continue;
    }
    if (result == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      break;
    }
    assert(result > 0);
    context.prefilled += (size_t)result;
  }
  assert(context.prefilled > 0);

  tasks_init();
  tasks_submit(io_bulk_writer, &context);
  tasks_submit(io_bulk_reader, &context);
  tasks_start();
  tasks_wait();
  tasks_destroy();

  assert(context.written == IO_PAYLOAD_SIZE);
  assert(context.received == context.prefilled + context.written);
  status = close(context.fds[0]);
  assert(status == 0);
}

void test_io() {
  struct io_test_context context = {0};
  int status = pipe(context.fds);
  assert(status == 0);

  int flags = fcntl(context.fds[0], F_GETFL);
  assert(flags != -1);
  status = fcntl(context.fds[0], F_SETFL, flags | O_NONBLOCK);
  assert(status == 0);

  tasks_init();
  tasks_submit(io_reader, &context);
  tasks_start();
  tasks_wait();
  tasks_destroy();

  assert(context.received == IO_ROUNDS);
  status = close(context.fds[0]);
  assert(status == 0);
  status = close(context.fds[1]);
  assert(status == 0);

  test_io_write();
}
