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

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include "../common/queue.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include "sys/event.h"
#include "private.h"

int
posix_evfilt_user_init(struct filter *filt)
{
    if (kqops.eventfd_init(&filt->kf_efd) < 0)
        return (-1);

    filt->kf_pfd = kqops.eventfd_descriptor(&filt->kf_efd);

    posix_kqueue_setfd(filt->kf_kqueue, filt->kf_pfd);

    return (0);
}

void
posix_evfilt_user_destroy(struct filter *filt)
{
    kqops.eventfd_close(&filt->kf_efd);
    return;
}

int
posix_evfilt_user_copyout(struct kevent *dst, struct knote *src, void *ptr UNUSED)
{
    struct filter *filt;
    uintptr_t ident;
    char buf[1024];

    filt = (struct filter *)ptr;

    /* Reset the counter */
    dbg_puts("lowering event level");
    if (read(filt->kf_efd.ef_id, buf, sizeof(buf)) < 0) {
        /* FIXME: handle EAGAIN and EINTR */
        /* FIXME: loop so as to consume all data.. may need mutex */
        dbg_printf("read(2): %s", strerror(errno));
        return (-1);
    }

    ident = *((uintptr_t *)buf);

    src = knote_lookup(filt, ident);
    if (src == NULL) {
        dbg_puts("knote_lookup failed");
        return (-1);
    }

    //kqops.eventfd_lower(&filt->kf_efd);

    memcpy(dst, &src->kev, sizeof(*dst));
  
    dst->fflags &= ~NOTE_FFCTRLMASK;     //FIXME: Not sure if needed
    dst->fflags &= ~NOTE_TRIGGER;
    if (src->kev.flags & EV_ADD) {
        /* NOTE: True on FreeBSD but not consistent behavior with
                  other filters. */
        dst->flags &= ~EV_ADD;
    }
    if (src->kev.flags & EV_CLEAR)
        src->kev.fflags &= ~NOTE_TRIGGER;
    if (src->kev.flags & (EV_DISPATCH | EV_CLEAR | EV_ONESHOT)) {
#if 0
        kqops.eventfd_raise(&src->kdata.kn_eventfd);
#endif
    }

    if (src->kev.flags & EV_DISPATCH) {
        dst->flags &=  ~EV_DISPATCH;
        src->kev.fflags &= ~NOTE_TRIGGER;
    }


    return (1);
}

int
posix_evfilt_user_knote_create(struct filter *filt, struct knote *kn)
{
#if TODO
    unsigned int ffctrl;

    //determine if EV_ADD + NOTE_TRIGGER in the same kevent will cause a trigger */
    if ((!(dst->kev.flags & EV_DISABLE)) && src->fflags & NOTE_TRIGGER) {
        dst->kev.fflags |= NOTE_TRIGGER;
        eventfd_raise(filt->kf_pfd);
    }

#endif

    return (0);
}

int
posix_evfilt_user_knote_modify(struct filter *filt, struct knote *kn, 
        const struct kevent *kev)
{
    unsigned int ffctrl;
    unsigned int fflags;

    /* Excerpted from sys/kern/kern_event.c in FreeBSD HEAD */
    ffctrl = kev->fflags & NOTE_FFCTRLMASK;
    fflags = kev->fflags & NOTE_FFLAGSMASK;
    switch (ffctrl) {
        case NOTE_FFNOP:
            break;

        case NOTE_FFAND:
            kn->kev.fflags &= fflags;
            break;

        case NOTE_FFOR:
            kn->kev.fflags |= fflags;
            break;

        case NOTE_FFCOPY:
            kn->kev.fflags = fflags;
            break;

        default:
            /* XXX Return error? */
            break;
    }

    if ((!(kn->kev.flags & EV_DISABLE)) && kev->fflags & NOTE_TRIGGER) {
        kn->kev.fflags |= NOTE_TRIGGER;
#if 0
        knote_enqueue(filt, kn);
#endif
        //kqops.eventfd_raise(&filt->kf_efd);

        dbg_puts("raising event level");
        if (write(filt->kf_efd.ef_wfd, &kn->kev.ident, sizeof(kn->kev.ident)) < 0) {
            /* FIXME: handle EAGAIN and EINTR */
            dbg_printf("write(2) on fd %d: %s", filt->kf_efd.ef_wfd, strerror(errno));
            return (-1);
        }
    }

    return (0);
}

int
posix_evfilt_user_knote_delete(struct filter *filt, struct knote *kn)
{
    return (0);
}

int
posix_evfilt_user_knote_enable(struct filter *filt, struct knote *kn)
{
    /* FIXME: what happens if NOTE_TRIGGER is in fflags?
       should the event fire? */
    return (0);
}

int
posix_evfilt_user_knote_disable(struct filter *filt, struct knote *kn)
{
    

    return (0);
}

const struct filter evfilt_user = {
    EVFILT_USER,
    posix_evfilt_user_init,
    posix_evfilt_user_destroy,
    posix_evfilt_user_copyout,
    posix_evfilt_user_knote_create,
    posix_evfilt_user_knote_modify,
    posix_evfilt_user_knote_delete,
    posix_evfilt_user_knote_enable,
    posix_evfilt_user_knote_disable,   
};
