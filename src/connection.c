/* $Id: connection.c,v 1.6.2.1 2011/10/18 03:26:54 bew Exp $ */
/* $Source: /nfs/cscbz/gdoi/gdoicvs/gdoi/src/connection.c,v $ */

/*	$OpenBSD: connection.c,v 1.17 2001/03/14 21:13:24 tholo Exp $	*/
/*	$EOM: connection.c,v 1.28 2000/11/23 12:21:18 niklas Exp $	*/

/* 
 * The license applies to all software incorporated in the "Cisco GDOI reference
 * implementation" except for those portions incorporating third party software 
 * specifically identified as being licensed under separate license. 
 *  
 *  
 * The Cisco Systems Public Software License, Version 1.0 
 * Copyright (c) 2001-2011 Cisco Systems, Inc. All rights reserved.
 * Subject to the following terms and conditions, Cisco Systems, Inc., 
 * hereby grants you a worldwide, royalty-free, nonexclusive, license, 
 * subject to third party intellectual property claims, to create 
 * derivative works of the Licensed Code and to reproduce, display, 
 * perform, sublicense, distribute such Licensed Code and derivative works. 
 * All rights not expressly granted herein are reserved. 
 * 1.      Redistributions of source code must retain the above 
 * copyright notice, this list of conditions and the following 
 * disclaimer.
 * 2.      Redistributions in binary form must reproduce the above 
 * copyright notice, this list of conditions and the following 
 * disclaimer in the documentation and/or other materials 
 * provided with the distribution.
 * 3.      The names Cisco and "Cisco GDOI reference implementation" must not 
 * be used to endorse or promote products derived from this software without 
 * prior written permission. For written permission, please contact 
 * opensource@cisco.com.
 * 4.      Products derived from this software may not be called 
 * "Cisco" or "Cisco GDOI reference implementation", nor may "Cisco" or 
 * "Cisco GDOI reference implementation" appear in 
 * their name, without prior written permission of Cisco Systems, Inc.
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
 * PURPOSE, TITLE AND NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
 * SHALL CISCO SYSTEMS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. THIS LIMITATION OF LIABILITY SHALL NOT APPLY TO 
 * LIABILITY FOR DEATH OR PERSONAL INJURY RESULTING FROM SUCH 
 * PARTY'S NEGLIGENCE TO THE EXTENT APPLICABLE LAW PROHIBITS SUCH 
 * LIMITATION. SOME JURISDICTIONS DO NOT ALLOW THE EXCLUSION OR 
 * LIMITATION OF INCIDENTAL OR CONSEQUENTIAL DAMAGES, SO THAT 
 * EXCLUSION AND LIMITATION MAY NOT APPLY TO YOU. FURTHER, YOU 
 * AGREE THAT IN NO EVENT WILL CISCO'S LIABILITY UNDER OR RELATED TO 
 * THIS AGREEMENT EXCEED AMOUNT FIVE THOUSAND DOLLARS (US) 
 * (US$5,000). 
 *  
 * ====================================================================
 * This software consists of voluntary contributions made by Cisco Systems, 
 * Inc. and many individuals on behalf of Cisco Systems, Inc. For more 
 * information on Cisco Systems, Inc., please see <http://www.cisco.com/>.
 *
 * This product includes software developed by Ericsson Radio Systems.
 */ 


/*
 * Copyright (c) 1999, 2000, 2001 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 1999 Hakan Olsson.  All rights reserved.
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

#include <sys/queue.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

#include "conf.h"
#include "connection.h"
#include "doi.h"
#include "ipsec.h"
#include "gdoi_phase2.h"

/* XXX isakmp.h only required for compare_ids().  */
#include "isakmp.h"

#include "log.h"
#include "timer.h"
#include "util.h"

/* How often should we check that connections we require to be up, are up?  */
#define CHECK_INTERVAL 60

struct connection
{
  TAILQ_ENTRY (connection) link;
  char *name;
  struct event *ev;
};

struct connection_passive
{
  TAILQ_ENTRY (connection_passive) link;
  char *name;
  u_int8_t *local_id, *remote_id, *group_id;
  size_t local_sz, remote_sz, group_sz;

#if 0
  /* XXX Potential additions to 'connection_passive'.  */
  char *isakmp_peer;
  struct sa *sa;                /* XXX "Soft" ref to active sa?  */
  struct timeval sa_expiration; /* XXX *sa may expire.  */
#endif
};

TAILQ_HEAD (connection_head, connection) connections;
TAILQ_HEAD (passive_head, connection_passive) connections_passive;

