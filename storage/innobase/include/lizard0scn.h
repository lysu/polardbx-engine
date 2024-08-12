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

/** @file include/lizard0scn.h
 Lizard scn number implementation.

 Created 2020-03-23 by Jianwei.zhao
 *******************************************************/

#ifndef lizard0scn_h
#define lizard0scn_h

#include <utility>

#include "lizard0scn0types.h"
#include "lizard0ut.h"
#include "ut0mutex.h"

#include "mtr0types.h"

#include "sql/lizard/lizard_service.h"

/** The number gap of persist scn number into system tablespace */
#define GCS_SCN_NUMBER_MAGIN 8192

struct mtr_t;

namespace lizard {

/**
   Writes a log record about gcn.

   @param[in]		type	redo log record type
   @param[in,out]	log_ptr	current end of mini-transaction log
   @param[in/out]	mtr	mini-transaction

   @reval	end of mtr
 */
byte *mlog_write_initial_gcn_log_record(mlog_id_t type, byte *log_ptr,
                                        mtr_t *mtr);
/**
   Parses an initial log record written by mlog_write_initial_gcn_log_record.

   @param[in]	ptr		buffer
   @param[in]	end_ptr		buffer end
   @param[out]	type		log record type, should be
                                MLOG_GCN_METADATA
   @param[out]	gcn

   @return parsed record end, NULL if not a complete record
 */
byte *mlog_parse_initial_gcn_log_record(const byte *ptr, const byte *end_ptr,
                                        mlog_id_t *type, gcn_t *gcn);

/**------------------------------------------------------------------------*/
/** Format in UTC */
/*
 *	-------		---------
 *	1  bit		csr
 *	63 bit		time (us)
 */
/**------------------------------------------------------------------------*/

/** US */
#define UTC_POS_US 0
#define UTC_WIDTH_US 63

#define UTC_MASK_US ((~(~0ULL << UTC_WIDTH_US)) << UTC_POS_US)
#define UTC_GET_US(value) ((((utc_t)value) & UTC_MASK_US) >> UTC_POS_US)

/** The most bit for commit source of UTC. */
#define UTC_POS_CSR (UTC_POS_US + UTC_WIDTH_US)
#define UTC_WIDTH_CSR 1

#define UTC_MASK_CSR ((~(~0ULL << UTC_WIDTH_CSR)) << UTC_POS_CSR)

#define UTC_GET_CSR(value) ((((utc_t)value) & UTC_MASK_CSR) >> UTC_POS_CSR)

inline utc_t encode_utc(const ib_time_system_us_t us, const csr_t csr) {
  ut_ad(UTC_GET_CSR(us) == 0);
  static_assert(CSR_AUTOMATIC == 0, "csr automatic != 0");
  static_assert(CSR_ASSIGNED == 1, "csr assigned != 1");

  utc_t value = static_cast<utc_t>(csr);

  return (utc_t)us | (value << UTC_POS_CSR);
}

inline std::pair<utc_t, csr_t> decode_utc(const utc_t utc) {
  utc_t us = UTC_GET_US(utc);
  csr_t csr = static_cast<csr_t>(UTC_GET_CSR(utc));
  return std::make_pair(us, csr);
}

/** UTC null value include <US, CSR>*/
#define UTC_NULL encode_utc(US_NULL, CSR_AUTOMATIC)

/** GCS Metadata which is used to persist */
class PersistentGcsData {
 public:
  PersistentGcsData() : m_scn(SCN_NULL), m_gcn(GCN_NULL) {}

  scn_t get_scn() const { return m_scn; }
  gcn_t get_gcn() const { return m_gcn; }

  void set_scn(scn_t scn) { m_scn = scn; }
  void set_gcn(gcn_t gcn) { m_gcn = gcn; }

  /**
    Set gcn if bigger
  */
  void set_gcn_if_bigger(const gcn_t gcn) {
    if (gcn == GCN_NULL) return;

    if (m_gcn == GCN_NULL || gcn > m_gcn) {
      m_gcn = gcn;
    }
  }

 private:
  scn_t m_scn;
  gcn_t m_gcn;
};

/** Persister of metadata interface. */
class Persister {
 public:
  Persister() {}
  virtual ~Persister() {}

 public:
  /** Write metadata and redo log. */
  virtual void write(const PersistentGcsData *metadata) = 0;

  /** Write only redo log */
  virtual void write_log(const PersistentGcsData *metadata, mtr_t *mtr) = 0;

  /** Read metadata */
  virtual void read(PersistentGcsData *metadata) = 0;
};

/** SCN persister */
class ScnPersister : public Persister {
 public:
  /** Write scn value into tablespace.

      @param[in]	metadata   scn metadata
   */
  virtual void write(const PersistentGcsData *metadata) override;

  /** Write scn value redo log.

      @param[in]	metadata   scn metadata
      @param[in]	mini transacton context

      @TODO
   */
  virtual void write_log(const PersistentGcsData *metadata,
                         mtr_t *mtr) override;

  /** Read scn value from tablespace and increase GCS_SCN_NUMBER_MAGIN
   *  to promise unique.

      @param[in/out]	metadata   scn metadata

   */
  virtual void read(PersistentGcsData *metadata) override;
};

/* The structure of scn number generation */
class SCN {
 public:
  SCN(Persister *persister);
  virtual ~SCN();

  /** Assign the init value by reading from tablespace */
  void boot();

  /** Calculate a new scn number
  @return     scn */
  scn_t new_scn();

  scn_t load_scn() {
    ut_ad(m_inited);
    return m_scn.load();
  }

 private:
  /** Disable the copy and assign function */
  SCN(const SCN &) = delete;
  SCN(const SCN &&) = delete;
  SCN &operator=(const SCN &) = delete;

