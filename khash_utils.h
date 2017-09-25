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

#ifndef KHASH_UTILS_H
#define KHASH_UTILS_H

typedef struct {
	loff_t p;
	void *value;
} khash_proc_iter_t;

int khash_proc_interator(khash_key_t hash, void *value, void *user_data);

int khash_key_match(khash_key_t *a, khash_key_t *b);

khash_key_t khash_hash_u32(uint32_t key);
khash_key_t khash_hash_u64(uint64_t key);
khash_key_t khash_hash_aligned32(uint32_t *key, int lkey);

#endif
