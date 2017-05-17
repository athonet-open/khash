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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/ktime.h>

#include "khash.h"
#include "khash_mgmnt.h"
#include "khash_internal.h"

#define KHASH_ADD               hash_add_rcu
#define KHASH_DEL               hash_del_rcu
#define KHASH_FOR_EACH_POSSIBLE hash_for_each_possible_rcu
#define KHASH_FOR_EACH          hash_for_each_rcu

khash_item_t *
khash_item_new(khash_key_t hash, void *value, gfp_t flags)
{
	khash_item_t *item = NULL;

	item = kzalloc(sizeof(khash_item_t), flags);
	if (!item)
		return (NULL);

	item->hash = hash;
	item->value = value;

	return (item);
}
EXPORT_SYMBOL(khash_item_new);

void
khash_item_del(khash_item_t *item)
{
	if (unlikely(!item))
		return;

	kfree(item);
}
EXPORT_SYMBOL(khash_item_del);

__always_inline static void *
khash_item_value_get(khash_item_t *item)
{
	if (unlikely(!item))
		return (NULL);

	return (item->value);
}

__always_inline static int
khash_item_key_get(khash_item_t *item, khash_key_t *key)
{
	if (unlikely(!item || !key))
		return (-1);

	*key = item->hash;

	return (0);
}

