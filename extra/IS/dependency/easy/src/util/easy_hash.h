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


#ifndef EASY_HASH_H_
#define EASY_HASH_H_

/**
 * 固定HASH桶的hashtable, 需要在使用的对象上定义一个easy_hash_list_t
 */
#include "easy_pool.h"
#include "easy_list.h"
#include "easy_buf.h"

EASY_CPP_START

typedef struct easy_hash_t easy_hash_t;
typedef struct easy_hash_list_t easy_hash_list_t;
typedef struct easy_string_pair_t easy_string_pair_t;
typedef struct easy_hash_string_t easy_hash_string_t;
typedef int (easy_hash_cmp_pt)(const void *a, const void *b);

struct easy_hash_t {
    easy_hash_list_t        **buckets;
    uint32_t                size;
    uint32_t                mask;
    uint32_t                count;
    int16_t                 offset;
    int16_t                 flags;
    uint64_t                seqno;
    easy_list_t             list;
};

struct easy_hash_list_t {
    easy_hash_list_t        *next;
    easy_hash_list_t        **pprev;
    uint64_t                key;
};

// string hash
struct easy_hash_string_t {
    easy_string_pair_t      **buckets;
    uint32_t                size;
    uint32_t                mask;
    uint32_t                count;
    int                     ignore_case;
    easy_list_t             list;
};

struct easy_string_pair_t {
    /* capitalize header name */
    easy_buf_string_t       name;
    easy_buf_string_t       value;
    easy_string_pair_t     *next;
    easy_list_t             list;
    void                   *user_data;
};

#define easy_hash_for_each(i, node, table)                      \
    for(i=0; i<table->size; i++)                                \
        for(node = table->buckets[i]; node; node = node->next)

extern easy_hash_t *easy_hash_create(easy_pool_t *pool, uint32_t size, int offset);
extern int easy_hash_add(easy_hash_t *table, uint64_t key, easy_hash_list_t *list);
extern void easy_hash_clear(easy_hash_t *table);
extern void *easy_hash_find(easy_hash_t *table, uint64_t key);
void *easy_hash_find_ex(easy_hash_t *table, uint64_t key, easy_hash_cmp_pt cmp, const void *a);
extern void *easy_hash_del(easy_hash_t *table, uint64_t key);
extern int easy_hash_del_node(easy_hash_list_t *n);
extern uint64_t easy_hash_key(uint64_t key);
extern uint64_t easy_hash_code(const void *key, int len, unsigned int seed);
extern uint64_t easy_fnv_hashcode(const void *key, int wrdlen, unsigned int seed);

extern int easy_hash_dlist_add(easy_hash_t *table, uint64_t key, easy_hash_list_t *hash, easy_list_t *list);
extern void *easy_hash_dlist_del(easy_hash_t *table, uint64_t key);

// string hash
extern easy_hash_string_t *easy_hash_string_create(easy_pool_t *pool, uint32_t size, int ignore_case);
extern void easy_hash_string_add(easy_hash_string_t *table, easy_string_pair_t *header);
extern easy_string_pair_t *easy_hash_string_get(easy_hash_string_t *table, const char *key, int len);
extern easy_string_pair_t *easy_hash_string_del(easy_hash_string_t *table, const char *key, int len);
extern easy_string_pair_t *easy_hash_pair_del(easy_hash_string_t *table, easy_string_pair_t *pair);

EASY_CPP_END

#endif