 private:
  std::atomic<scn_t> m_scn;
  Persister *m_persister;
  bool m_inited;
};

/* The structure of gcn number generation */
class GCN {
 public:
  GCN(Persister *persister);
  virtual ~GCN() { m_inited = false; }

  void boot();

  /**
    Prepare a gcn number according to source type when
    commit.

    1) CSR_AUTOMATIC
      -- Generate from current gcn early, use it directly.
      -- Use current gcn as commit gcn.
    2) CSR_ASSIGNED
      -- Come from 3-party component, use it directly,
         increase current gcn if bigger.

    @param[in]	gcn
    @param[in]	csr
    @param[in]	mini transaction

    @retval	gcn
  */
  gcn_tuple_t new_gcn(const gcn_t gcn, const csr_t csr, mtr_t *mtr);

  gcn_t load_gcn() const { return m_gcn.load(); }

  /**
     Push up current gcn if bigger.

     @param[in]	gcn
  */
  void set_gcn_if_bigger(const gcn_t gcn);

  /** Disable the copy and assign function */
  GCN(const GCN &) = delete;
  GCN(const GCN &&) = delete;
  GCN &operator=(const GCN &) = delete;

 private:
  std::atomic<gcn_t> m_gcn;

  Persister *m_persister;
  bool m_inited;
};

/** gcn persister */
class GcnPersister : public Persister {
 public:
  /** Write gcn value into tablespace.

      @param[in]	metadata   gcn metadata
   */
  virtual void write(const PersistentGcsData *metadata) override;

  /** Write gcn value redo log.

      @param[in]	metadata   gcn metadata
      @param[in]	mini transacton context

   */
  virtual void write_log(const PersistentGcsData *metadata,
                         mtr_t *mtr) override;

  /** Read gcn value from tablespace

      @param[in/out]	metadata   gcn metadata

   */
  virtual void read(PersistentGcsData *metadata) override;
};

/**
  Check the commit scn state

  @param[in]    scn       commit scn
  @return       scn state SCN_STATE_INITIAL, SCN_STATE_ALLOCATED or
                          SCN_STATE_INVALID
*/
enum scn_state_t commit_mark_state(const commit_mark_t &cmmt);

/**
  Check the proposal gcn state

  @param[in]    pmmt      proposal mark
  @return       state     PROPOSAL_STATE_NULL, PROPOSAL_STATE_ALLOCATED or
                          PROPOSAL_STATE_INVALID
*/
enum proposal_state_t proposal_mark_state(const proposal_mark_t &pmmt);
}  // namespace lizard

#if defined UNIV_DEBUG || defined LIZARD_DEBUG

/* Debug validation of commit scn directly */
#define assert_commit_mark_state(scn, state)       \
  do {                                             \
    ut_a(lizard::commit_mark_state(scn) == state); \
  } while (0)

#define assert_commit_mark_initial(scn)               \
  do {                                                \
    assert_commit_mark_state(scn, SCN_STATE_INITIAL); \
  } while (0)

#define assert_commit_mark_allocated(scn)               \
  do {                                                  \
    assert_commit_mark_state(scn, SCN_STATE_ALLOCATED); \
  } while (0)

/* Debug validation of commit scn from trx->scn */
#define assert_trx_commit_mark_state(trx, state)                  \
  do {                                                            \
    ut_a(lizard::commit_mark_state(trx->txn_desc.cmmt) == state); \
  } while (0)

#define assert_trx_commit_mark_initial(trx)               \
  do {                                                    \
    assert_trx_commit_mark_state(trx, SCN_STATE_INITIAL); \
  } while (0)

#define assert_trx_commit_mark_allocated(trx)               \
  do {                                                      \
    assert_trx_commit_mark_state(trx, SCN_STATE_ALLOCATED); \
  } while (0)

#define assert_trx_commit_mark(trx)                                         \
  do {                                                                      \
    if (trx->state == TRX_STATE_PREPARED || trx->state == TRX_STATE_ACTIVE) \
      assert_trx_commit_mark_state(trx, SCN_STATE_INITIAL);                 \
    else if (trx->state == TRX_STATE_COMMITTED_IN_MEMORY)                   \
      assert_trx_commit_mark_state(trx, SCN_STATE_ALLOCATED);               \
  } while (0)

/* Debug validation of commit scn from undo->scn */
#define assert_undo_commit_mark_state(undo, state)        \
  do {                                                    \
    ut_a(lizard::commit_mark_state(undo->cmmt) == state); \
  } while (0)

#define assert_undo_commit_mark_initial(undo)               \
  do {                                                      \
    assert_undo_commit_mark_state(undo, SCN_STATE_INITIAL); \
  } while (0)

#define assert_undo_commit_mark_allocated(undo)               \
  do {                                                        \
    assert_undo_commit_mark_state(undo, SCN_STATE_ALLOCATED); \
  } while (0)

#else

/* Debug validation of commit scn directly */
#define assert_commit_mark_state(scn, state)
#define assert_commit_mark_initial(scn)
#define assert_commit_mark_allocated(scn)

/* Debug validation of commit scn from trx->scn */
#define assert_trx_commit_mark_state(trx, state)
#define assert_trx_commit_mark_initial(trx)
#define assert_trx_commit_mark_allocated(trx)
#define assert_trx_commit_mark(trx)

/* Debug validation of commit scn from undo->scn */
#define assert_undo_commit_mark_state(undo, state)
#define assert_undo_commit_mark_initial(undo)
#define assert_undo_commit_mark_allocated(undo)

#endif /* UNIV_DEBUG || LIZARD_DEBUG */

#endif /* lizard0scn_h define */
