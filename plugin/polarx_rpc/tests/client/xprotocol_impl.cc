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
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "../../global_defines.h"
#ifndef MYSQL8
#define MYSQL_SERVER
#include "sql/sql_class.h"
#endif

#include "xprotocol_impl.h"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#include "errmsg.h"
#include "my_config.h"
#include "my_dbug.h"
#include "mysql_com.h"
#include "sha2.h"

#include "authentication/password_hasher.h"
#include "authentication/sha256_scramble_generator.h"
#include "mysqlxclient/xerror.h"
#include "mysqlxclient/xrow.h"
#include "stream/connection_output_stream.h"
#include "xpriority_list.h"

namespace xcl {

using StringOutputStream = google::protobuf::io::StringOutputStream;
using CodedOutputStream = google::protobuf::io::CodedOutputStream;
using ZeroCopyOutputStream = google::protobuf::io::ZeroCopyOutputStream;
using ZeroCopyInputStream = google::protobuf::io::ZeroCopyInputStream;

const char *const ERR_MSG_INVALID_AUTH_METHOD =
    "Invalid authentication method ";
const char *const ERR_MSG_UNEXPECTED_MESSAGE =
    "Unexpected response received from server, msg-id:";
const char *const ERR_MSG_MESSAGE_NOT_INITIALIZED =
    "Message is not properly initialized: ";
const char *const ER_TEXT_HASHING_FUNCTION_FAILED =
    "Invalid result while calculating hash";
const char *const ER_TEXT_DATA_TOO_LARGE =
    "Messages payload size exceeded the the value that message header can hold";
const char *const ER_TEXT_RECEIVE_HANDLER_FAILED =
    "Aborted by internal callback at received message processing";
const char *const ER_TEXT_NOTICE_HANDLER_FAILED =
    "Aborted by internal callback at send message processing";
const char *const ER_TEXT_RECEIVE_BUFFER_TO_SMALL = "Receive buffer to small";

namespace details {

/**
    Function checks the stream for data available to read.

    This function is useful in cases when a stream consists from
    multiple sub-streams which may implement a data-cache.
    To do i properly it would need to check number of bytes "until
    the limit" on each layer, still we do not have this information
    and it would be complicated to obtain it.
    Instead, try to read the data which should give the same effect.

    Precondition: Top level stream must have a data limiter set.

    @param stream   Protobuf zero output stream

    @return
      @retval == true stream has more data
  */
bool has_data(ZeroCopyInputStream *stream) {
  const void *data;
  int size;

  if (stream->Next(&data, &size)) {
    stream->BackUp(size);
    return true;
  }

  return false;
}

XError make_xerror(const PolarXRPC::Error &error) {
  bool is_fatal = error.severity() == PolarXRPC::Error::FATAL;
  return XError{static_cast<int>(error.code()), error.msg(), is_fatal,
                error.sql_state()};
}

bool is_timeout_error(const XError &error) {
  return (CR_X_READ_TIMEOUT == error.error());
}

class Query_sequencer : public Query_instances {
 public:
  Instance_id instances_fetch_begin() override { return m_last_instance++; }

  void instances_fetch_end() override { ++m_current_instance; }

  bool is_instance_active(const Instance_id id) override {
    return id == m_current_instance;
  }

