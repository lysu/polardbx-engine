/* Copyright (c) 2008, 2023, Alibaba and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/xa/lizard_xa_trx.h"

#include "sql/binlog.h"
#include "sql/handler.h"
#include "sql/lizard0handler.h"
#include "sql/lizard_binlog.h"
#include "sql/mysqld.h"
#include "sql/raii/sentry.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/sql_parse.h"
#include "sql/xa/sql_xa_commit.h"
#include "sql/xa/transaction_cache.h"

#include "mysql/components/services/log_builtins.h"

namespace lizard {
namespace xa {

class Xa_active_pretender {
 public:
  Xa_active_pretender(THD *thd) {
    m_xid_state = thd->get_transaction()->xid_state();

    assert(m_xid_state->has_state(XID_STATE::XA_IDLE));

    m_xid_state->set_state(XID_STATE::XA_ACTIVE);
  }

  ~Xa_active_pretender() {
    assert(m_xid_state->has_state(XID_STATE::XA_ACTIVE));
    m_xid_state->set_state(XID_STATE::XA_IDLE);
  }

 private:
  XID_STATE *m_xid_state;
};

/**
  Apply for a readwrite transaction specially for external xa through allocating
  transaction slot from transaction slot storage engine.

    1. start trx in transaction slot storage engine.[ttse]
    2. register ttse as a participants
    3. alloc transaction slot in ttse
    4. register binlog as another participants if need

  @param[in]	Thread handler
  @param[in]	XID
  @param[out]	transaction slot address
  @param[out]	transaction identifier
*/
bool apply_trx_for_xa(THD *thd, const XID *xid, slot_ptr_t *slot_ptr,
                      trx_id_t *trx_id) {
  /** Take innodb as transaction slot storage engine. */
  handlerton *ttse = innodb_hton;

#ifndef NDEBUG
  XID_STATE *xid_state = thd->get_transaction()->xid_state();
  assert(xid_state->has_state(XID_STATE::XA_IDLE));
  assert(xid_state->get_xid()->eq(xid));
  assert(ttse);
#endif
  (void)xid;

  /**
    Lizard: In the past, the binlog can only be registered in the XA ACTIVE
    state. But now the registration is completed in the IDLE state.

    So the pretender is introduced to pretend the XA is still in ACTIVE
    status.
  */
  Xa_active_pretender pretender(thd);

  /** 1. start trx within transaction slot storage engine, and register it as a
   * participants. */
  if (ttse->ext.start_trx_for_xa(ttse, thd, true)) {
    my_error(ER_XA_PROC_REPLAY_TRX_SLOT_ALLOC_ERROR, MYF(0));
    return true;
  }

  /** 2. alloc transaction slot in ttse, will also write xid into TXN. */
  if (ttse->ext.assign_slot_for_xa(thd, slot_ptr, trx_id)) {
    my_error(ER_XA_PROC_BLANK_XA_TRX, MYF(0));
    return true;
  }

  /** 3. register binlog as another participants if need */
  if (mysql_bin_log.is_open() &&
      (thd->variables.option_bits & OPTION_BIN_LOG) &&
      binlog_start_trans(thd)) {
    my_error(ER_XA_PROC_REPLAY_REGISTER_BINLOG_ERROR, MYF(0));
    return true;
  }

  return false;
}

/**
  Find Transaction_ctx in Transaction_cache by XID
*/
static std::shared_ptr<Transaction_ctx> find_trn_for_search_and_get_its_state(
    xid_t *xid_for_trn, bool *is_detached) {
  *is_detached = false;

  auto foundit = ::xa::Transaction_cache::find(
      xid_for_trn, [&](std::shared_ptr<Transaction_ctx> const &item) -> bool {
        *is_detached = item->xid_state()->is_detached();
        return true;
      });

  return foundit;
}

static bool search_detach_prepare_trx(std::shared_ptr<Transaction_ctx> &trx_ctx,
                                      xid_t *xid, MyXAInfo *info) {
  bool is_detached;
  bool found;
  auto detached_xs = trx_ctx->xid_state();
  handlerton_ext &ibh_ext = innodb_hton->ext;

  /** If can not acquire lock, must someone do xa commit, xa rollback,
  or another find_by_xid */
  if (!detached_xs->get_xa_lock().try_lock()) {
    *info = MY_XA_INFO_ATTACH;
    return true;
  }

  raii::Sentry<> mutex_guard_on_error{
      [detached_xs]() -> void { detached_xs->get_xa_lock().unlock(); }};

  /** 2. Detached prepared XA transaction. */
  if (find_trn_for_search_and_get_its_state(xid, &is_detached) != nullptr) {
    assert(is_detached);

    found = ibh_ext.search_detach_prepare_trx_by_xid(xid, info);
    assert(!found || info->status == XA_status::DETACHED_PREPARE);
    if (!found) {
      /**
        Found empty XA transaction, like:

        xa start 'fbx_empty_b2';
        xa end 'fbx_empty_b2';

        xa prepare 'fbx_empty_b2';
        call dbms_xa.find_by_xid('fbx_empty_b2', '', 1);
      */
      *info = MY_XA_INFO_FORGET;
    }

    return true;
  }

  /** Must commit / rollback already. */
  return false;
}

/**
  Search XA transaction info by XID.
  @param[in]    xid     XID
  @param[out]   info    XA info
*/
void search_trx_info(xid_t *xid, MyXAInfo *info) {
  bool is_detached;
  handlerton_ext &ibh_ext = innodb_hton->ext;

  std::shared_ptr<Transaction_ctx> trx_ctx =
      find_trn_for_search_and_get_its_state(xid, &is_detached);

  if (trx_ctx != nullptr) {
    if (!is_detached) {
      /** Attached XA transaction. */
      *info = MY_XA_INFO_ATTACH;
      return;
    } else {
      /** Detached XA transaction. */
      if (search_detach_prepare_trx(trx_ctx, xid, info)) {
        return;
      }
    }
  }

  /** Attached by backgroud xa transaction. */
  /** TODO: Hold mutex always, might search history first. <08-08-24, zanye.zjy> */
  if (ibh_ext.search_rollback_background_trx_by_xid(xid, info)) {
    return;
  }

  /** XA transaction in history, must committed or rollbacked. */
  if (ibh_ext.search_history_trx_by_xid(xid, info)) {
    return;
  }

  /** Not found anywhere, not start or forget. */
  *info = MY_XA_INFO_FORGET;
}

}  // namespace xa
}  // namespace lizard
