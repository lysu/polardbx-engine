/*****************************************************************************

Copyright (c) 2013, 2020, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
lzeusited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the zeusplied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file trx/lizard0purge.cc
 Lizard transaction purge system implementation.

 Created 2020-03-27 by zanye.zjy
 *******************************************************/

#include "lizard0purge.h"
#include "lizard0dbg.h"
#include "lizard0gcs.h"
#include "lizard0scn.h"
#include "lizard0undo.h"

#include "mtr0log.h"
#include "trx0purge.h"
#include "trx0rseg.h"
#include "que0que.h"
#include "row0purge.h"
#include "fut0lst.h"
#include "trx0rec.h"
#include "row0upd.h"

/** A sentinel undo record used as a return value when we have a whole
undo log which can be skipped by purge */
extern trx_undo_rec_t trx_purge_ignore_rec;

/** lizard: a sentinel undo record to identify the BLOCKED record */
extern trx_undo_rec_t trx_purge_blocked_rec;

namespace lizard {

/** Sentinel value */
const TxnUndoRsegs TxnUndoRsegsIterator::NullElement(SCN_NULL);

/** Constructor */
TxnUndoRsegsIterator::TxnUndoRsegsIterator(trx_purge_t *purge_sys)
    : m_purge_sys(purge_sys),
      m_txn_undo_rsegs(NullElement),
      m_iter(m_txn_undo_rsegs.end()) {}

const page_size_t TxnUndoRsegsIterator::set_next(bool *keep_top) {
  ut_ad(keep_top != NULL);
  mutex_enter(&m_purge_sys->pq_mutex);
  *keep_top = false;

  lizard_purged_scn_validation();

  if (m_iter != m_txn_undo_rsegs.end()) {
    m_purge_sys->iter.ommt = (*m_iter)->last_ommt;
  } else if (!m_purge_sys->purge_heap->empty()) {
    /** We can't just pop the top element of the heap. In the past,
    It must be the smallest trx_no in the top of the heap, so we just
    wait until the purge sys get a big enough view.

    However, it's possible the top element is not the one with smallest
    scn. In order to avoid pop the element, we added the following codes.
    We might relax the limit in the future. */

    if (purge_sys->purge_heap->top().get_scn() >
        m_purge_sys->vision.snapshot_scn()) {
      *keep_top = true;
      ut_ad(purge_sys == m_purge_sys);
      mutex_exit(&m_purge_sys->pq_mutex);
      return (univ_page_size);
    }

    m_txn_undo_rsegs = NullElement;

    while (!m_purge_sys->purge_heap->empty()) {
      if (m_txn_undo_rsegs.get_scn() == SCN_NULL) {
        m_txn_undo_rsegs = purge_sys->purge_heap->top();
      } else if (purge_sys->purge_heap->top().get_scn() ==
                 m_txn_undo_rsegs.get_scn()) {
        /** Assume that there are temp rseg and durable rseg in a trx,
        when the trx was commited, only temp rseg was added in the purge
        heap for the reason: the last_page_no of durable rseg is not
        equal FIL_NULL. And then the rseg was poped and pushed again,
        the following branch can be achieved */
        m_txn_undo_rsegs.insert(purge_sys->purge_heap->top());
      } else {
        break;
      }
      m_purge_sys->purge_heap->pop();
    }

    /* In order for 'AS OF' to correctly determine whether the undo log
     * is still available, we should ensure that the txn rseg of a transaction
     * beging purged before the other rsegs. */
    m_iter = m_txn_undo_rsegs.arrange_txn_first();
  } else {
    /* Queue is empty, reset iterator. */
    m_txn_undo_rsegs = NullElement;
    m_iter = m_txn_undo_rsegs.end();

    mutex_exit(&m_purge_sys->pq_mutex);

    m_purge_sys->rseg = nullptr;

    /* return a dummy object, not going to be used by the caller */
    return (univ_page_size);
  }

  m_purge_sys->rseg = *m_iter++;

  mutex_exit(&m_purge_sys->pq_mutex);

  ut_a(m_purge_sys->rseg != nullptr);

  m_purge_sys->rseg->latch();

  ut_a(m_purge_sys->rseg->last_page_no != FIL_NULL);
  ut_ad(m_purge_sys->rseg->last_ommt.scn == m_txn_undo_rsegs.get_scn());

  /* The space_id must be a tablespace that contains rollback segments.
  That includes the system, temporary and all undo tablespaces. */
  ut_a(fsp_is_system_or_temp_tablespace(m_purge_sys->rseg->space_id) ||
       fsp_is_undo_tablespace(m_purge_sys->rseg->space_id));

  const page_size_t page_size(m_purge_sys->rseg->page_size);

  /** ZEUS: We don't hold pq_mutex when we commit a trx. The possible case:
  TRX_A: scn = 5, scn allocated, rseg not pushed in purge_heap
  TRX_B: scn = 6, scn allocated, rseg pushed in purge_heap

  Then, purge_sys purge undo of TRX_B, purge_sys->iter.scn = 6. And rseg of
  TRX_A finally pushed in purge_heap, the following assert can't be achieved.

  In other words, the rollback segments are not added to the heap in order,
  which may result in the above situations. We haven't found a possible
  hazard, so we comment the assertion out */
  /* ut_a(purge_sys->iter.scn <= purge_sys->rseg->last_scn); */

  m_purge_sys->iter.ommt = m_purge_sys->rseg->last_ommt;
  m_purge_sys->hdr_offset = m_purge_sys->rseg->last_offset;
  m_purge_sys->hdr_page_no = m_purge_sys->rseg->last_page_no;

  m_purge_sys->rseg->unlatch();

  return (page_size);
}

/**
  Initialize / reload purged_scn from purge_sys->purge_heap

  @retval              a valid scn if found, or PURGED_SCN_INVALID if in
                       "srv_force_recovery >= SRV_FORCE_NO_UNDO_LOG_SCAN"
*/
scn_t trx_purge_reload_purged_scn() {
  scn_t min_history_scn;
  /** If undo log scan is forbidden, purge_sys->purged_scn can't get a valid
  value */
  if (srv_force_recovery >= SRV_FORCE_NO_UNDO_LOG_SCAN) {
    return PURGED_SCN_INVALID;
  }

  ut_ad(purge_sys);

  if (purge_sys->purge_heap->empty()) {
    min_history_scn = gcs_load_scn();
  } else {
    min_history_scn = purge_sys->purge_heap->top().get_scn();
    ut_ad(min_history_scn < gcs_load_scn());
  }

  return min_history_scn;
}

/**
  Set purged_scn in purge sys

  @param[in]    txn_scn     purged scn
*/
void trx_purge_set_purged_scn(scn_t txn_scn) {
  /* It's safe because there is purge coordinator thread and server
  starting thread updating it. */
  purge_sys->purged_scn.store(txn_scn);
}

/**
  precheck if txn of the row is purged, without really reading txn

  @param[in]    txn_rec     the current row to be checked

  @retval       bool        true if the corresponding txn has been purged
*/
bool precheck_if_txn_is_purged(const txn_rec_t *txn_rec) {
  if (!undo_ptr_is_active(txn_rec->undo_ptr)) {
    /** scn must allocated */
    lizard_ut_ad(txn_rec->scn > 0 && txn_rec->scn < SCN_MAX);

    return (txn_rec->scn <= purge_sys->purged_scn);
  }
  return false;
}

void trx_purge_add_sp_list(trx_rseg_t *rseg, trx_rsegf_t *rseg_hdr,
                           trx_ulogf_t *log_hdr, ulint type, mtr_t *mtr) {
  ut_ad(mutex_own(&rseg->mutex));

  if (type == TRX_UNDO_UPDATE) {
    flst_add_first(rseg_hdr + TRX_RSEG_SEMI_PURGE_LIST,
                   log_hdr + TRX_UNDO_HISTORY_NODE, mtr);
  }
}

/**
 * Removes the undo segment from the history list.
 *
 * @param[in] rseg            Rollback segment
 * @param[in] hdr_addr        File address of log_hdr
 */
void trx_purge_remove_last_log(trx_rseg_t *rseg, fil_addr_t hdr_addr) {
  mtr_t mtr;
  page_t *undo_page;
  trx_rsegf_t *rseg_hdr;
  trx_ulogf_t *log_hdr;
  trx_usegf_t *seg_hdr;
  ulint seg_size;
  ulint hist_size;
  ulint sp_size;
  ulint type;

  mtr_start(&mtr);
  mutex_enter(&rseg->mutex);

  rseg_hdr =
      trx_rsegf_get(rseg->space_id, rseg->page_no, rseg->page_size, &mtr);
  undo_page = trx_undo_page_get(page_id_t(rseg->space_id, hdr_addr.page),
                                rseg->page_size, &mtr);
  seg_hdr = undo_page + TRX_UNDO_SEG_HDR;
  log_hdr = undo_page + hdr_addr.boffset;
  type = mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_TYPE);

