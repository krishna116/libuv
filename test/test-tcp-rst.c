/* Copyright libuv project and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "task.h"

static uv_tcp_t tcp;
static uv_connect_t connect_req;
static uv_shutdown_t shutdown_req;
static uv_buf_t qbuf;
static int called_alloc_cb;
static int called_connect_cb;
static int called_shutdown_cb;
static int called_close_cb;


static void close_cb(uv_handle_t* handle) {
  ASSERT(handle == (uv_handle_t*) &tcp);
  called_close_cb++;
}


static void alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* buf) {
  buf->base = malloc(size);
  buf->len = size;
  called_alloc_cb++;
}


static void read_cb(uv_stream_t* t, ssize_t nread, const uv_buf_t* buf) {
  ASSERT_PTR_EQ((uv_tcp_t*) t, &tcp);
  ASSERT_EQ(nread, UV_ECONNRESET);

  int fd;
  ASSERT_EQ(0, uv_fileno((uv_handle_t*) t, &fd));
  uv_handle_type type = uv_guess_handle(fd);
  ASSERT_EQ(type, UV_TCP);

  uv_close((uv_handle_t *) t, close_cb);
  free(buf->base);
}


static void connect_cb(uv_connect_t *req, int status) {
  ASSERT_EQ(status, 0);
  ASSERT_PTR_EQ(req, &connect_req);

  /* Start reading from the connection so we receive the RST in uv__read. */
  ASSERT_EQ(0, uv_read_start((uv_stream_t*) &tcp, alloc_cb, read_cb));

  /* Write 'QSH' to receive RST from the echo server. */
  ASSERT_EQ(qbuf.len, uv_try_write((uv_stream_t*) &tcp, &qbuf, 1));

  called_connect_cb++;
  ASSERT_EQ(called_shutdown_cb, 0);
}


/*
 * This test has a client which connects to the echo_server and receives TCP
 * RST. Test checks that uv_guess_handle still works on a reset TCP handle.
 */
TEST_IMPL(tcp_rst) {
#ifndef _WIN32
  struct sockaddr_in server_addr;
  int r;

  qbuf.base = "QSH";
  qbuf.len = 3;

  ASSERT_EQ(0, uv_ip4_addr("127.0.0.1", TEST_PORT, &server_addr));
  r = uv_tcp_init(uv_default_loop(), &tcp);
  ASSERT_EQ(r, 0);

  r = uv_tcp_connect(&connect_req,
                     &tcp,
                     (const struct sockaddr*) &server_addr,
                     connect_cb);
  ASSERT_EQ(r, 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT_EQ(called_alloc_cb, 1);
  ASSERT_EQ(called_connect_cb, 1);
  ASSERT_EQ(called_shutdown_cb, 0);
  ASSERT_EQ(called_close_cb, 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
#else
  RETURN_SKIP("Unix only test");
#endif
}
