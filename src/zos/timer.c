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

#include <pthread.h>
#include "../common/queue.h"
#include "private.h"

struct sleeper_info {
    /* @status: 0: not done; 1: good; -1: bad */
    int status;
    struct knote *orig_knote;
    uintptr_t interval;
};

static void *
sleeper_thread(void *opaque)
{
    /* main thread is responsible for managing opaque memory */
    struct sleeper_info *orig_info = (struct sleeper_info *)opaque;
    struct sleeper_info info = *orig_info;
    struct timespec ts;
    struct timeval now;
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

    atomic_inc(&orig_info->status);

    gettimeofday(&now, NULL);
    ts.tv_sec = now.tv_sec + info.interval / 1000;
    ts.tv_nsec = now.tv_usec * 1000 + (info.interval % 1000) * 1000000;
    /* make sure tv_nsec is no larger than 999999999 */
    ts.tv_sec += ts.tv_nsec / 1000000000;
    ts.tv_nsec = ts.tv_nsec % 1000000000;

    dbg_printf("about to goto sleep, will wakeup %ld.%lu later",
               info.interval / 1000, info.interval % 1000);

    /* go sleep */
    pthread_mutex_lock(&mtx);
    /* FIXME: handle error */
    pthread_cond_timedwait(&cond, &mtx, &ts);
    pthread_mutex_unlock(&mtx);

    info.orig_knote->kdata.expired = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    dbg_printf("wake up from sleep");

    /* wake up */
    int cnt;
    int write_fd = info.orig_knote->kdata.kn_eventfd[1];

write_again:
    cnt = write(write_fd, &info.orig_knote, sizeof(info.orig_knote));
    if (cnt < sizeof(info.orig_knote)) {
        if (errno == EAGAIN) {
            goto write_again;
        } else {
            dbg_printf("write fail: %s", strerror(errno));
        }
    }
    return NULL;
}

/* close pipefd if exist and reset pipefd to -1 */
static void
reset_pipe(struct filter *filt, int *pipefd)
{
    int read_fd = pipefd[0];
    int write_fd = pipefd[1];
    if (read_fd != -1) {
        posix_kqueue_clearfd_read(filt->kf_kqueue, read_fd);
        filt->fd_map[read_fd] = INVALID_IDENT;
        close(read_fd);
    }
    if (write_fd != -1) close(write_fd);

    pipefd[0] = pipefd[1] = -1;
}

static void
setfd(struct filter *filt, int fd, int ident)
{
    posix_kqueue_setfd_read(filt->kf_kqueue, fd);
    dbg_printf("filt->fd_map[%d] = %lu", fd, filt->fd_map[fd]);
    assert(filt->fd_map[fd] == INVALID_IDENT);
    filt->fd_map[fd] = ident;
}

static uintptr_t
user_fd_to_ident(struct filter *filt, int fd)
{
    uintptr_t ident = filt->fd_map[fd];
    dbg_printf("fd -> ident: %d -> 0x%lX", fd, ident);
    return ident;
}

int
evfilt_timer_init(struct filter *filt)
{
    filt->fd_to_ident = user_fd_to_ident;

    /* create fd_map and initialize it to 0xff */
    size_t size = MAX_FILE_DESCRIPTORS * sizeof(uintptr_t);
    filt->fd_map = malloc(size);
    memset(filt->fd_map, INVALID_IDENT, size);
    return 0;
}

void
evfilt_timer_destroy(struct filter *filt)
{
    if (filt->fd_map) {
        free(filt->fd_map);
        filt->fd_map = NULL;
    }
}

int
evfilt_timer_copyout(struct kevent *dst, struct knote *src, void *ptr)
{
    uintptr_t orig_knote_ptr;
    ssize_t n;
    struct filter *filt = (struct filter *)ptr;

    memcpy(dst, &src->kev, sizeof(*dst));

    n = read(src->kdata.kn_eventfd[0], &orig_knote_ptr, sizeof(orig_knote_ptr));
    if (n != sizeof(orig_knote_ptr)) {
        dbg_puts("invalid read from timerfd");
        orig_knote_ptr = 1; /* Fail gracefully */
    }

    dst->data = src->kdata.expired;

    if (src->kev.flags & EV_ONESHOT) {
        evfilt_timer_knote_delete(filt, src);
        knote_delete(filt, src);
    }

    return 0;
}

int
evfilt_timer_knote_create(struct filter *filt, struct knote *kn)
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

    dbg_printf("pipefd[0] = %d, pipefd[1] = %d", pipefd[0], pipefd[1]);
    /* add the read end of pipe to kqueue's waiting fd list */
    setfd(filt, pipefd[0], kn->kev.ident);

    /* create sleeper thread, set the interval to 0 for one shot timer */
    uintptr_t interval = (kn->kev.flags & EV_ONESHOT) ? 0 : kn->kev.data;
    volatile struct sleeper_info info = {
        .status = 0,
        .orig_knote = kn,
        .interval = interval
    };

    int rv = 0;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&tid, &attr, sleeper_thread, (void*)&info) != 0) {
        dbg_printf("pthread_create fail: %s", strerror(errno));
        rv = -1;
    } else {
        /* spin wait for sleeper_thread get ready */
        do {} while (info.status == 0);
        if (info.status == -1) {
            rv = -1;
        }
    }
    if (rv == -1) {
        reset_pipe(filt, pipefd);
    }
    pthread_attr_destroy(&attr);
    return rv;
}

int
evfilt_timer_knote_modify(struct filter *filt,
                          struct knote *kn,
                          const struct kevent *kev)
{
    (void)filt;
    (void)kn;
    (void)kev;
    return (-1); /* STUB */
}

int
evfilt_timer_knote_delete(struct filter *filt, struct knote *kn)
{
    reset_pipe(filt, &kn->kdata.kn_eventfd[0]);
    return 0;
}

int
evfilt_timer_knote_enable(struct filter *filt, struct knote *kn)
{
    reset_pipe(filt, &kn->kdata.kn_eventfd[0]);
    return evfilt_timer_knote_create(filt, kn);
}

int
evfilt_timer_knote_disable(struct filter *filt, struct knote *kn)
{
    return evfilt_timer_knote_delete(filt, kn);
}

const struct filter evfilt_timer = {
    EVFILT_TIMER,
    evfilt_timer_init,
    evfilt_timer_destroy,
    evfilt_timer_copyout,
    evfilt_timer_knote_create,
    evfilt_timer_knote_modify,
    evfilt_timer_knote_delete,
    evfilt_timer_knote_enable,
    evfilt_timer_knote_disable,
};
