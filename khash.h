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

#ifndef KHASH_H
#define KHASH_H

#define KHASH_NAME  "KHASH"
#define KHASH_MAJOR 1
#define KHASH_MINOR 0
#define KHASH_PATCH 0
#define KHASH_STR_H(x) #x
#define KHASH_STR(x) KHASH_STR_H(x)
#define KHASH_VERSION(a, b, c) (((a) << 24) + ((b) << 16) + (c))
#define KHASH_VERSION_STR \
		KHASH_NAME ": " KHASH_STR(KHASH_MAJOR) "." \
		KHASH_STR(KHASH_MINOR) "." KHASH_STR(KHASH_PATCH)

#include "khash_mgmnt.h"
#include "khash_utils.h"

#endif
