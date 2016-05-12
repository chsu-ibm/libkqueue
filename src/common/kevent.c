/*
 * Copyright (c) 2009 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma csect (CODE,   "common#kevent#C")
#pragma csect (STATIC, "common#kevent#S")
#pragma csect (TEST,   "common#kevent#T")


/* To get asprintf(3) */
#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>

#include "private.h"
#ifdef __MVS__
#include "../zos/platform.h"
#endif 

static const char *
kevent_filter_dump(const struct kevent *kev)
{
#ifndef __MVS__
    static __thread char tls_buf[64];
    char * buf = tls_buf;
    int size = sizeof(tls_buf);
#else
    char * buf = get_tls()->buf1;
    int size = sizeof(((struct tlsflat*)0)->buf1);
#endif

    snprintf(buf, size, "%d (%s)", 
            kev->filter, filter_name(kev->filter));
    return ((const char *) buf);
}

static const char *
kevent_fflags_dump(const struct kevent *kev)
{
#ifndef __MVS__
    static __thread char tls_buf[1024];
    char * buf = tls_buf;
    int size = sizeof(tls_buf);
#else
    char * buf = get_tls()->buf2;
    int size = sizeof(((struct tlsflat*)0)->buf2);
#endif

#define KEVFFL_DUMP(attrib) \
    if (kev->fflags & attrib) \
    strncat(buf, #attrib" ", 64);

    snprintf(buf, size, "fflags=0x%04x (", kev->fflags);
    if (kev->filter == EVFILT_VNODE) {
        KEVFFL_DUMP(NOTE_DELETE);
        KEVFFL_DUMP(NOTE_WRITE);
        KEVFFL_DUMP(NOTE_EXTEND);
        KEVFFL_DUMP(NOTE_ATTRIB);
        KEVFFL_DUMP(NOTE_LINK);
        KEVFFL_DUMP(NOTE_RENAME);
    } else if (kev->filter == EVFILT_USER) {
        KEVFFL_DUMP(NOTE_FFNOP);
        KEVFFL_DUMP(NOTE_FFAND);
        KEVFFL_DUMP(NOTE_FFOR);
        KEVFFL_DUMP(NOTE_FFCOPY);
        KEVFFL_DUMP(NOTE_TRIGGER);
    }  else {
        strncat(buf, " ", 1);
    }
    buf[strlen(buf) - 1] = ')';

#undef KEVFFL_DUMP

    return ((const char *) buf);
}

static const char *
kevent_flags_dump(const struct kevent *kev)
{
#ifndef __MVS__
    static __thread char tls_buf[1024];
    char * buf = tls_buf;
    int size = sizeof(tls_buf);
#else
    char * buf = get_tls()->buf3;
    int size = sizeof(((struct tlsflat*)0)->buf3);
#endif

#define KEVFL_DUMP(attrib) \
    if (kev->flags & attrib) \
	strncat(buf, #attrib" ", 64);

    snprintf(buf, size, "flags=0x%04x (", kev->flags);
    KEVFL_DUMP(EV_ADD);
    KEVFL_DUMP(EV_ENABLE);
    KEVFL_DUMP(EV_DISABLE);
    KEVFL_DUMP(EV_DELETE);
    KEVFL_DUMP(EV_ONESHOT);
    KEVFL_DUMP(EV_CLEAR);
    KEVFL_DUMP(EV_EOF);
    KEVFL_DUMP(EV_ERROR);
    KEVFL_DUMP(EV_DISPATCH);
    KEVFL_DUMP(EV_RECEIPT);
    buf[strlen(buf) - 1] = ')';

#undef KEVFL_DUMP

    return ((const char *) buf);
}

typedef unsigned int u_int;

const char *
kevent_dump(const struct kevent *kev)
{
#ifndef __MVS__
    static __thread char tls_buf[1024];
    char * buf = tls_buf;
    int size = sizeof(tls_buf);
#else
    char * buf = get_tls()->buf4;
    int size = sizeof(((struct tlsflat*)0)->buf4);
#endif

    snprintf(buf, size, 
            "{ ident=%d, filter=%s, %s, %s, data=%d, udata=%p }",
            (u_int) kev->ident,
            kevent_filter_dump(kev),
            kevent_flags_dump(kev),
            kevent_fflags_dump(kev),
            (int) kev->data,
            kev->udata);

    return ((const char *) buf);
}

static int
kevent_copyin_one(struct kqueue *kq, const struct kevent *src)
{
    struct knote  *kn = NULL;
    struct filter *filt;
    int rv = 0;

    if (src->flags & EV_DISPATCH && src->flags & EV_ONESHOT) {
        dbg_puts("Error: EV_DISPATCH and EV_ONESHOT are mutually exclusive");
        errno = EINVAL;
        return (-1);
    }

    if (filter_lookup(&filt, kq, src->filter) < 0) 
        return (-1);

    dbg_printf("src=%s", kevent_dump(src));

    kn = knote_lookup(filt, src->ident);
    dbg_printf("knote_lookup: ident %d == %p", (int)src->ident, kn);

    if (kn != NULL && src->flags & EV_ADD) {
        knote_delete(filt, kn);
        kn = NULL;
    }

    if (kn == NULL) {
        if (src->flags & EV_ADD) {
            if ((kn = knote_new()) == NULL) {
                errno = ENOENT;
                return (-1);
            }
            memcpy(&kn->kev, src, sizeof(kn->kev));
            kn->kev.flags &= ~EV_ENABLE;
            kn->kev.flags |= EV_ADD;
			kn->kn_kq = kq;
            assert(filt->kn_create);
            if (filt->kn_create(filt, kn) < 0) {
                knote_release(kn);
                errno = EFAULT;
                return (-1);
            } 
            knote_insert(filt, kn);
            dbg_printf("created kevent %s", kevent_dump(src));
            dbg_printf("filt=0x%x, kn=0x%x", filt, kn);

/* XXX- FIXME Needs to be handled in kn_create() to prevent races */
            if (src->flags & EV_DISABLE) {
                kn->kev.flags |= EV_DISABLE;
                return (filt->kn_disable(filt, kn));
            }

            return (0);
        } else {
            dbg_printf("no entry found for ident=%u", (unsigned int)src->ident); 
            errno = ENOENT;
            return (-1);
        }
    }

    if (src->flags & EV_DELETE) {
        rv = knote_delete(filt, kn);
        dbg_printf("knote_delete returned %d", rv);
    } else if (src->flags & EV_DISABLE) {
        kn->kev.flags |= EV_DISABLE;
        rv = filt->kn_disable(filt, kn);
        dbg_printf("kn_disable returned %d", rv);
    } else if (src->flags & EV_ENABLE) {
        kn->kev.flags &= ~EV_DISABLE;
        rv = filt->kn_enable(filt, kn);
        dbg_printf("kn_enable returned %d", rv);
    } else if (src->flags & EV_ADD || src->flags == 0 || src->flags & EV_RECEIPT) {
        kn->kev.udata = src->udata;
        rv = filt->kn_modify(filt, kn, src);
        dbg_printf("kn_modify returned %d", rv);
    }

    return (rv);
}

/** @return number of events added to the eventlist */
static int
kevent_copyin(struct kqueue *kq, const struct kevent *src, int nchanges,
        struct kevent *eventlist, int nevents)
{
    int status, nret;

    dbg_printf("nchanges=%d nevents=%d", nchanges, nevents);

    /* TODO: refactor, this has become convoluted to support EV_RECEIPT */
    for (nret = 0; nchanges > 0; src++, nchanges--) {

        if (kevent_copyin_one(kq, src) < 0) {
            dbg_printf("errno=%s",strerror(errno));
            status = errno;
            goto err_path;
        } else {
            if (src->flags & EV_RECEIPT) {
                status = 0;
                goto err_path;
            }
        }

        continue;

err_path:
        if (nevents > 0) {
            memcpy(eventlist, src, sizeof(*src));
            eventlist->data = status;
            nevents--;
            eventlist++;
            nret++;
        } else {
            return (-1);
        }
    }

    return (nret);
}

int VISIBLE
kevent(int kqfd, const struct kevent *changelist, int nchanges,
        struct kevent *eventlist, int nevents,
        const struct timespec *timeout)
{
    struct kqueue *kq;
    int rv = 0;
#ifndef NDEBUG
    static unsigned int _kevent_counter = 0;
    unsigned int myid = 0;

    (void) myid;
#endif

    /* Convert the descriptor into an object pointer */
    kq = kqueue_lookup(kqfd);
    if (kq == NULL) {
        errno = ENOENT;
        return (-1);
    }

#ifndef NDEBUG
    if (DEBUG_KQUEUE) {
        myid = atomic_inc(&_kevent_counter);
        dbg_printf("--- kevent %u --- (nchanges = %d, nevents = %d)", myid, nchanges, nevents);
    }
#endif

    /*
     * Process each kevent on the changelist.
     */
    if (nchanges > 0) {
        kqueue_lock(kq);
        rv = kevent_copyin(kq, changelist, nchanges, eventlist, nevents);
        kqueue_unlock(kq);
        dbg_printf("(%u) changelist: rv=%d", myid, rv);
        if (rv < 0)
            goto out;
        if (rv > 0) {
            eventlist += rv;
            nevents -= rv;
        }
    }

    rv = 0;
    
    /*
     * Wait for events and copy them to the eventlist
     */
    if (nevents > MAX_KEVENT)
        nevents = MAX_KEVENT;
    if (nevents > 0) {
        rv = kqops.kevent_wait(kq, nevents, timeout);
        dbg_printf("kqops.kevent_wait returned %d", rv);
        if (fastpath(rv > 0)) {
            kqueue_lock(kq);
            rv = kqops.kevent_copyout(kq, rv, eventlist, nevents);
            kqueue_unlock(kq);
        } else if (rv == 0) {
            /* Timeout reached */
        } else {
            dbg_printf("(%u) kevent_wait failed", myid);
            goto out;
        }
    }

#ifndef NDEBUG
    if (DEBUG_KQUEUE) {
        int n;

        dbg_printf("(%u) returning %d events", myid, rv);
        for (n = 0; n < rv; n++) {
	    dbg_printf("(%u) eventlist[%d] = %s", myid, n, kevent_dump(&eventlist[n]));
        }
    }
#endif

out:
    dbg_printf("--- END kevent %u ret %d ---", myid, rv);
    return (rv);
}
