/*****************************************************************************

Copyright (c) 2013, 2020, Alibaba and/or its affiliates. All Rights Reserved.

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

/** @file include/lizard0trx.h
  Lizard transaction structure.

 Created 2020-08-27 by Jianwei.zhao
 *******************************************************/

#include "lizard0trx.h"
#include "lizard0cleanout.h"
#include "lizard0row.h"
#include "lizard0undo.h"
#include "trx0trx.h"

#ifndef UNIV_HOTBACKUP
#include <sql_class.h>
#endif

/* To get current session thread default THD */
THD *thd_get_current_thd();

namespace lizard {

void alloc_cleanout_cursors(trx_t *trx) {
  trx->cleanout_cursors = ut::new_withkey<Cleanout_cursors>(
      ut::make_psi_memory_key(mem_key_row_cleanout));
  ut_ad(trx->cleanout_cursors != nullptr);
}

void release_cleanout_cursors(trx_t *trx) {
  ut::delete_(trx->cleanout_cursors);
  trx->cleanout_cursors = nullptr;
}

void cleanout_rows_at_commit(trx_t *trx) {
  ut_ad(trx != nullptr);
  ut_ad(trx->cleanout_cursors != nullptr);

  if (trx->cleanout_cursors->cursor_count() == 0) {
    return;
  }

  /** Skip cleanout as the transaction is a full rollback. */
  if (trx->is_rollback) {
    trx->cleanout_cursors->init();
    return;
  }

  auto undo_ptr = trx->txn_desc.undo_ptr;
  ut_ad(!undo_ptr_is_active(trx->txn_desc.undo_ptr));

  txn_rec_t txn_rec{trx->id, trx->txn_desc.cmmt.scn, undo_ptr,
                    trx->txn_desc.cmmt.gcn};

  commit_cleanout_do(trx, txn_rec);
}

/** In order to optimize performance of multi-branch execution on one DN,
 *  XA branchs will be divided into XA MASTER AND XA SLAVE, they can be
 *  executed perallel and share same SCN when MVCC.
 *
 *  Check if trx is xa slave branch.
 *
 *  @param[in]	trx
 *
 *  @retval	true	yes
 *  @retval	false	no */
bool trx_is_xa_slave(const trx_t *trx) {
  assert_trx_commit_mark_allocated(trx);
  return !trx->txn_desc.maddr.is_null();
}

}  // namespace lizard
