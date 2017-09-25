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
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/skbuff.h>
#include <linux/jhash.h>

#include "khash.h"
#include "khash_internal.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)
#define GOLDEN_RATIO_32 0x61C88647
#define GOLDEN_RATIO_64 0x61C8864680B583EBull

#ifndef HAVE_ARCH__HASH_32
#define __hash_32 __hash_32_generic
#endif
static inline u32
__hash_32_generic(u32 val)
{
	return val * GOLDEN_RATIO_32;
}

#ifndef HAVE_ARCH_HASH_32
#define hash_32 hash_32_generic
#endif
static inline u32
hash_32_generic(u32 val, unsigned int bits)
{
	/* High bits are more random, so use them. */
	return __hash_32(val) >> (32 - bits);
}

#ifndef HAVE_ARCH_HASH_64
#define hash_64 hash_64_generic
#endif
static __always_inline u32
hash_64_generic(u64 val, unsigned int bits)
{
#if BITS_PER_LONG == 64
	/* 64x64-bit multiply is efficient on all 64-bit processors */
	return val * GOLDEN_RATIO_64 >> (64 - bits);
#else
	/* Hash 64 bits using only 32x32-bit multiply. */
	return hash_32((u32)val ^ __hash_32(val >> 32), bits);
#endif
}
#endif

int
khash_proc_interator(khash_key_t hash, void *value, void *user_data)
{
	khash_proc_iter_t *iter = (khash_proc_iter_t *)user_data;

	if (iter->p--) {
		iter->value = NULL;
		return (0);
	}

	iter->value = value;
	return (1);
}
EXPORT_SYMBOL(khash_proc_interator);

/*
 * Introduce some hashing functions; check for typical cases:
 * ipv4_addr_hash, ipv6_addr_hash, __ipv6_addr_jhash
 */

__always_inline int
khash_key_match(khash_key_t *a, khash_key_t *b)
{
	if (a->key == b->key)
		return (!memcmp(a->__key._64, b->__key._64, _64WORDS_NUM));

	return (0);
}
