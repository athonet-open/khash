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

#ifndef KHASH_H
#define KHASH_H

#define KHASH_NAME  "KHASH"
#define KHASH_MAJOR 1
#define KHASH_MINOR 3
#define KHASH_PATCH 3
#define KHASH_STR_H(x) #x
#define KHASH_STR(x) KHASH_STR_H(x)
#define KHASH_VERSION(a, b, c) (((a) << 24) + ((b) << 16) + (c))
#define KHASH_VERSION_STR \
		KHASH_NAME ": " KHASH_STR(KHASH_MAJOR) "." \
		KHASH_STR(KHASH_MINOR) "." KHASH_STR(KHASH_PATCH)

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)
#define GOLDEN_RATIO_32 0x61C88647
#define GOLDEN_RATIO_64 0x61C8864680B583EBull

#ifndef HAVE_ARCH__HASH_32
#define __hash_32_khash __hash_32_generic_khash
#endif
static inline u32
__hash_32_generic_khash(u32 val)
{
	return val * GOLDEN_RATIO_32;
}

#ifndef HAVE_ARCH_HASH_32
#define hash_32_khash hash_32_generic_khash
#endif
static inline u32
hash_32_generic_khash(u32 val, unsigned int bits)
{
	/* High bits are more random, so use them. */
	return __hash_32_khash(val) >> (32 - bits);
}

#ifndef HAVE_ARCH_HASH_64
#define hash_64_khash hash_64_generic_khash
#endif
static __always_inline u32
hash_64_generic_khash(u64 val, unsigned int bits)
{
#if BITS_PER_LONG == 64
	/* 64x64-bit multiply is efficient on all 64-bit processors */
	return val * GOLDEN_RATIO_64 >> (64 - bits);
#else
	/* Hash 64 bits using only 32x32-bit multiply. */
	return hash_32_khash((u32)val ^ __hash_32_khash(val >> 32), bits);
#endif
}
#endif

__always_inline static u32
hash_128_khash(uint64_t _u64[2])
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
 * Key struct and some evaluation functions
 */

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

__always_inline static int
khash_key_match(khash_key_t *a, khash_key_t *b)
{
	if (a->key == b->key)
		return (!memcmp(a->__key._64, b->__key._64, _64WORDS_NUM));

	return (0);
}

__always_inline static khash_key_t *
__khash_hash_u32(khash_key_t *key, u32 _u32)
{
	key->__key._64[0] = _u32;
	key->key = hash_64_khash(key->__key._64[0], 32);

	return (key);
}

__always_inline static khash_key_t *
__khash_hash_u64(khash_key_t *key, u64 _u64)
{
	key->__key._64[0] = _u64;
	key->key = hash_64_khash(key->__key._64[0], 32);

	return (key);
}

__always_inline static khash_key_t *
__khash_hash_u128(khash_key_t *key, u64 _u64[2])
{
	memcpy(key->__key._64, _u64, 2);
	key->key = hash_128_khash(_u64);

	return (key);
}

__always_inline static khash_key_t *
__khash_hash_u160(khash_key_t *key, u64 _u64[2], u32 _u32)
{
	u64 _key;

	memcpy(key->__key._64, _u64, 2);
	key->__key._64[2] = _u32;

	_key =(((u64)hash_128_khash(_u64)) << 32) | _u32;

	key->key = hash_64_khash(_key, 32);

	return (key);
}

__always_inline static khash_key_t *
__khash_hash_aligned32(khash_key_t *key, u32 _u32[], int l_u32)
{
	key->key = jhash2(_u32, l_u32, JHASH_INITVAL);

	return (key);
}

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

#include "khash_mgmnt.h"

typedef struct {
	loff_t p;
	void *value;
} khash_proc_iter_t;

__always_inline static int
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

#endif
