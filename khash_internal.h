/*
 * KHASH
 * An ultra fast hash table in kernel space based on hashtable.h
 * Copyright (C) 2016-2017 - Athonet s.r.l. - All Rights Reserved
 *
 * Authors:
 *         Paolo Missiaggia, <paolo.Missiaggia@athonet.com>
 *         Paolo Missiaggia, <paolo.ratm@gmail.com>
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

#ifndef KHASH_INTERNAL_H
#define KHASH_INTERNAL_H

#define DEFINE_KHASH_STRUCT(__bucket_size__)         \
		uint32_t          count;                     \
		uint8_t           ht_is_static;              \
		uint8_t           ht_static_idx;             \
		uint32_t          bck_size;                  \
		uint32_t          ht_count[__bucket_size__]; \
		struct hlist_head ht[__bucket_size__];

/* Hash table size MUST be a power of 2 */
#define KHASH_BCK_SIZE_16    (1 << 4)
#define KHASH_BCK_SIZE_1k    (1 << 10)
#define KHASH_BCK_SIZE_512k  (1 << 19)

struct khash_t {
	DEFINE_KHASH_STRUCT(KHASH_BCK_SIZE_16)
};

typedef struct khash_t khash_16_t;

typedef struct {
	DEFINE_KHASH_STRUCT(KHASH_BCK_SIZE_1k)
} khash_1k_t;

typedef struct {
	DEFINE_KHASH_STRUCT(KHASH_BCK_SIZE_512k)
} khash_512k_t;

#endif