__always_inline static int
khash_item_value_set(khash_item_t *item, void *value)
{
	if (unlikely(!item))
		return (-1);

	item->value = value;

	return (0);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
#define KHASH_BUCKET_LOOKUP(__kh__, _hh_, __item__, __f__)                    \
	do {                                                                      \
		struct hlist_node *n;                                                 \
		KHASH_FOR_EACH_POSSIBLE((__kh__)->ht, __item__, n, hh, (_hh_).key) {  \
			if (khash_key_match(&(__item__)->hash, &(_hh_))) {                \
				__f__++;                                                      \
				break;                                                        \
			}                                                                 \
		}                                                                     \
	} while (0)
#else
#define KHASH_BUCKET_LOOKUP(__kh__, _hh_, __item__, __f__)                    \
	do {                                                                      \
		KHASH_FOR_EACH_POSSIBLE((__kh__)->ht, __item__, hh, (_hh_).key) {     \
			if (khash_key_match(&(__item__)->hash, &(_hh_))) {                \
				__f__++;                                                      \
				break;                                                        \
			}                                                                 \
		}                                                                     \
	} while (0)
#endif

__always_inline static khash_item_t *
__khash_lookup(khash_t *kh, khash_key_t hash)
{
	khash_item_t *item = NULL;
	uint8_t found = 0;

	if (likely(kh->bck_size == KHASH_BCK_SIZE_512k))
		KHASH_BUCKET_LOOKUP((khash_512k_t *)kh, hash, item, found);
	else if (likely(kh->bck_size == KHASH_BCK_SIZE_1k))
		KHASH_BUCKET_LOOKUP((khash_1k_t *)kh, hash, item, found);
	else
		KHASH_BUCKET_LOOKUP((khash_16_t *)kh, hash, item, found);

	if (!found)
		return (NULL);

	return (item);
}

__always_inline static uint32_t
khash_hash_idx_get(khash_t *kh, khash_key_t hash)
{
	uint32_t idx;

	if (likely(kh->bck_size == KHASH_BCK_SIZE_512k))
		idx = hash_min(hash.key, HASH_BITS(((khash_512k_t *)kh)->ht));
	else if (likely(kh->bck_size == KHASH_BCK_SIZE_1k))
		idx = hash_min(hash.key, HASH_BITS(((khash_1k_t *)kh)->ht));
	else
		idx = hash_min(hash.key, HASH_BITS(((khash_16_t *)kh)->ht));

	return (idx);
}

uint64_t
khash_footprint(khash_t *kh)
{
	if (unlikely(!kh))
		return (0);

	switch (kh->bck_size) {
	case KHASH_BCK_SIZE_512k:
		return (sizeof(khash_512k_t));
	case KHASH_BCK_SIZE_1k:
		return (sizeof(khash_1k_t));
	case KHASH_BCK_SIZE_16:
	default:
		return (sizeof(khash_t));
	}
}
EXPORT_SYMBOL(khash_footprint);

uint64_t
khash_entry_footprint(void)
{
	return (sizeof(khash_item_t));
}
EXPORT_SYMBOL(khash_entry_footprint);

khash_t *
khash_init(uint32_t bck_size)
{
	khash_t *khash = NULL;

	if (bck_size <= KHASH_BCK_SIZE_16)
		bck_size = KHASH_BCK_SIZE_16;
	else if (bck_size <= KHASH_BCK_SIZE_1k)
		bck_size = KHASH_BCK_SIZE_1k;
	else
		bck_size = KHASH_BCK_SIZE_512k;

	switch (bck_size) {
	case KHASH_BCK_SIZE_512k:
		khash = vzalloc(sizeof(khash_512k_t));
		if (unlikely(!khash))
			return (NULL);

		hash_init(((khash_512k_t *)khash)->ht);
		break;
	case KHASH_BCK_SIZE_1k:
		khash = vzalloc(sizeof(khash_1k_t));
		if (unlikely(!khash))
			return (NULL);

		hash_init(((khash_1k_t *)khash)->ht);
		break;
	case KHASH_BCK_SIZE_16:
	default:
		khash = vzalloc(sizeof(khash_16_t));
		if (unlikely(!khash))
			return (NULL);

		hash_init(((khash_16_t *)khash)->ht);
		break;
	}

	khash->bck_size = bck_size;

	return (khash);
}
EXPORT_SYMBOL(khash_init);

__always_inline void
__khash_rementry(khash_t *khash, khash_item_t *item)
{
	khash->ht_count[khash_hash_idx_get(khash, item->hash)]--;
	khash->count--;

	KHASH_DEL(&item->hh);

	kfree_rcu(item, rcu);
}

void
khash_flush(khash_t *kh)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	struct hlist_node *n;
#endif
	khash_item_t *item = NULL;
	int idx;

	if (!kh)
		return;

	switch (kh->bck_size) {
	case KHASH_BCK_SIZE_512k:
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
		KHASH_FOR_EACH(((khash_512k_t *)kh)->ht, idx, n, item, hh) {
#else
		KHASH_FOR_EACH(((khash_512k_t *)kh)->ht, idx, item, hh) {
#endif
			__khash_rementry(kh, item);
		}
		break;
	case KHASH_BCK_SIZE_1k:
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
		KHASH_FOR_EACH(((khash_1k_t *)kh)->ht, idx, n, item, hh) {
#else
		KHASH_FOR_EACH(((khash_1k_t *)kh)->ht, idx, item, hh) {
#endif
			__khash_rementry(kh, item);
		}
		break;
	case KHASH_BCK_SIZE_16:
	default:
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
		KHASH_FOR_EACH(((khash_16_t *)kh)->ht, idx, n, item, hh) {
#else
		KHASH_FOR_EACH(((khash_16_t *)kh)->ht, idx, item, hh) {
#endif
			__khash_rementry(kh, item);
		}
		break;
	}
}
EXPORT_SYMBOL(khash_flush);

void
khash_term(khash_t *kh)
{
	if (unlikely(!kh))
		return;

	khash_flush(kh);

	if (kh->ht_is_static)
		memset(kh, 0, sizeof(*kh));
	else
		vfree(kh);
}
EXPORT_SYMBOL(khash_term);

int
khash_rementry(khash_t *khash, khash_key_t hash, void **retval)
{
	khash_item_t *item = NULL;
	void *value = NULL;

	if (!khash)
		goto khash_rementry_fail;

	item = __khash_lookup(khash, hash);
	if (!item)
		goto khash_rementry_fail;

	value = item->value;

	__khash_rementry(khash, item);

	if (retval)
		*retval = value;
	return (0);

khash_rementry_fail:
	if (retval)
		*retval = NULL;
	return (-1);
}
EXPORT_SYMBOL(khash_rementry);

int
khash_add_item(khash_t *khash, khash_item_t *item)
{
	khash_item_t *old_item = NULL;

	if (!khash || !item)
		return (-1);

	old_item = __khash_lookup(khash, item->hash);
	if (old_item)
		return (-1);

	switch (khash->bck_size) {
	case KHASH_BCK_SIZE_512k:
		KHASH_ADD(((khash_512k_t *)khash)->ht, &item->hh, item->hash.key);
		break;
	case KHASH_BCK_SIZE_1k:
		KHASH_ADD(((khash_1k_t *)khash)->ht, &item->hh, item->hash.key);
		break;
	case KHASH_BCK_SIZE_16:
	default:
		KHASH_ADD(((khash_16_t *)khash)->ht, &item->hh, item->hash.key);
		break;
	}

	khash->ht_count[khash_hash_idx_get(khash, item->hash)]++;
	khash->count++;

	return (0);
}
EXPORT_SYMBOL(khash_add_item);

int
khash_addentry(khash_t *khash, khash_key_t hash, void *value, gfp_t flags)
{
	khash_item_t *item = NULL;

	if (unlikely(!khash))
		return (-1);

	item = khash_item_new(hash, value, flags);
	if (unlikely(!item))
		return (-1);

	if (khash_add_item(khash, item) < 0) {
		kfree(item);
		return (-1);
	}

	return (0);
}
EXPORT_SYMBOL(khash_addentry);

int
khash_lookup(khash_t *khash, khash_key_t hash, void **retval)
{
	khash_item_t *item = NULL;

	if (unlikely(!khash))
		goto khash_lookup_fail;

	item = __khash_lookup(khash, hash);
	if (!item)
		goto khash_lookup_fail;

	if (retval)
		*retval = (rcu_dereference(item))->value;

	return (0);

khash_lookup_fail:
	if (retval)
		*retval = NULL;

	return (-1);
}
EXPORT_SYMBOL(khash_lookup);

int
khash_size(khash_t *khash)
{
	if (unlikely(!khash))
        return (-1);

	return (khash->count);
}
EXPORT_SYMBOL(khash_size);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
#define KHASH_FOREACH(__kh__, __idx__, __item__, __func__, __data__)        \
	do {                                                                    \
		struct hlist_node *n;                                               \
		KHASH_FOR_EACH((__kh__)->ht, __idx__, n, __item__, hh) {            \
			if (__func__((__item__)->hash, (__item__)->value, __data__))    \
				break;                                                      \
		}                                                                   \
	} while (0)
#else
#define KHASH_FOREACH(__kh__, __idx__, __item__, __func__, __data__)        \
	do {                                                                    \
		KHASH_FOR_EACH((__kh__)->ht, __idx__, __item__, hh) {               \
			if (__func__((__item__)->hash, (__item__)->value, __data__))    \
				break;                                                      \
		}                                                                   \
	} while (0)
#endif

__always_inline u32
khash_bck_size_get(khash_t *kh)
{
	return (kh->bck_size);
}

__always_inline struct hlist_head *
khash_bck_get(khash_t *kh, uint32_t idx)
{
	return (&kh->ht[idx]);
}

void
khash_foreach(khash_t *khash, khfunc func, void *data)
{
	khash_item_t *item = NULL;
	int idx;

	if (unlikely(!khash || !func))
		return;

	switch (khash->bck_size) {
	case KHASH_BCK_SIZE_512k:
		KHASH_FOREACH((khash_512k_t *)khash, idx, item, func, data);
		break;
	case KHASH_BCK_SIZE_1k:
		KHASH_FOREACH((khash_1k_t *)khash, idx, item, func, data);
		break;
	case KHASH_BCK_SIZE_16:
	default:
		KHASH_FOREACH((khash_16_t *)khash, idx, item, func, data);
		break;
	}
}
EXPORT_SYMBOL(khash_foreach);

__always_inline static uint64_t
sqrt_u64(uint64_t a)
{
	uint64_t max = ((uint64_t) 1) << 32;
	uint64_t min = 0;
	uint64_t sqt = 0;
	uint64_t sq = 0;

	while (1) {
		if (max <= (1 + min))
			return min;

		sqt = min + (max - min) / 2;
		sq = sqt * sqt;

		if (sq == a)
			return sqt;

		if (sq > a)
			max = sqt;
		else
			min = sqt;
	}

	return (0);
}

__always_inline static int64_t
sqrt_s64(int64_t a)
{
	return ((int64_t)sqrt_u64(a));
}

int
khash_stats_get(khash_t *khash, khash_stats_t *stats)
{
	uint64_t i, variance = 0, tmp;

	if (unlikely(!khash || !stats || !khash->bck_size))
		return (-1);

	memset(stats, 0, sizeof(khash_stats_t));
	stats->min = 1000000;

	stats->count = khash->count;

	for (i = 0; i < khash->bck_size; i++) {
		tmp = khash->ht_count[i];
		if (tmp < stats->min) {
			stats->min = tmp;
			stats->min_counter = 1;
		} else if (tmp == stats->min) {
			stats->min_counter++;
		}

		if (tmp > stats->max) {
			stats->max = tmp;
			stats->max_counter = 1;
		} else if (tmp == stats->max) {
			stats->max_counter++;
		}

		stats->statistical_mode[tmp]++;

		tmp *= PRECISION;
		stats->mean += tmp;
		variance += tmp * tmp;
	}

	stats->mean /= khash->bck_size;
	variance = (variance / khash->bck_size) - (stats->mean * stats->mean);
	stats->std_dev = sqrt_u64(variance);

	stats->mean /= PRECISION;
	stats->std_dev /= PRECISION;

	tmp = (MAX_STATISTICAL_MODE < stats->max)?MAX_STATISTICAL_MODE:stats->max;
	for(i = 0; i < tmp; i++) {
		if (stats->statistical_mode[i] > stats->stat_mode_counter) {
			stats->stat_mode = i;
			stats->stat_mode_counter = stats->statistical_mode[i];
		}
	}

	for (i = 0; i < MAX_STATISTICAL_MODE; i++)
		stats->x_axis[i] = i;

	stats->bucket_number = khash->bck_size;

	return (0);
}
EXPORT_SYMBOL(khash_stats_get);

int
khash_init_module(void)
{
	printk(KERN_INFO "[%s] module loaded\n", KHASH_VERSION_STR);

	return 0;
}

void
khash_exit_module(void)
{
	printk(KERN_INFO "[%s] module unloaded\n", KHASH_VERSION_STR);
}

module_init(khash_init_module);
module_exit(khash_exit_module);

MODULE_AUTHOR("Paolo Missiaggia <paolo.missiaggia@athonet.com>");
MODULE_DESCRIPTION("Kernel ultrafast HASH tables");
MODULE_VERSION(KHASH_VERSION_STR);

MODULE_LICENSE("GPL");