/*
 * This is where we setup all the connections we want there right from the
 * start.
 */
void
connection_init ()
{
  struct conf_list *conns, *attrs;
  struct conf_list_node *conn, *attr = NULL;

  /*
   * Passive connections normally include: all "active" connections that
   * are not flagged "Active-Only", plus all connections listed in
   * the 'Passive-Connections' list.
   */

  TAILQ_INIT (&connections);
  TAILQ_INIT (&connections_passive);

  conns = conf_get_list ("Phase 2", "Connections");
  if (conns)
    {
      for (conn = TAILQ_FIRST (&conns->fields); conn;
	   conn = TAILQ_NEXT (conn, link))
	{
	  if (connection_setup (conn->field))
	    log_print ("connection_init: could not setup \"%s\"", conn->field);

	  /* XXX Break/abort here if connection_setup failed?  */

	  /*
	   * XXX This code (i.e. the attribute lookup) seems like a
	   * likely candidate for factoring out into a function of its
	   * own.
	   */
	  attrs = conf_get_list (conn->field, "Flags");
	  if (attrs)
	    for (attr = TAILQ_FIRST (&attrs->fields); attr;
		 attr = TAILQ_NEXT (attr, link))
	      if (strcasecmp ("active-only", attr->field) == 0)
		break;
	  if (!attrs || (attrs && !attr))
	    if (connection_record_passive (conn->field))
	      log_print ("connection_init: could not record "
			 "connection \"%s\"", conn->field);
	  if (attrs)
	    conf_free_list (attrs);

	}
      conf_free_list (conns);
    }

  conns = conf_get_list ("Phase 2", "Passive-Connections");
  if (conns)
    {
      for (conn = TAILQ_FIRST (&conns->fields); conn;
	   conn = TAILQ_NEXT (conn, link))
	if (connection_record_passive (conn->field))
	  log_print ("connection_init: could not record passive "
		     "connection \"%s\"", conn->field);
      conf_free_list (conns);
    }
}

/* Check the connection in VCONN and schedule another check later.  */
static void
connection_checker (void *vconn)
{
  struct timeval now;
  struct connection *conn = vconn;

  gettimeofday (&now, 0);
  now.tv_sec += conf_get_num ("General", "check-interval", CHECK_INTERVAL);
  conn->ev
    = timer_add_event ("connection_checker", connection_checker, conn, &now);
  if (!conn->ev)
    log_print ("connection_checker: could not add timer event");
  sysdep_connection_check (conn->name);
}

/* Find the connection named NAME.  */
static struct connection *
connection_lookup (char *name)
{
  struct connection *conn;

  for (conn = TAILQ_FIRST (&connections); conn; conn = TAILQ_NEXT (conn, link))
    if (strcasecmp (conn->name, name) == 0)
      return conn;
  return 0;
}

/* Does the connection named NAME exist?  */
int
connection_exist (char *name)
{
  return (connection_lookup (name) != NULL);
}

/* Find the passive connection named NAME.  */
static struct connection_passive *
connection_passive_lookup_by_name (char *name)
{
  struct connection_passive *conn;

  for (conn = TAILQ_FIRST (&connections_passive); conn;
       conn = TAILQ_NEXT (conn, link))
    if (strcasecmp (conn->name, name) == 0)
      return conn;
  return 0;
}

/*
 * IDs of different types cannot be the same.
 * XXX Rename to ipsec_compare_id, and move to ipsec.c ?
 */
int
compare_ids (u_int8_t *id1, u_int8_t *id2, size_t idlen)
{
  int id1_type, id2_type;

  id1_type = GET_ISAKMP_ID_TYPE (id1);
  id2_type = GET_ISAKMP_ID_TYPE (id2);

  return id1_type == id2_type
    ? memcmp (id1 + ISAKMP_ID_DATA_OFF, id2 + ISAKMP_ID_DATA_OFF,
	      idlen - ISAKMP_ID_DATA_OFF) : -1;
}