  ut_ad(type == TRX_UNDO_UPDATE);
  ut_ad(lizard::trx_useg_is_2pc_purge(undo_page, rseg->page_size, &mtr));
  ut_ad(mach_read_from_2(log_hdr + TRX_UNDO_NEXT_LOG) == 0);

  seg_size = flst_get_len(seg_hdr + TRX_UNDO_PAGE_LIST);
  hist_size =
      mtr_read_ulint(rseg_hdr + TRX_RSEG_HISTORY_SIZE, MLOG_4BYTES, &mtr);
  sp_size = mtr_read_ulint(rseg_hdr + TRX_RSEG_SEMI_PURGE_LIST_SIZE,
                           MLOG_4BYTES, &mtr);
  ut_ad(hist_size >= seg_size);
  ut_ad(rseg->get_curr_size() >= seg_size);
  ut_ad(rseg->get_curr_size() > (hist_size + sp_size));

  trx_purge_remove_log_hdr(rseg_hdr, log_hdr, &mtr);
  mlog_write_ulint(rseg_hdr + TRX_RSEG_HISTORY_SIZE, hist_size - seg_size,
                   MLOG_4BYTES, &mtr);

  trx_purge_add_sp_list(rseg, rseg_hdr, log_hdr, type, &mtr);
  mlog_write_ulint(rseg_hdr + TRX_RSEG_SEMI_PURGE_LIST_SIZE, sp_size + seg_size,
                   MLOG_4BYTES, &mtr);