 private:
  Instance_id m_current_instance{0};
  Instance_id m_last_instance{0};
};

}  // namespace details

Protocol_impl::Protocol_impl(std::shared_ptr<Context> context,
                             Protocol_factory *factory)
    : m_factory(factory), m_context(context) {
  assert(nullptr != factory);
  m_connection = factory->create_connection(context);
  m_query_instances.reset(new details::Query_sequencer);
  m_connection_input_stream.reset(
      new Connection_input_stream(m_connection.get()));
  m_static_recv_buffer.resize(VIO_READ_BUFFER_SIZE);
}

XError Protocol_impl::execute_set_capability(
    const PolarXRPC::Connection::CapabilitiesSet &capabilities_set) {
  auto result = send(capabilities_set);

  if (result) return result;

  return recv_ok();
}

XError Protocol_impl::execute_authenticate(const std::string &user,
                                           const std::string &pass,
                                           const std::string &schema,
                                           const std::string &method) {
  XError error;

  if (method == "PLAIN")
    error = authenticate_plain(user, pass, schema);
  else if (method == "MYSQL41")
    error = authenticate_mysql41(user, pass, schema);
  else if (method == "SHA256_MEMORY")
    error = authenticate_sha256_memory(user, pass, schema);
  else
    return XError(CR_X_INVALID_AUTH_METHOD,
                  ERR_MSG_INVALID_AUTH_METHOD + method);

  return error;
}

std::unique_ptr<XProtocol::Capabilities>
Protocol_impl::execute_fetch_capabilities(XError *out_error) {
  *out_error = send(PolarXRPC::Connection::CapabilitiesGet());

  if (*out_error) return {};

  std::unique_ptr<Message> message(
      recv_id(PolarXRPC::ServerMessages::CONN_CAPABILITIES, out_error));

  if (*out_error) return {};

  return std::unique_ptr<XProtocol::Capabilities>{
      static_cast<PolarXRPC::Connection::Capabilities *>(message.release())};
}

XError Protocol_impl::execute_close() {
  XError error = send(PolarXRPC::Connection::Close());

  if (error) return error;

  error = recv_ok();

  return error;
}

std::unique_ptr<XQuery_result> Protocol_impl::recv_resultset() {
  return m_factory->create_result(shared_from_this(), m_query_instances.get(),
                                  m_context);
}

std::unique_ptr<XQuery_result> Protocol_impl::recv_resultset(
    XError *out_error) {
  if (m_context->m_global_error) {
    *out_error = m_context->m_global_error;

    return {};
  }

  auto result = recv_resultset();

  result->get_metadata(out_error);

  return result;
}

std::unique_ptr<XQuery_result> Protocol_impl::execute_with_resultset(
    const Client_message_type_id mid, const Message &msg, XError *out_error) {
  *out_error = send(mid, msg);

  if (*out_error) return {};

  return recv_resultset(out_error);
}

std::unique_ptr<XQuery_result> Protocol_impl::execute_stmt(
    const PolarXRPC::Sql::StmtExecute &m, XError *out_error) {
  return execute(m, out_error);
}

XError Protocol_impl::authenticate_mysql41(const std::string &user,
                                           const std::string &pass,
                                           const std::string &db) {
  class Mysql41_continue_handler {
   public:
    explicit Mysql41_continue_handler(Protocol_impl *protocol)
        : m_protocol(protocol) {}

    std::string get_name() const { return "MYSQL41"; }

    XError operator()(
        const std::string &user, const std::string &pass, const std::string &db,
        const PolarXRPC::Session::AuthenticateContinue &auth_continue) {
      std::string data;
      std::string password_hash;

      PolarXRPC::Session::AuthenticateContinue auth_continue_response;

      if (pass.length()) {
        password_hash = password_hasher::scramble(
            auth_continue.auth_data().c_str(), pass.c_str());
        password_hash = password_hasher::get_password_from_salt(password_hash);

        if (password_hash.empty()) {
          return XError{CR_UNKNOWN_ERROR, ER_TEXT_HASHING_FUNCTION_FAILED};
        }
      }

      data.append(db).push_back('\0');    // authz
      data.append(user).push_back('\0');  // authc
      data.append(password_hash);         // pass
      auth_continue_response.set_auth_data(data);

      return m_protocol->send(auth_continue_response);
    }

   private:
    Protocol_impl *m_protocol;
  };

  return authenticate_challenge_response<Mysql41_continue_handler>(user, pass,
                                                                   db);
}

XError Protocol_impl::authenticate_sha256_memory(const std::string &user,
                                                 const std::string &pass,
                                                 const std::string &db) {
  class Sha256_memory_continue_handler {
   public:
    explicit Sha256_memory_continue_handler(Protocol_impl *protocol)
        : m_protocol(protocol) {}

    std::string get_name() const { return "SHA256_MEMORY"; }

    XError operator()(
        const std::string &user, const std::string &pass, const std::string &db,
        const PolarXRPC::Session::AuthenticateContinue &auth_continue) {
      PolarXRPC::Session::AuthenticateContinue auth_continue_response;

      auto nonce = auth_continue.auth_data();
      char sha256_scramble[32] = {0};
      if (xcl::generate_sha256_scramble(
              reinterpret_cast<unsigned char *>(sha256_scramble), 32,
              pass.c_str(), pass.length(), nonce.c_str(), nonce.length()))
        return XError{CR_UNKNOWN_ERROR, ER_TEXT_HASHING_FUNCTION_FAILED};

      std::string scramble_hex(2 * 32 + 1, '\0');
      password_hasher::octet2hex(&scramble_hex[0], &sha256_scramble[0], 32);
      scramble_hex
          .pop_back();  // Skip the additional \0 sign added by octet2hex

      std::string data;
      data.append(db).push_back('\0');
      data.append(user).push_back('\0');
      data.append(scramble_hex);
      auth_continue_response.set_auth_data(data);

      return m_protocol->send(auth_continue_response);
    }

   private:
    Protocol_impl *m_protocol;
  };

  return authenticate_challenge_response<Sha256_memory_continue_handler>(
      user, pass, db);
}

XError Protocol_impl::authenticate_plain(const std::string &user,
                                         const std::string &pass,
                                         const std::string &db) {
  XError error;

  {
    PolarXRPC::Session::AuthenticateStart auth;

    auth.set_mech_name("PLAIN");
    std::string data;

    data.append(db).push_back('\0');    // authz
    data.append(user).push_back('\0');  // authc
    data.append(pass);                  // pass

    auth.set_auth_data(data);
    error = send(PolarXRPC::ClientMessages::SESS_AUTHENTICATE_START, auth);
  }

  if (error) return error;

  return recv_id(PolarXRPC::ServerMessages::SESS_AUTHENTICATE_OK);
}

XError Protocol_impl::send(const Header_message_type_id mid,
                           const uint8_t *buffer,
                           const std::size_t buffer_length) {
  if (m_context->m_global_error) return m_context->m_global_error;

  union {
    uint8_t header[5];  // Must be properly aligned
    longlong dummy;
  };
  /*
    Use dummy, otherwise g++ 4.4 reports: unused variable 'dummy'
    MY_ATTRIBUTE((unused)) did not work, so we must use it.
  */
  dummy = 0;

  uint32_t *buf_ptr = reinterpret_cast<uint32_t *>(header);
  *buf_ptr = static_cast<uint32>(buffer_length + 1);
#ifdef WORDS_BIGENDIAN
  std::swap(header[0], header[3]);
  std::swap(header[1], header[2]);
#endif
  header[4] = mid;

  if (buffer_length + 1 > std::numeric_limits<uint32>::max())
    return XError{CR_MALFORMED_PACKET, ER_TEXT_DATA_TOO_LARGE};

  /** polarx rpc sid header */
  uint8_t hdr[8];

  uint64_t *ptr = reinterpret_cast<uint64_t *>(hdr);
  *ptr = static_cast<uint64_t>(m_sid);
#ifdef WORDS_BIGENDIAN
  std::swap(hdr[0], hdr[7]);
  std::swap(hdr[1], hdr[6]);
  std::swap(hdr[2], hdr[5]);
  std::swap(hdr[3], hdr[4]);
#endif
  m_connection->write(hdr, 8);

  XError error = m_connection->write(header, 5);
  if (!error) {
    if (0 != buffer_length) error = m_connection->write(buffer, buffer_length);
  }

  return error;
}

bool Protocol_impl::send_impl(const Client_message_type_id mid,
                              const Message &msg,
                              ZeroCopyOutputStream *input_stream) {
  CodedOutputStream cos(input_stream);
  const Header_message_type_id header_mesage_id = mid;
  const int header_message_type_size = sizeof(Header_message_type_id);
  const std::size_t header_whole_message_size =
#if GOOGLE_PROTOBUF_VERSION / 1000000 >= 3
      msg.ByteSizeLong() + header_message_type_size;
#else
      msg.ByteSize() + header_message_type_size;
#endif

  /** polarx rpc sid header */
  cos.WriteLittleEndian64(m_sid);

  cos.WriteLittleEndian32(header_whole_message_size);
  cos.WriteRaw(&header_mesage_id, header_message_type_size);

  dispatch_send_message(mid, msg);

  return msg.SerializeToCodedStream(&cos);
}

XError Protocol_impl::send(const Client_message_type_id mid,
                           const Message &msg) {
  if (m_context->m_global_error) return m_context->m_global_error;

  Connection_output_stream output_stream(m_connection.get());

  if (!send_impl(mid, msg, &output_stream)) return output_stream.getLastError();

  // Ensure that we flush all data before getting last error
  output_stream.Flush();

  return output_stream.getLastError();
}

XProtocol::Handler_id Protocol_impl::add_notice_handler(
    Notice_handler handler, const Handler_position position,
    const Handler_priority priority) {
  const auto id = m_last_handler_id++;
  const auto prio = static_cast<int>(priority);

  switch (position) {
    case Handler_position::Begin:
      m_notice_handlers.push_front({id, prio, handler});
      break;

    case Handler_position::End:
      m_notice_handlers.push_back({id, prio, handler});
      break;
  }

  return id;
}

XProtocol::Handler_id Protocol_impl::add_received_message_handler(
    Server_message_handler handler, const Handler_position position,
    const Handler_priority priority) {
  const auto id = m_last_handler_id++;
  const auto prio = static_cast<int>(priority);

  switch (position) {
    case Handler_position::Begin:
      m_message_received_handlers.push_front({id, prio, handler});
      break;

    case Handler_position::End:
      m_message_received_handlers.push_back({id, prio, handler});
      break;
  }

  return id;
}

XProtocol::Handler_id Protocol_impl::add_send_message_handler(
    Client_message_handler handler, const Handler_position position,
    const Handler_priority priority) {
  const auto id = m_last_handler_id++;
  const auto prio = static_cast<int>(priority);

  switch (position) {
    case Handler_position::Begin:
      m_message_send_handlers.push_front({id, prio, handler});
      break;

    case Handler_position::End:
      m_message_send_handlers.push_back({id, prio, handler});
      break;
  }

  return id;
}

void Protocol_impl::remove_notice_handler(const Handler_id id) {
  const auto handler = std::find_if(
      m_notice_handlers.begin(), m_notice_handlers.end(),
      [id](const Handler_with_id<Notice_handler> &handler) -> bool {
        return id == handler.m_id;
      });

  if (handler == m_notice_handlers.end()) return;

  m_notice_handlers.erase(handler);
}

void Protocol_impl::remove_received_message_handler(const Handler_id id) {
  const auto handler = std::find_if(
      m_message_received_handlers.begin(), m_message_received_handlers.end(),
      [id](const Server_handler_with_id &handler) -> bool {
        return id == handler.m_id;
      });

  if (handler == m_message_received_handlers.end()) return;

  m_message_received_handlers.erase(handler);
}

void Protocol_impl::remove_send_message_handler(const Handler_id id) {
  const auto handler = std::find_if(
      m_message_send_handlers.begin(), m_message_send_handlers.end(),
      [id](const Client_handler_with_id &handler) -> bool {
        return id == handler.m_id;
      });

  if (handler == m_message_send_handlers.end()) return;

  m_message_send_handlers.erase(handler);
}

XError Protocol_impl::dispatch_received(const Server_message_type_id id,
                                        const Message &message,
                                        bool *out_ignore) {
  const Handler_result result = dispatch_received_message(id, message);

  if (Handler_result::Consumed == result) {
    *out_ignore = true;
    return {};
  }

  if (Handler_result::Error == result) {
    return XError{CR_X_INTERNAL_ABORTED, ER_TEXT_RECEIVE_HANDLER_FAILED};
  }

  if (PolarXRPC::ServerMessages::NOTICE == id) {
    auto frame = static_cast<const PolarXRPC::Notice::Frame *>(&message);
    const Handler_result notice_ext_handled = dispatch_received_notice(*frame);

    if (Handler_result::Consumed == notice_ext_handled) {
      *out_ignore = true;
      return {};
    }

    if (Handler_result::Error == notice_ext_handled) {
      return XError{CR_X_INTERNAL_ABORTED, ER_TEXT_NOTICE_HANDLER_FAILED};
    }
  }

  return {};
}

Handler_result Protocol_impl::dispatch_received_notice(
    const PolarXRPC::Notice::Frame &frame) {
  for (const auto &holder : m_notice_handlers) {
    const Handler_result result = holder.m_handler(
        this, frame.scope() == PolarXRPC::Notice::Frame_Scope_GLOBAL,
        static_cast<PolarXRPC::Notice::Frame::Type>(frame.type()),
        frame.has_payload() ? frame.payload().c_str() : nullptr,
        frame.has_payload() ? static_cast<uint32>(frame.payload().length())
                            : 0);

    if (Handler_result::Continue != result) return result;
  }

  return Handler_result::Continue;
}

Handler_result Protocol_impl::dispatch_received_message(
    const Server_message_type_id id, const Message &message) {
  for (const auto &holder : m_message_received_handlers) {
    const Handler_result result = holder.m_handler(this, id, message);

    if (Handler_result::Continue != result) return result;
  }

  return Handler_result::Continue;
}

void Protocol_impl::dispatch_send_message(const Client_message_type_id id,
                                          const Message &message) {
  for (const auto &holder : m_message_send_handlers) {
    holder.m_handler(this, id, message);
  }
}

XError Protocol_impl::recv_ok() {
  return recv_id(PolarXRPC::ServerMessages::OK);
}

XError Protocol_impl::recv_header(Header_message_type_id *out_mid,
                                  uint32_t *out_buffer_size) {
  *out_mid = 0;

  /** polarx rpc */
  uint64_t sid;
  uint8_t size = 5 + 8;

  m_connection_input_stream->AllowedRead(size);

  google::protobuf::io::CodedInputStream cis(m_connection_input_stream.get());

  if (!cis.ReadLittleEndian64(&sid)) {
    auto error = m_connection_input_stream->GetIOError();

    if (details::is_timeout_error(error))
      m_connection_input_stream->ClearIOError();

    return error;
  }
  /** polarx rpc */

  /* Clearing timeout error make sense only in case when
     the client is waiting for X Client header.

     Thus in this case client can retry later on.
   */
  if (!cis.ReadLittleEndian32(out_buffer_size)) {
    auto error = m_connection_input_stream->GetIOError();

    if (details::is_timeout_error(error))
      m_connection_input_stream->ClearIOError();

    return error;
  }

  if (!cis.ReadRaw(out_mid, 1)) {
    auto error = m_connection_input_stream->GetIOError();

    if (details::is_timeout_error(error))
      m_connection_input_stream->ClearIOError();

    return error;
  }

  --(*out_buffer_size);

  return {};
}

XError Protocol_impl::recv(Header_message_type_id *out_mid, uint8_t **buffer,
                           std::size_t *buffer_size) {
  std::unique_ptr<uint8_t[]> payload_buffer;
  uint32_t msglen = 0;
  XError error = recv_header(out_mid, &msglen);

  if (error) {
    return error;
  }

  if (*buffer && *buffer_size < msglen) {
    return XError{CR_X_RECEIVE_BUFFER_TO_SMALL, ER_TEXT_RECEIVE_BUFFER_TO_SMALL,
                  true};
  }

  if (0 < msglen) {
    uint8_t *payload = *buffer;

    if (nullptr == payload) {
      payload_buffer.reset(new uint8_t[msglen]);
      payload = payload_buffer.get();
    }

    m_connection_input_stream->AllowedRead(msglen);
    google::protobuf::io::CodedInputStream cis(m_connection_input_stream.get());
    if (!cis.ReadRaw(payload, msglen))
      return m_connection_input_stream->GetIOError();
  }

  if (payload_buffer) {
    *buffer = payload_buffer.release();
  }

  *buffer_size = msglen;

  return {};
}

void Protocol_impl::skip_not_parsed(
    google::protobuf::io::CodedInputStream *input_stream, XError *out_error) {
  // Check if we parsed whole
  const auto until_limit = input_stream->BytesUntilLimit();
  DBUG_LOG("debug", "Skip data until until_limit: " << until_limit);

  if (until_limit > 0) {
    input_stream->Skip(until_limit);
  }
  /* overwrite the error in case when skip fails */
  auto error = m_connection_input_stream->GetIOError();
  if (error) {
    DBUG_PRINT("info", ("Overwrite the error to IOError"));
    *out_error = error;
  }
}

std::unique_ptr<XProtocol::Message> Protocol_impl::deserialize_message(
    const Header_message_type_id mid,
    google::protobuf::io::CodedInputStream *input_stream, XError *out_error) {
  std::unique_ptr<Message> ret_val = alloc_message(mid);

  if (!ret_val) {
    DBUG_PRINT("info", ("Invalid message type received"));
    *out_error =
        XError(CR_MALFORMED_PACKET, ERR_MSG_UNEXPECTED_MESSAGE +
                                        std::to_string(static_cast<int>(mid)));

    // Only header of the messages was reed
    // to ensure continuity of data `payload` needs to be skipped
    skip_not_parsed(input_stream, out_error);
    return {};
  }

  DBUG_LOG("debug", "Deserialize message: " << ret_val->GetTypeName());
  if (!ret_val->ParseFromCodedStream(input_stream)) {
    std::string error_message(ERR_MSG_MESSAGE_NOT_INITIALIZED);
    error_message += "Name:" + ret_val->GetTypeName() + ", ";
    error_message += ret_val->InitializationErrorString();
    *out_error = XError(CR_MALFORMED_PACKET, error_message);

    // Check if whole frame was parsed, if not then
    // skip the rest of the data
    skip_not_parsed(input_stream, out_error);

    return {};
  }

  return ret_val;
}

std::unique_ptr<XProtocol::Message> Protocol_impl::deserialize_received_message(
    const Header_message_type_id mid, const uint8_t *payload,
    const std::size_t payload_size, XError *out_error) {
  std::unique_ptr<Message> ret_val = alloc_message(mid);

  if (!ret_val) {
    *out_error =
        XError(CR_MALFORMED_PACKET, ERR_MSG_UNEXPECTED_MESSAGE +
                                        std::to_string(static_cast<int>(mid)));

    return {};
  }

  // Parses the received message
  ret_val->ParseFromArray(reinterpret_cast<const char *>(payload),
                          static_cast<int>(payload_size));

  if (!ret_val->IsInitialized()) {
    std::string err(ERR_MSG_MESSAGE_NOT_INITIALIZED);
    err += "Name:" + ret_val->GetTypeName() + ", ";
    err += ret_val->InitializationErrorString();
    *out_error = XError(CR_MALFORMED_PACKET, err);

    return {};
  }

  return ret_val;
}

std::unique_ptr<XProtocol::Message> Protocol_impl::recv_single_message(
    Server_message_type_id *out_mid, XError *out_error) {
  if (m_context->m_global_error) {
    *out_error = m_context->m_global_error;
    return {};
  }

  *out_error = XError();

  while (true) {
    bool out_ignore = false;
    std::unique_ptr<Message> msg(recv_message_with_header(out_mid, out_error));

    if (*out_error) return {};

    // In case when both out_error and msg are not set,
    // this means that dispatching of compressed message skipped it
    // lets retry
    if (msg) {
      *out_error = dispatch_received(*out_mid, *msg.get(), &out_ignore);

      if (*out_error) return {};

      if (out_ignore) continue;

      return msg;
    }
  }
}

std::unique_ptr<Protocol_impl::Message> Protocol_impl::alloc_message(
    const Header_message_type_id mid) {
  std::unique_ptr<Message> ret_val;

  switch (static_cast<PolarXRPC::ServerMessages::Type>(mid)) {
    case PolarXRPC::ServerMessages::OK:
      ret_val.reset(new PolarXRPC::Ok());
      break;
    case PolarXRPC::ServerMessages::ERROR:
      ret_val.reset(new PolarXRPC::Error());
      break;
    case PolarXRPC::ServerMessages::NOTICE:
      ret_val.reset(new PolarXRPC::Notice::Frame());
      break;
    case PolarXRPC::ServerMessages::CONN_CAPABILITIES:
      ret_val.reset(new PolarXRPC::Connection::Capabilities());
      break;
    case PolarXRPC::ServerMessages::SESS_AUTHENTICATE_CONTINUE:
      ret_val.reset(new PolarXRPC::Session::AuthenticateContinue());
      break;
    case PolarXRPC::ServerMessages::SESS_AUTHENTICATE_OK:
      ret_val.reset(new PolarXRPC::Session::AuthenticateOk());
      break;
    case PolarXRPC::ServerMessages::RESULTSET_COLUMN_META_DATA:
      ret_val.reset(new PolarXRPC::Resultset::ColumnMetaData());
      break;
    case PolarXRPC::ServerMessages::RESULTSET_ROW:
      ret_val.reset(new PolarXRPC::Resultset::Row());
      break;
    case PolarXRPC::ServerMessages::RESULTSET_FETCH_DONE:
      ret_val.reset(new PolarXRPC::Resultset::FetchDone());
      break;
    case PolarXRPC::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS:
      ret_val.reset(new PolarXRPC::Resultset::FetchDoneMoreResultsets());
      break;
    case PolarXRPC::ServerMessages::SQL_STMT_EXECUTE_OK:
      ret_val.reset(new PolarXRPC::Sql::StmtExecuteOk());
      break;
    case PolarXRPC::ServerMessages::RESULTSET_FETCH_DONE_MORE_OUT_PARAMS:
      ret_val.reset(new PolarXRPC::Resultset::FetchDoneMoreOutParams());
      break;
    case PolarXRPC::ServerMessages::RESULTSET_TSO:
      ret_val.reset(new PolarXRPC::ExecPlan::ResultTSO);
      break;
    case PolarXRPC::ServerMessages::RESULTSET_TOKEN_DONE:
      ret_val.reset(new PolarXRPC::Resultset::TokenDone());
      break;

    case PolarXRPC::ServerMessages::RESULTSET_GET_FILE_INFO_OK:
      ret_val.reset(new PolarXRPC::PhysicalBackfill::GetFileInfoOperator());
      break;
    case PolarXRPC::ServerMessages::RESULTSET_TRANSFER_FILE_DATA_OK:
      ret_val.reset(
          new PolarXRPC::PhysicalBackfill::TransferFileDataOperator());
      break;
    case PolarXRPC::ServerMessages::RESULTSET_FILE_MANAGE_OK:
      ret_val.reset(
          new PolarXRPC::PhysicalBackfill::FileManageOperatorResponse());
      break;
    default:
      break;
  }

  return ret_val;
}

XError Protocol_impl::recv_id(
    const XProtocol::Server_message_type_id expected_id) {
  XError out_error;
  Server_message_type_id out_mid;
  std::unique_ptr<Message> msg(recv_single_message(&out_mid, &out_error));

  if (out_error) return out_error;

  if (PolarXRPC::ServerMessages::ERROR == out_mid) {
    const ::PolarXRPC::Error &error =
        *static_cast<PolarXRPC::Error *>(msg.get());

    return details::make_xerror(error);
  }

  if (expected_id != out_mid) {
    return XError{CR_MALFORMED_PACKET,
                  "Unknown message received from server " +
                      std::to_string(static_cast<int>(out_mid))};
  }

  return {};
}

XProtocol::Message *Protocol_impl::recv_id(
    const XProtocol::Server_message_type_id expected_id, XError *out_error) {
  Server_message_type_id out_mid;
  std::unique_ptr<Message> msg(recv_single_message(&out_mid, out_error));

  if (*out_error) return nullptr;

  if (PolarXRPC::ServerMessages::ERROR == out_mid) {
    const ::PolarXRPC::Error &error =
        *static_cast<PolarXRPC::Error *>(msg.get());

    *out_error = details::make_xerror(error);
    return nullptr;
  }

  if (expected_id != out_mid) {
    *out_error = XError{CR_MALFORMED_PACKET,
                        "Unknown message received from server " +
                            std::to_string(static_cast<int>(out_mid))};
    return nullptr;
  }

  return msg.release();
}

XProtocol::Message *Protocol_impl::recv_message_with_header(
    Server_message_type_id *mid, XError *out_error) {
  uint32_t payload_size = 0;

  Header_message_type_id header_mid;
  *out_error = recv_header(&header_mid, &payload_size);
  *mid = static_cast<Server_message_type_id>(header_mid);

  if (*out_error) return nullptr;

  DBUG_LOG("info",
           "Reading X message of type: " << static_cast<int>(header_mid)
                                         << ", and size: " << payload_size);

  m_connection_input_stream->AllowedRead(payload_size);

  // The 'cis' variable must be destroyed before doing read_buffered
  google::protobuf::io::CodedInputStream cis(m_connection_input_stream.get());

  cis.PushLimit(payload_size);

  DBUG_PRINT("info", ("Reading uncompressed X message"));
  auto result = deserialize_message(header_mid, &cis, out_error);

  if (!*out_error) {
    *out_error = m_connection_input_stream->GetIOError();
  }

  if (*out_error) {
    return nullptr;
  }

  *mid = static_cast<XProtocol::Server_message_type_id>(header_mid);

  return result.release();
}

}  // namespace xcl