/* Find the connection named with matching IDs.  */
char *
connection_passive_lookup_by_ids (u_int8_t *id1, u_int8_t *id2)
{
  struct connection_passive *conn;

  for (conn = TAILQ_FIRST (&connections_passive); conn;
       conn = TAILQ_NEXT (conn, link))
    {
      if (conn->remote_id == NULL)
	continue;

      /*
       * If both IDs match what we have saved, return the name.  Don't bother
       * in which order they are.
       */
      if ((compare_ids (id1, conn->local_id, conn->local_sz) == 0
	   && compare_ids (id2, conn->remote_id, conn->remote_sz) == 0)
	  || (compare_ids (id1, conn->remote_id, conn->remote_sz) == 0
	      && compare_ids (id2, conn->local_id, conn->local_sz) == 0))
	{
	  LOG_DBG ((LOG_MISC, 60,
		    "connection_passive_lookup_by_ids: returned \"%s\"",
		    conn->name));
	  return conn->name;
	}
    }

  /* In the road warrior case, we do not know the remote ID. In that
   * case we will just match against the local ID.
   */
  for (conn = TAILQ_FIRST (&connections_passive); conn;
       conn = TAILQ_NEXT (conn, link))
    {
      if (conn->remote_id != NULL)
	continue;

      if (compare_ids (id1, conn->local_id, conn->local_sz) == 0
	  || compare_ids (id2, conn->local_id, conn->local_sz) == 0)
	{
	  LOG_DBG ((LOG_MISC, 60,
		    "connection passive_lookup_by_ids: returned \"%s\""
		    " only matched local id", conn->name));
	  return conn->name;
	}
    }
  LOG_DBG ((LOG_MISC, 60,
	    "connection_passive_lookup_by_ids: no match"));
  return 0;
}

/* Find the connection named with matching group ID.  */
char *
connection_passive_lookup_by_group_id (u_int8_t *id1)
{
  struct connection_passive *conn;
  
  for (conn = TAILQ_FIRST (&connections_passive); conn;
       conn = TAILQ_NEXT (conn, link))
    {
      /*
       * If the group ID matches what we have saved, return the name. 
       */
      if (compare_ids (id1, conn->group_id, conn->group_sz) == 0)
	{
	  LOG_DBG ((LOG_MISC, 60,
		     "connection_passive_lookup_by_group_id: returned \"%s\"",
		     conn->name));
	  return conn->name;
	}
    }
  LOG_DBG ((LOG_MISC, 60,
	     "connection_passive_lookup_by_group_id: no match"));
  return 0;
}

/*
 * Setup NAME to be a connection that should be up "always", i.e. if it dies,
 * for whatever reason, it should be tried to be brought up, over and over
 * again.
 */
int
connection_setup (char *name)
{
  struct connection *conn = 0;
  struct timeval now;

  /* Check for trials to add duplicate connections.  */
  if (connection_lookup (name))
    {
      LOG_DBG ((LOG_MISC, 10, "connection_setup: cannot add \"%s\" twice",
		name));
      return 0;
    }

  conn = calloc (1, sizeof *conn);
  if (!conn)
    {
      log_error ("connection_setup: calloc (1, %d) failed", sizeof *conn);
      goto fail;
    }

  conn->name = strdup (name);
  if (!conn->name)
    {
      log_error ("connection_setup: strdup (\"%s\") failed", name);
      goto fail;
    }

  gettimeofday (&now, 0);
  conn->ev
    = timer_add_event ("connection_checker", connection_checker, conn, &now);
  if (!conn->ev)
    {
      log_print ("connection_setup: could not add timer event");
      goto fail;
    }

  TAILQ_INSERT_TAIL (&connections, conn, link);
  return 0;

 fail:
  if (conn)
    {
      if (conn->name)
	free (conn->name);
      free (conn);
    }
  return -1;
}

int
connection_record_passive_ipsec (char *name, char *local_id, char *remote_id)
{
  struct connection_passive *conn;

  local_id = conf_get_str (name, "Local-ID");
  if (!local_id)
    {
      log_print ("connection_record_passive: "
		 "\"Local-ID\" is missing from section [%s]",
		 name);
      return -1;
    }

  /* If the remote id lookup fails we defer it to later */
  remote_id = conf_get_str (name, "Remote-ID");

  conn = calloc (1, sizeof *conn);
  if (!conn)
    {
      log_error ("connection_record_passive: calloc (1, %d) failed",
		 sizeof *conn);
      return -1;
    }

  conn->name = strdup (name);
  if (!conn->name)
    {
      log_error ("connection_record_passive: strdup (\"%s\") failed", name);
      goto fail;
    }

  /* XXX IPSec DOI-specific.  */
  conn->local_id = ipsec_build_id (local_id, &conn->local_sz);
  if (!conn->local_id)
    goto fail;

  if (remote_id)
    {
      conn->remote_id = ipsec_build_id (remote_id, &conn->remote_sz);
      if (!conn->remote_id)
	goto fail;
    }
  else
    conn->remote_id = NULL;

  TAILQ_INSERT_TAIL (&connections_passive, conn, link);

  LOG_DBG ((LOG_MISC, 60,
	    "connection_record_passive: passive connection \"%s\" "
	    "added", conn->name));
  return 0;

 fail:
  if (conn->local_id)
    free (conn->local_id);
  if (conn->name)
    free (conn->name);
  free (conn);
  return -1;
}

