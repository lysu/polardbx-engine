/*****************************************************************************

Copyright (c) 2023, 2024, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/


#ifndef EASY_CONNECTION_H_
#define EASY_CONNECTION_H_

#include <easy_define.h>
#include "easy_io_struct.h"

/**
 * 连接主程序
 */

EASY_CPP_START

// fuction
easy_listen_t *easy_connection_listen_addr(easy_io_t *eio, easy_addr_t addr, easy_io_handler_pt *handler);
void easy_connection_on_wakeup(struct ev_loop *loop, ev_async *w, int revents);
void easy_connection_on_listen(struct ev_loop *loop, ev_timer *w, int revents);
int easy_connection_write_socket(easy_connection_t *c);
int easy_connection_request_process(easy_request_t *r, easy_io_process_pt *process);
uint64_t easy_connection_get_packet_id(easy_connection_t *c, void *packet, int flag);

int easy_connection_send_session_list(easy_list_t *list);
int easy_connection_session_build(easy_session_t *s);
void easy_connection_wakeup_session(easy_connection_t *c);
void easy_connection_destroy(easy_connection_t *c);
int easy_connection_request_done(easy_request_t *c);
int easy_connection_write_again(easy_connection_t *c);
void easy_connection_on_readable(struct ev_loop *loop, ev_io *w, int revents);
void easy_connection_on_writable(struct ev_loop *loop, ev_io *w, int revents);
void easy_connection_reuseport(easy_io_t *eio, easy_listen_t *l, int idx);
void easy_connection_on_accept(struct ev_loop *loop, ev_io *w, int revents);
void easy_connection_on_udpread(struct ev_loop *loop, ev_io *w, int revents);

EASY_CPP_END

#endif

