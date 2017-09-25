/*
 * KHASH
 * An ultra fast hash table in kernel space based on hashtable.h
 * Copyright (C) 2016-2017 - Athonet s.r.l. - All Rights Reserved
 *
 * Authors:
 *         Paolo Missiaggia, <paolo.Missiaggia@athonet.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef KHASH_MGMNT_H
#define KHASH_MGMNT_H

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
#include "hashtable.h"
#else
#include <linux/hashtable.h>
#endif

#include <linux/jhash.h>

#define _64WORDS_NUM (3)
typedef struct {
	union {
		u8   _8[8 * _64WORDS_NUM];
		u16 _16[4 * _64WORDS_NUM];
		u32 _32[2 * _64WORDS_NUM];
		u64 _64[_64WORDS_NUM];
	} __key;
	u32 key;
} khash_key_t;

typedef struct {
	struct rcu_head rcu;
	struct hlist_node hh;
	khash_key_t hash;
	void *value;
} khash_item_t;

typedef struct khash_t khash_t;

typedef int(*khfunc)(khash_key_t hash, void *value, void *user_data);

khash_t *khash_init(uint32_t bck_size); /* Requires non atomic context */
void khash_term(khash_t *khash);
void khash_flush(khash_t *khash);

uint64_t khash_footprint(khash_t *kh);
uint64_t khash_entry_footprint(void);

int khash_size(khash_t *khash);
int khash_addentry(khash_t *khash, khash_key_t hash, void *val, gfp_t flags);

khash_item_t *khash_item_new(khash_key_t hash, void *value, gfp_t flags);
void khash_item_del(khash_item_t *item);
int khash_add_item(khash_t *khash, khash_item_t *item);

int khash_rementry(khash_t *khash, khash_key_t hash, void **retval);
int khash_lookup(khash_t *khash, khash_key_t hash, void **retval);
void khash_foreach(khash_t *khash, khfunc func, void *data);

u32 khash_bck_size_get(khash_t *kh);
struct hlist_head *khash_bck_get(khash_t *kh, uint32_t idx);

__always_inline static u32
hash_128(uint64_t _u64[2])
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS) && BITS_PER_LONG == 64
	unsigned long x = _u64[0] ^ _u64[1];

	return (u32)(x ^ (x >> 32));
#else
	uint32_t *_u32 = (u32 *)_u64;
	return (__force u32)(_u32[0] ^ _u32[1] ^ _u32[2] ^ _u32[3]);
#endif
}

/*
 *
 */
__always_inline static khash_key_t *
__khash_hash_u32(khash_key_t *key, u32 _u32)
{
	key->__key._64[0] = _u32;
	key->key = hash_64(key->__key._64[0], 32);

	return (key);
}

__always_inline static khash_key_t *
__khash_hash_u64(khash_key_t *key, u64 _u64)
{
	key->__key._64[0] = _u64;
	key->key = hash_64(key->__key._64[0], 32);

	return (key);
}

__always_inline static khash_key_t *
__khash_hash_u128(khash_key_t *key, u64 _u64[2])
{
	memcpy(key->__key._64, _u64, 2);
	key->key = hash_128(_u64);

	return (key);
}

__always_inline static khash_key_t *
__khash_hash_u160(khash_key_t *key, u64 _u64[2], u32 _u32)
{
	u64 _key;

	memcpy(key->__key._64, _u64, 2);
	key->__key._64[2] = _u32;

	_key =(((u64)hash_128(_u64)) << 32) | _u32;

	key->key = hash_64(_key, 32);

	return (key);
}

__always_inline static khash_key_t *
__khash_hash_aligned32(khash_key_t *key, u32 _u32[], int l_u32)
{
	key->key = jhash2(_u32, l_u32, JHASH_INITVAL);

	return (key);
}

/*
 *
 */

__always_inline static khash_key_t
khash_hash_u32(uint32_t _u32)
{
	khash_key_t key = {};

	__khash_hash_u32(&key, _u32);

	return (key);
}

__always_inline static khash_key_t
khash_hash_u64(uint64_t _u64)
{
	khash_key_t key = {};

	__khash_hash_u64(&key, _u64);

	return (key);
}

__always_inline static khash_key_t
khash_hash_aligned32(u32 _u32[], int l_u32)
{
	khash_key_t key = {};

	__khash_hash_aligned32(&key, _u32, l_u32);

	return (key);
}

/*
 * AAA:
 * hash_for_each_safe (if no RCU) should be used if del is needed
 */

#define HLIST_FOR_EACH hlist_for_each_entry_rcu

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
/* A "struct hlist_node *" pointer has to be passed as third argument */
#define KHASH_ITER(kh, i, node, obj)                                           \
	for ((i) = 0, (node) = NULL;                                               \
			(kh) && (node) == NULL && (i) < khash_bck_size_get(kh); (i)++)     \
		HLIST_FOR_EACH(item, node, khash_bck_get(kh, i), hh)
#else
#define KHASH_ITER(kh, i, item)                                                \
	for ((i) = 0, (item) = NULL;                                               \
			(kh) && ((item) == NULL) && ((i) < khash_bck_size_get(kh)); (i)++) \
		HLIST_FOR_EACH((item), khash_bck_get(kh, i), hh)
#endif

#define PRECISION 1000
#define MAX_STATISTICAL_MODE 25
typedef struct {
	uint32_t count;
	uint64_t mean;
	uint64_t std_dev;
	uint64_t min;
	uint64_t min_counter;
	uint64_t max;
	uint64_t max_counter;
	uint64_t stat_mode;
	uint64_t stat_mode_counter;
	uint32_t bucket_number;
	uint32_t x_axis[MAX_STATISTICAL_MODE];
	uint32_t statistical_mode[MAX_STATISTICAL_MODE];
} khash_stats_t;

int khash_stats_get(khash_t *khash, khash_stats_t *stats);

#endif
