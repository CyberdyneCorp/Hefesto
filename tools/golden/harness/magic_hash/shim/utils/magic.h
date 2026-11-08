/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The Hefesto Authors
 *
 * Minimal stand-in for Magic's utils/magic.h, providing only what Magic's
 * utils/hash.c, utils/hash.h and utils/malloc.h need. Magic's real header pulls
 * in zlib, generated build info and platform configuration that the hash table
 * does not use. Definitions below copy Magic 8.3.694 (fc5daa9e) utils/magic.h,
 * whose notice is kept as its license requires:
 *
 *     *********************************************************************
 *     * Copyright (C) 1985, 1990 Regents of the University of California. *
 *     * Permission to use, copy, modify, and distribute this              *
 *     * software and its documentation for any purpose and without        *
 *     * fee is hereby granted, provided that the above copyright          *
 *     * notice appear in all copies.  The University of California        *
 *     * makes no representations about the suitability of this            *
 *     * software for any purpose.  It is provided "as is" without         *
 *     * express or implied warranty.  Export of this software outside     *
 *     * of the United States of America may require an export license.    *
 *     *********************************************************************
 *
 * Test tooling only: this file is never built into or shipped with Hefesto.
 */
#ifndef _MAGIC__UTILS__MAGIC_H
#define _MAGIC__UTILS__MAGIC_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef intptr_t spointertype;
typedef uintptr_t pointertype;
typedef void *ClientData;

#define ASSERT(cond, msg) \
    do { if (!(cond)) { fprintf(stderr, "assertion failed: %s\n", msg); abort(); } } while (0)

/* utils/magic.h:303 */
#define FLEX_UNION_MEMBER(member, member_no) struct {\
    char __empty ## member_no [0]; member;\
}

#endif /* _MAGIC__UTILS__MAGIC_H */