int
connection_record_passive_gdoi (char *name, char *group_id)
{
  struct connection_passive *conn;

  conn = calloc (1, sizeof *conn);
  if (!conn)
    {
      log_error ("connection_record_passive: calloc (1, %d) failed",
		 sizeof *conn);
      return -1;
    }
  
  conn->name = strdup (name);
  if (!conn->name)
    {
      log_error ("connection_record_passive: strdup (\"%s\") failed", name);
      goto fail;
    }

  conn->group_id = group_build_id (group_id, &conn->group_sz);
  if (!conn->group_id)
    goto fail;

  TAILQ_INSERT_TAIL (&connections_passive, conn, link);
  
  LOG_DBG ((LOG_MISC, 60,
	     "connection_record_passive: passive connection \"%s\" "
	     "added", conn->name));
  return 0;

 fail:
  if (conn->group_id)
    free (conn->group_id);
  if (conn->name)
    free (conn->name);
  free (conn);
  return -1;
}

int
connection_record_passive (char *name)
{
  if (connection_passive_lookup_by_name (name))
    {
      LOG_DBG ((LOG_MISC, 10, 
		 "connection_record_passive: cannot add \"%s\" twice",
		 name));
      return 0;
    }

  if (connection_record_passive_gdoi (name, name))
	{
	  log_print ("connection_record_passive: "
	     "\"ID-type\" missing from section [%s]", name);
  	  return -1;
	}

  return 0;
}

/* Remove the connection named NAME.  */
void
connection_teardown (char *name)
{
  struct connection *conn;

  conn = connection_lookup (name);
  if (!conn)
    return;

  TAILQ_REMOVE (&connections, conn, link);
  timer_remove_event (conn->ev);
  free (conn->name);
  free (conn);
}

/* Remove the passive connection named NAME.  */
void
connection_passive_teardown (char *name)
{
  struct connection_passive *conn;

  conn = connection_passive_lookup_by_name (name);
  if (!conn)
    return;

  TAILQ_REMOVE (&connections_passive, conn, link);
  free (conn->name);
  free (conn->local_id);
  free (conn->remote_id);
  free (conn);
}

void
connection_report (void)
{
  struct connection *conn;
  struct timeval now;
#ifdef USE_DEBUG
  struct connection_passive *pconn;
  struct doi *doi = doi_lookup (ISAKMP_DOI_ISAKMP);
#endif

  gettimeofday (&now, 0);
  for (conn = TAILQ_FIRST (&connections); conn; conn = TAILQ_NEXT (conn, link))
    LOG_DBG ((LOG_REPORT, 0,
	      "connection_report: connection %s next check %ld seconds",
	      (conn->name ? conn->name : "<unnamed>"),
	      conn->ev->expiration.tv_sec - now.tv_sec));
#ifdef USE_DEBUG
  for (pconn = TAILQ_FIRST (&connections_passive); pconn;
       pconn = TAILQ_NEXT (pconn, link))
    LOG_DBG ((LOG_REPORT, 0,
	      "connection_report: passive connection %s %s", pconn->name,
	      doi->decode_ids ("local_id: %s, remote_id: %s",
			       pconn->local_id, pconn->local_sz,
			       pconn->remote_id, pconn->remote_sz, 1)));
#endif
}

/* Reinitialize all connections (SIGHUP handling).  */
void
connection_reinit (void)
{
  struct connection *conn, *next;
  struct connection_passive *pconn, *pnext;

  LOG_DBG ((LOG_MISC, 30,
	    "connection_reinit: reinitializing connection list"));

  /* Remove all present connections.  */
  for (conn = TAILQ_FIRST (&connections); conn; conn = next)
    {
      next = TAILQ_NEXT (conn, link);
      connection_teardown (conn->name);
    }

  for (pconn = TAILQ_FIRST (&connections_passive); pconn; pconn = pnext)
    {
      pnext = TAILQ_NEXT (pconn, link);
      connection_passive_teardown (pconn->name);
    }

  /* Setup new connections, as the (new) config directs.  */
  connection_init ();
}
