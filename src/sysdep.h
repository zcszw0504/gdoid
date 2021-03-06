/* $Id: sysdep.h,v 1.5 2005/10/11 17:57:40 bew Exp $ */
/* $Source: /nfs/cscbz/gdoi/gdoicvs/gdoi/src/sysdep.h,v $ */

/*	$OpenBSD: sysdep.h,v 1.9 2001/02/24 03:59:56 angelos Exp $	*/
/*	$EOM: sysdep.h,v 1.17 2000/12/04 04:46:35 angelos Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#ifndef _SYSDEP_H_
#define _SYSDEP_H_

#include <sys/types.h>
#include "config.h"

#ifdef OSX
#include <stdint.h>
#endif

#ifndef HAVE_STRLCPY
extern size_t strlcpy(char *, const char *, size_t);
#endif

#ifdef DEFINE_EXTRA_QUEUE_FUNCTIONS
#define	LIST_FIRST(head)		((head)->lh_first)
#define	LIST_NEXT(elm, field)		((elm)->field.le_next)
#define	TAILQ_FIRST(head)		((head)->tqh_first)
#define	TAILQ_NEXT(elm, field)		((elm)->field.tqe_next)
#define TAILQ_LAST(head, headname) 					\
        (*(((struct headname *)((head)->tqh_last))->tqh_last))
#define TAILQ_INSERT_BEFORE(listelm, elm, field) do {                   \
        (elm)->field.tqe_prev = (listelm)->field.tqe_prev;              \
        (elm)->field.tqe_next = (listelm);                              \
        *(listelm)->field.tqe_prev = (elm);                             \
        (listelm)->field.tqe_prev = &(elm)->field.tqe_next;             \
} while (0)
#endif

#ifdef DEFINE_SA_LEN
#define SA_LEN(x) 			sizeof(struct sockaddr)
#endif

#ifdef OPENBSD_PFKEY_EXT
#include "sysdep/openbsd/pf_key_ext.h"
#endif

#ifdef FREEBSD_PFKEY_EXT
#include <netipsec/ipsec.h>
#endif

#ifdef NETBSD_PFKEY_EXT
#include <netinet6/ipsec.h>
#endif

struct proto;
struct sa;
struct sockaddr;

extern void sysdep_app_handler (int);
extern int sysdep_app_open (void);
extern int sysdep_cleartext (int);
extern void sysdep_connection_check (char *);
extern int sysdep_ipsec_delete_spi (struct sa *, struct proto *, int);
extern int sysdep_ipsec_enable_sa (struct sa *, struct sa *);
extern u_int8_t *sysdep_ipsec_get_spi (size_t *, u_int8_t, struct sockaddr *,
				       int, struct sockaddr *, int, u_int32_t);
extern int sysdep_ipsec_group_spis (struct sa *, struct proto *,
				    struct proto *, int);
extern int sysdep_ipsec_set_spi (struct sa *, struct proto *, int);
extern char *sysdep_progname (void);

#endif /* _SYSDEP_H_ */