  mutex_exit(&rseg->mutex);
  mtr_commit(&mtr);
}

#if defined UNIV_DEBUG || defined LIZARD_DEBUG
/**
  Validate all transactions whose SCN > purged_scn is always unpurged.

  @return         true      sucessful validation
*/
bool purged_scn_validation() {
  bool ret = false;
  scn_t top_scn;

  /** If undo log scan is forbidden, purge_sys->purged_scn can't get a valid
  value */
  if (srv_force_recovery >= SRV_FORCE_NO_UNDO_LOG_SCAN) {
    return true;
  }

  /* purge sys not init yet */
  if (!purge_sys) return true;

  ut_a(mutex_own(&purge_sys->pq_mutex));

  ut_a(purge_sys->purged_scn.load() != PURGED_SCN_INVALID);

  if (!purge_sys->purge_heap->empty()) {
    top_scn = purge_sys->purge_heap->top().get_scn();
    ret = (purge_sys->purged_scn <= top_scn);
  } else {
    ret = (purge_sys->purged_scn.load() <= gcs_load_min_safe_scn());
  }
  ut_ad(ret);

  return ret;
}
#endif /* UNIV_DEBUG || defined LIZARD_DEBUG */

void trx_purge_start_history() {
  que_thr_t *thr = nullptr;
  for (thr = UT_LIST_GET_FIRST(purge_sys->query->thrs); thr != nullptr;
       thr = UT_LIST_GET_NEXT(thrs, thr)) {
    purge_node_t *node = static_cast<purge_node_t *>(thr->child);
    node->start_history_purge();
  }
}

}  // namespace lizard
