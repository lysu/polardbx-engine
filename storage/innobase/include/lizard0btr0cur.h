/*****************************************************************************

Copyright (c) 2013, 2024, Alibaba and/or its affiliates. All Rights Reserved.

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

/** @file include/lizard0btr0cur.h
   Lizard index tree cursor


 Created 2024-04-12 by Yichang.Song
 *******************************************************/

#ifndef lizard0btr0cur_h
#define lizard0btr0cur_h

#include "btr0cur.h"
#include "lizard0data0types.h"

namespace lizard {
static inline gpp_no_t btr_cur_get_page_no(const btr_cur_t *cursor) {
  return btr_cur_get_block(cursor)->get_page_no();
}

/**
 * Reset the index id to 0 if btr page freed.
 *
 * @param[in] block   Pointer to the page
 * @param[in] mtr     Mini-transaction, can be NULL if redo log is not needed
 */
inline void btr_page_reset_index_id(buf_block_t *block, mtr_t *mtr = nullptr) {
  page_t *page = buf_block_get_frame(block);
  page_zip_des_t *page_zip = buf_block_get_page_zip(block);

  ut_ad(fil_page_index_page_check(page));

  if (mtr) {
    btr_page_set_index_id(page, page_zip, 0, mtr);
  } else {
    mach_write_to_8(page + (PAGE_HEADER + PAGE_INDEX_ID), 0);
    if (page_zip) {
      page_zip_write_header(page_zip, page + (PAGE_HEADER + PAGE_INDEX_ID), 8,
                            nullptr);
    }
  }
}

/**
 * Attempts to position a persistent cursor on a clustered index
 * record based on the gpp_no read from the secondary index record.
 *
 * @param[in]     clust_idx       Clustered index
 * @param[in]     sec_idx         Secondary index
 * @param[in]     clust_ref       Reference tuple for the clustered index
 * @param[in]     sec_rec         Secondary index record
 * @param[in,out] clust_pcur      Persistent cursor for the clustered index
 * @param[out]    sec_offsets     Offsets array for the secondary record
 * @param[in]     latch_mode      latching mode
 * @param[in]     mtr             Mini-transaction handle
 * @return        True if successful positioning, False otherwise
 */
bool btr_cur_guess_clust_by_gpp(dict_index_t *clust_idx,
                                const dict_index_t *sec_idx,
                                const dtuple_t *clust_ref, const rec_t *sec_rec,
                                btr_pcur_t *clust_pcur,
                                const ulint *sec_offsets, ulint latch_mode,
                                ulint &gpp_no_offset,
                                mtr_t *mtr);

/**
  Write redo log when updating scn/uba/gcn fileds in physical records.

  @param[in]      rec        physical record
  @param[in]      index      dict that interprets the row record
  @param[in]      txn_rec    txn info from the record
  @param[in]      mtr        mtr
*/
extern void btr_cur_upd_lizard_fields_clust_rec_log(const rec_t *rec,
                                                    const dict_index_t *index,
                                                    const txn_rec_t *txn_rec,
                                                    mtr_t *mtr);

/**
  Parse the txn info from redo log record, and apply it if necessary.
  @param[in]      ptr        buffer
  @param[in]      end        buffer end
  @param[in]      page       page (NULL if it's just get the length)
  @param[in]      page_zip   compressed page, or NULL
  @param[in]      index      index corresponding to page

  @return         return the end of log record or NULL
*/
extern byte *btr_cur_parse_lizard_fields_upd_clust_rec(
    byte *ptr, byte *end_ptr, page_t *page, page_zip_des_t *page_zip,
    const dict_index_t *index);

/** Updates the redo log record for a gpp_no of a secondary index record. 
  @param[in]      rec             secondary record
  @param[in]      index           secondary index
  @param[in]      gpp_no_offset   the offsets of gpp_no field       
  @param[in]      gpp_no          page no of the cluster index
  @param[in]      mtr             mtr
*/
extern void btr_cur_upd_gpp_no_sec_rec_log(const rec_t *rec,
                                           const dict_index_t *index,
                                           ulint gpp_no_offset,
                                           page_no_t gpp_no, mtr_t *mtr);

/**
  Parses the redo log record for a gpp_no of a secondary index record.
  @param[in]      ptr        buffer
  @param[in]      end        buffer end
  @param[in]      page       page (NULL if it's just get the length)
  @param[in]      page_zip   compressed page, or NULL
  @param[in]      index      index corresponding to page

  @return         return the end of log record or NULL
*/
extern byte *btr_cur_parse_gpp_no_upd_sec_rec(
    byte *ptr,                 /*!< in: buffer */
    byte *end_ptr,             /*!< in: buffer end */
    page_t *page,              /*!< in/out: page or NULL */
    page_zip_des_t *page_zip); /*!< in/out: compressed page, or NULL */
}  // namespace lizard

#endif
