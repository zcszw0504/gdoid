/* $Id: prf.c,v 1.3 2007/03/21 20:03:05 bew Exp $ */
/* $Source: /nfs/cscbz/gdoi/gdoicvs/gdoi/src/prf.c,v $ */

/*	$OpenBSD: prf.c,v 1.7 1999/05/02 19:16:41 niklas Exp $	*/
/*	$EOM: prf.c,v 1.7 1999/05/02 12:50:29 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 * Copyright (c) 1999 Niklas Hallqvist.  All rights reserved.
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

#include <sys/param.h>
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

#include "hash.h"
#include "log.h"
#include "prf.h"

void prf_hash_init (struct prf_hash_ctx *);
void prf_hash_update (struct prf_hash_ctx *, unsigned char *, unsigned int);
void prf_hash_final (unsigned char *, struct prf_hash_ctx *);

/* PRF behaves likes a hash */

void
prf_hash_init (struct prf_hash_ctx *ctx)
{
  memcpy (ctx->hash->ctx, ctx->ctx, ctx->hash->ctxsize);
  memcpy (ctx->hash->ctx2, ctx->ctx2, ctx->hash->ctxsize);
}

void
prf_hash_update (struct prf_hash_ctx *ctx, unsigned char *data,
		 unsigned int len)
{
  ctx->hash->Update (ctx->hash->ctx, data, len);
}

void
prf_hash_final (unsigned char *digest, struct prf_hash_ctx *ctx)
{
  ctx->hash->HMACFinal (digest, ctx->hash);
}

/*
 * Obtain a Pseudo-Random Function for us. At the moment this is
 * the HMAC version of a hash. See RFC-2104 for reference.
 */
struct prf *
prf_alloc (enum prfs type, int subtype, char *shared, int sharedsize)
{
  struct hash *hash;
  struct prf *prf;
  struct prf_hash_ctx *prfctx;

  switch (type)
    {
    case PRF_HMAC:
    case PRF_HASH:
      hash = hash_get (subtype);
      if (!hash)
	{
	  log_print ("prf_alloc: unknown hash type %d", subtype);
	  return 0;
	}
      break;
    default:
      log_print ("prf_alloc: unknown PRF type %d", type);
      return 0;
    }

  prf = malloc (sizeof *prf);
  if (!prf)
    {
      log_error ("prf_alloc: malloc (%d) failed", sizeof *prf);
      return 0;
    }

  switch (type)
    {
    case PRF_HMAC:
    case PRF_HASH:
      /* Obtain needed memory.  */
      prfctx = malloc (sizeof *prfctx);
      if (!prfctx)
	{
	  log_error ("prf_alloc: malloc (%d) failed", sizeof *prfctx);
	  goto cleanprf;
	}
      prf->prfctx = prfctx;

      prfctx->ctx = malloc (hash->ctxsize);
      if (!prfctx->ctx)
	{
	  log_error ("prf_alloc: malloc (%d) failed", hash->ctxsize);
	  goto cleanprfctx;
	}

      prfctx->ctx2 = malloc (hash->ctxsize);
      if (!prfctx->ctx2)
	{
	  log_error ("prf_alloc: malloc (%d) failed", hash->ctxsize);
	  free (prfctx->ctx);
	  goto cleanprfctx;
	}
      prf->type = PRF_HMAC;
      prf->blocksize = hash->hashsize;
      prfctx->hash = hash;

      /* Use the correct function pointers.  */
      prf->Init = (void (*) (void *))prf_hash_init;
      prf->Update
	= (void (*) (void *, unsigned char *, unsigned int))prf_hash_update;
      prf->Final = (void (*) (unsigned char *, void *))prf_hash_final;

      if (type == PRF_HMAC)
        {
      	  /* Init HMAC contexts.  */
      	  hash->HMACInit (hash, (unsigned char *)shared, sharedsize);
        }

      /* Save contexts.  */
      memcpy (prfctx->ctx, hash->ctx, hash->ctxsize);
      memcpy (prfctx->ctx2, hash->ctx2, hash->ctxsize);
      break;
    }

  return prf;

 cleanprfctx:
  free (prf->prfctx);
 cleanprf:
  free (prf);
  return 0;
}

/* Deallocate the PRF pointed to by PRF.  */
void
prf_free (struct prf *prf)
{
  struct prf_hash_ctx *prfctx = prf->prfctx;

  if (prf->type == PRF_HMAC)
    {
      free (prfctx->ctx2);
      free (prfctx->ctx);
    }
  free (prf->prfctx);
  free (prf);
}
