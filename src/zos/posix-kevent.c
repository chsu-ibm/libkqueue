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

#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <sys/select.h>

#include "sys/event.h"
#include "private.h"

static int process(struct kevent *e, struct filter *f, fd_set *g, int n, int r);

void
posix_kqueue_setfd(struct kqueue *kq, int fd)
{
    dbg_printf("setting fd %d", fd);

    FD_SET(fd, &kq->kq_fds);
    if (kq->kq_nfds <= fd)
        kq->kq_nfds = fd + 1;
}

int
posix_kevent_wait(struct kqueue *kq,
                  int nevents,
                  const struct timespec *timeout)
{
    tlsflat_t *tls;
    int n, nfds;
    fd_set rfds, wfds;
    struct timespec ts;
    int i ;

    dbg_puts("waiting for events");

    nfds = kq->kq_nfds;
    rfds = kq->kq_fds;
    wfds = kq->kq_wfds;

    n = pselect(nfds, &rfds, &wfds , NULL, timeout, NULL);
    if (n < 0) {
        if (errno == EINTR) {
            dbg_puts("signal caught");
            return (-1);
        }
        dbg_perror("pselect(2)");
        return (-1);
    }

    tls = get_tls();
    tls->rfds = rfds;
    tls->wfds = wfds;
#ifndef NDEBUG
    dbg_printf2("pselect returns %d events added", n);
    for (i = 0; i < nfds; i++) {
        if (FD_ISSET(i, &rfds))
            dbg_printf2("read file descriptor = %d is added", i);
        if (FD_ISSET(i, &wfds))
            dbg_printf2("write file descriptor = %d is added", i);
    }
#endif
    return (n);
}

int
posix_kevent_copyout(struct kqueue *kq,
                     int nready,
                     struct kevent *eventlist,
                     int nevents)
{
    int i, n;

    int remain = nevents;
    int nfds = kq->kq_nfds;
    tlsflat_t *tls = get_tls();
    fd_set rfds = tls->rfds;
    fd_set wfds = tls->wfds;
    int nret = 0;
    for (i = 0; i < EVFILT_SYSCOUNT && remain; ++i) {
        fd_set *fds = (~i == (EVFILT_WRITE)) ? &wfds : &rfds;
        struct filter *filt = &kq->kq_filt[i];
        if (!filt || filt->kf_id == 0) {
            dbg_printf2("filt %d is not implemented", i);
            continue;
        }
        assert(filt && "filter is not implemented");

        n = process(eventlist, filt, fds, nfds, remain);

        nret += n;
        remain -= n;
        eventlist += n;
        assert(remain >= 0);
    }

    return nret;
}

static int
process(struct kevent *eventlist,
        struct filter *filt,
        fd_set *fds,
        const int nfds,
        const int remain)
{
    int nret;
    int fd;

    nret = 0;
    dbg_printf("filt = %d", filt->kf_id);
    for (fd = 0; fd < nfds && nret < remain; ++fd) {
        if (!FD_ISSET(fd, fds)) continue;

        uintptr_t ident = filt->fd_to_ident(filt, fd);
        if (ident == INVALID_IDENT) continue;

        dbg_printf("filt = %d, fd -> ident = %d -> %lu", filt->kf_id, fd,
                   ident);
        /* FIXME: this operation is very expensive, try to avoid it */
        struct knote *kn = knote_lookup(filt, ident);
        dbg_printf("knote_lookup(0x%p, %lu) = 0x%p", filt, ident, kn);
        if (kn == NULL) continue;

        dbg_printf("before kf_copyout");
        int rv = filt->kf_copyout(eventlist, kn, filt);
        if (slowpath(rv < 0)) {
            dbg_puts("knote_copyout failed");
            /* XXX-FIXME: hard to handle this without losing events */
            abort();
        }

        /* Delete/disable knote according to flags */
        if (eventlist->flags & EV_DISPATCH) knote_disable(filt, kn);
        if (eventlist->flags & EV_ONESHOT) knote_delete(filt, kn);

        /* If an empty kevent structure is returned, the event is discarded.
         */
        if (fastpath(eventlist->filter != 0)) {
            eventlist++;
            nret++;
        } else {
            dbg_puts("spurious wakeup, discarding event");
        }
    }
    dbg_printf("filt = %d", filt->kf_id);
    return nret;
}
