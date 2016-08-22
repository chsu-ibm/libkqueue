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
#include <assert.h>
#include "../common/queue.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include "sys/event.h"
#include "private.h"

/* close pipefd if exist and reset pipefd to -1 */
static void
reset_pipe(struct filter *filt, int *pipefd)
{
    int read_fd = pipefd[0];
    int write_fd = pipefd[1];
    if (read_fd != -1) {
        filt->knote_map[read_fd] = NULL;
        posix_kqueue_clearfd_read(filt->kf_kqueue, read_fd);
        close(read_fd);
    }
    if (write_fd != -1) close(write_fd);

    pipefd[0] = pipefd[1] = -1;
}

int
posix_evfilt_user_init(struct filter *filt)
{
    filt->knote_map = allocate_knote_map();
    return 0;
}

void
posix_evfilt_user_destroy(struct filter *filt)
{
    deallocate_knote_map(filt->knote_map);
}

int
posix_evfilt_user_copyout(struct kevent *dst,
                          struct knote *src,
                          void *ptr)
{
    struct filter *filt = (struct filter *)ptr;
    memcpy(dst, &src->kev, sizeof(*dst));
    dst->fflags &= ~NOTE_FFCTRLMASK;  // FIXME: Not sure if needed
    dst->fflags &= ~NOTE_TRIGGER;
    if (src->kev.flags & EV_ADD) {
        /* NOTE: True on FreeBSD but not consistent behavior with
                  other filters. */
        dst->flags &= ~EV_ADD;
    }
    if (src->kev.flags & EV_CLEAR) src->kev.fflags &= ~NOTE_TRIGGER;
    if (src->kev.flags & (EV_DISPATCH | EV_CLEAR | EV_ONESHOT)) {
        dbg_puts("read out anonymous pipe");
        char buf[1024];
        if (read(src->kdata.kn_eventfd[0], buf, sizeof(buf)) < 0) {
            /* pipe_read_fd is in blocking mode so there is no EAGAIN */
            /* should we consider interrupt EINTR ? */
            dbg_printf("read(2): %s", strerror(errno));
            return -1;
        }
    }

    return (0);
}

int
posix_evfilt_user_knote_create(struct filter *filt, struct knote *kn)
{
    int *pipefd = &kn->kdata.kn_eventfd[0];

    /* create a pipe and set the write end in non-blocking mode */
    if (pipe(pipefd) == -1) {
        dbg_perror("eventfd");
        return -1;
    }

    if (fcntl(pipefd[1], F_SETFL, O_NONBLOCK) == -1) {
        reset_pipe(filt, pipefd);
        dbg_perror("fcntl(F_SETFL)");
        return -1;
    }

    dbg_printf("pipefd[0] = %d, pipefd[1] = %d",
               pipefd[0], pipefd[1]);
    /* add the read end of pipe to kqueue's waiting fd list */
    posix_kqueue_setfd_read(filt->kf_kqueue, pipefd[0]);
    filt->knote_map[pipefd[0]] = kn;
    return 0;
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
        case NOTE_FFAND: kn->kev.fflags &= fflags; break;
        case NOTE_FFOR: kn->kev.fflags |= fflags; break;
        case NOTE_FFCOPY: kn->kev.fflags = fflags; break;
        case NOTE_FFNOP: /* do nothing */ break;
        default: /* FIXME: should report error? */ break;
    }

    /* if trigger and not disabled */
    if ((!(kn->kev.flags & EV_DISABLE)) && kev->fflags & NOTE_TRIGGER) {
        dbg_printf("trigger user event: knote = 0x%p", kn);
        kn->kev.fflags |= NOTE_TRIGGER;
        if (write(kn->kdata.kn_eventfd[1], ".", 1) == -1 && errno != EAGAIN) {
            /* EAGAIN is not considered as error */
            dbg_printf("write(2) on fd %d: %s", kn->kdata.kn_eventfd[1],
                       strerror(errno));
            return -1;
        }
    }
    return 0;
}

int
posix_evfilt_user_knote_delete(struct filter *filt, struct knote *kn)
{
    dbg_printf("filt %p kn %p", filt, kn);
    assert(filt->knote_map[kn->kdata.kn_eventfd[0]] == kn);

    reset_pipe(filt, &kn->kdata.kn_eventfd[0]);
    return (0);
}

int
posix_evfilt_user_knote_enable(struct filter *filt, struct knote *kn)
{
    dbg_printf("filt %p kn %p", filt, kn);
    assert(filt->knote_map[kn->kdata.kn_eventfd[0]] == kn);

    kn->kev.fflags &= ~NOTE_TRIGGER;
    reset_pipe(filt, &kn->kdata.kn_eventfd[0]);
    return posix_evfilt_user_knote_create(filt, kn);
}

int
posix_evfilt_user_knote_disable(struct filter *filt, struct knote *kn)
{
    dbg_printf("filt %p kn %p", filt, kn);
    return posix_evfilt_user_knote_delete(filt, kn);
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
