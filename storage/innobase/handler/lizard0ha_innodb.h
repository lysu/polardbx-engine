/* Copyright (c) 2008, 2018, Alibaba and/or its affiliates. All rights reserved.

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

#include "handler.h"
#include "mysql/plugin.h"

struct trx_t;

extern bool xa_compare_xid_between_thd_and_trx(const THD *thd,
                                               const trx_t *trx);

/** Like innobase_register_trx. But it only register as TRANS level (no STMT
LEVEL). */
void innobase_register_trx_only_trans(handlerton *hton, THD *thd, trx_t *trx);

/**
  Initialize innobase extension.

  param[in]  innobase_hton  handlerton of innobase.
*/
void innobase_init_ext(handlerton *innobase_hton);

/**
 * InnoDB storage copy external commit number (gcn) if assigned by user when
 * commit
 *
 * @param[in]		user context
 * @param[in/out]	innobase trx context */
extern void innobase_copy_user_commit(THD *thd, trx_t *trx);
/**
 * InnoDB storage copy external proposal number (gcn) if assigned by user when
 * prepare
 * @param[in/out]	user context
 * @param[in/out]	innobase trx context */
extern void innobase_copy_user_prepare(THD *thd, trx_t *trx);
