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


/*
 * Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_TESTS_DRIVER_CONNECTOR_SESSION_HOLDER_H_
#define PLUGIN_X_TESTS_DRIVER_CONNECTOR_SESSION_HOLDER_H_

#include <cassert>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../../../global_defines.h"
#ifdef MYSQL8
#include "my_macros.h"
#else
#include "my_global.h"
#endif
#include "../../client/mysqlxclient/xconnection.h"
#include "../../client/mysqlxclient/xsession.h"
#include "../formatters/console.h"

struct Connection_options {
  std::string socket;
  std::string host;
  std::string network_namespace;
  int port{0};

  std::string user;
  std::string password;
  std::string schema;

  std::string ssl_mode;
  std::string ssl_ca;
  std::string ssl_fips_mode;
  std::string ssl_ca_path;
  std::string ssl_cert;
  std::string ssl_cipher;
  std::string ssl_key;
  std::string allowed_tls;
  std::int64_t io_timeout{-1};
  std::int64_t session_connect_timeout{-1};
  bool dont_wait_for_disconnect{false};
  bool trace_protocol{false};
  xcl::Internet_protocol ip_mode{xcl::Internet_protocol::V4};
  std::vector<std::string> auth_methods;
  bool compatible{false};
  std::vector<std::string> compression_server_style{"GROUP", "MULTIPLE",
                                                    "SINGLE"};
  std::vector<std::string> compression_client_style{"SINGLE", "MULTIPLE",
                                                    "GROUP"};
  std::vector<std::string> compression_algorithm{"DEFLATE", "LZ4"};
  std::string compression_mode{"DISABLED"};

  bool is_ssl_set() const {
    return !ssl_ca.empty() || !ssl_ca_path.empty() || !ssl_cert.empty() ||
           !ssl_cipher.empty() || !ssl_key.empty();
  }
};

class Session_holder {
 private:
  using Frame_type = PolarXRPC::Notice::Frame::Type;

 public:
  Session_holder(std::unique_ptr<xcl::XSession> session, const Console &console,
                 const Connection_options &options);

  xcl::XSession *get_session();

  void clear_received_messages();
  bool try_get_number_of_received_messages(const std::string message_name,
                                           uint64_t *value) const;
  void remove_notice_handler();

  xcl::XError connect(const bool is_raw_connection);
  xcl::XError reconnect();

 private:
  xcl::XError setup_session();
  xcl::XError setup_connection();
  void setup_ssl();
  void setup_other_options();
  void setup_msg_callbacks();

  xcl::Handler_result trace_send_messages(
      xcl::XProtocol *protocol,
      const xcl::XProtocol::Client_message_type_id msg_id,
      const xcl::XProtocol::Message &msg);

  xcl::Handler_result trace_received_messages(
      xcl::XProtocol *protocol,
      const xcl::XProtocol::Server_message_type_id msg_id,
      const xcl::XProtocol::Message &msg);

  xcl::Handler_result count_received_messages(
      xcl::XProtocol *protocol,
      const xcl::XProtocol::Server_message_type_id msg_id,
      const xcl::XProtocol::Message &msg);

  xcl::Handler_result dump_notices(const xcl::XProtocol *protocol,
                                   const bool is_global, const Frame_type type,
                                   const char *data,
                                   const uint32_t data_length);

  void print_message(const std::string &direction,
                     const xcl::XProtocol::Message &msg);

  xcl::XProtocol::Handler_id m_handler_id{-1};
  std::unique_ptr<xcl::XSession> m_session;
  std::map<std::string, uint64_t> m_received_msg_counters;
  const Console &m_console;
  Connection_options m_options;
  bool m_is_raw_connection{false};
};

#endif  // PLUGIN_X_TESTS_DRIVER_CONNECTOR_SESSION_HOLDER_H_
