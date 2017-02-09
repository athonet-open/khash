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

typedef struct {
	union {
		u8 _8[8];
		u16 _16[4];
		u32 _32[2];
		u64 _64;
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
int khash_addentry2(khash_t *khash, khash_item_t *item);
int khash_rementry(khash_t *khash, khash_key_t hash, void **retval);
int khash_lookup(khash_t *khash, khash_key_t hash, void **retval);
void khash_foreach(khash_t *khash, khfunc func, void *data);

u32 khash_bck_size_get(khash_t *kh);
struct hlist_head *khash_bck_get(khash_t *kh, uint32_t idx);

__always_inline static khash_key_t
khash_hash_u32(uint32_t _u32)
{
	khash_key_t hash = {};

	hash.__key._64 = _u32;
	hash.key = hash_64(hash.__key._64, 32);

	return (hash);
}

__always_inline static khash_key_t
khash_hash_u64(uint64_t _u64)
{
	khash_key_t hash = {};

	hash.__key._64 = _u64;
	hash.key = hash_64(hash.__key._64, 32);

	return (hash);
}

__always_inline static khash_key_t
khash_hash_aligned32(uint32_t *key, int lkey)
{
	khash_key_t hash = {};

	hash.key = jhash2(key, lkey, JHASH_INITVAL);

	return (hash);
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
