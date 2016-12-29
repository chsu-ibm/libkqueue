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
    struct knote *kn;
};

struct mutex_t {
    pthread_mutex_t mtx;
    pthread_cond_t cond;
};

#define NOTE_TIMER_MASK (NOTE_ABSOLUTE-1)

static void *
sleeper_thread(void *opaque)
{
    /* main thread is responsible for managing opaque memory */
    struct sleeper_info *orig_info = (struct sleeper_info *)opaque;
    struct sleeper_info info = *orig_info;
    struct knote *kn = info.kn;
    struct timespec ts;
    time_t sec;
    time_t nsec;
    long delay;
    unsigned long long now_value;
    struct mutex_t mutex;

    pthread_mutex_init(&mutex.mtx, NULL);
    pthread_cond_init(&mutex.cond, NULL);

    kn->kdata.opaque = &mutex;
    atomic_inc(&orig_info->status);

    delay = kn->kev.data;
    switch (kn->kev.fflags & NOTE_TIMER_MASK) {
        case NOTE_USECONDS:
            sec = delay / 1000000;
            nsec = (delay % 1000000);
            break;
        case NOTE_NSECONDS:
            sec = delay / 1000000000;
            nsec = (delay % 1000000000);
            break;
        case NOTE_SECONDS:
            sec = delay;
            nsec = 0;
            break;
        default: /* milliseconds */
            sec = delay / 1000;
            nsec = (delay % 1000) * 1000000;
    }

    __stckf(&now_value);
    ts.tv_sec = sec + (now_value / 4096000000UL) - 2208988800UL;
    ts.tv_nsec = nsec + (now_value % 4096000000UL) * 1000 / 4096;
    /* make sure tv_nsec is no larger than 999999999 */
    ts.tv_sec += ts.tv_nsec / 1000000000;
    ts.tv_nsec = ts.tv_nsec % 1000000000;

    dbg_printf("about to goto sleep, will wakeup %ld.%lu later", sec, nsec);

    /* go sleep */
    pthread_mutex_lock(&mutex.mtx);
    /* FIXME: handle error */
    pthread_cond_timedwait(&mutex.cond, &mutex.mtx, &ts);
    pthread_mutex_unlock(&mutex.mtx);

    if (kn->kdata.opaque) {
        const int cnt = 1;
        pthread_mutex_lock(&mutex.mtx);
        while (write(kn->kdata.kn_eventfd[1], &cnt, 1) == -1) {
            int err = errno;
            KQ_ABORT_IF(!(err == EAGAIN || err == EWOULDBLOCK || err == EINTR));
        }
        atomic_store_ptr(&kn->kdata.opaque, NULL);
        pthread_mutex_unlock(&mutex.mtx);
    }
    pthread_exit(NULL);
}

/* close pipefd if exist and reset pipefd to -1 */
static void
reset_pipe(struct filter *filt, struct knote *kn)
{
    int *pipefd = kn->kdata.kn_eventfd;
    int read_fd = pipefd[0];
    int write_fd = pipefd[1];

    if (kn->kdata.opaque) {
        struct mutex_t *mutex = (struct mutex_t *)kn->kdata.opaque;
        pthread_mutex_lock(&mutex->mtx);
        atomic_store32(&pipefd[0], -1);
        atomic_store32(&pipefd[1], -1);
        atomic_store_ptr(&kn->kdata.opaque, NULL);
        pthread_cond_signal(&mutex->cond);
        pthread_mutex_unlock(&mutex->mtx);
    }

    if (read_fd != -1) {
        knote_map_remove(filt->knote_map, read_fd);
        posix_kqueue_clearfd_read(filt->kf_kqueue, read_fd);
        close(read_fd);
    }
    if (write_fd != -1) close(write_fd);
}

int
evfilt_timer_init(struct filter *filt)
{
    filt->knote_map = knote_map_init();
    return 0;
}

void
evfilt_timer_destroy(struct filter *filt)
{
    knote_map_destroy(filt->knote_map);
}

int
evfilt_timer_copyout(struct kevent *dst, struct knote *src, void *ptr)
{
    uintptr_t dummy;
    ssize_t n;
    struct filter *filt = (struct filter *)ptr;

    memcpy(dst, &src->kev, sizeof(*dst));

    n = read(src->kdata.kn_eventfd[0], &dummy, sizeof(dummy));
    if (!n) {
        dbg_puts("invalid read from timerfd");
    }

    /* On return, data contains the number of times the
       timer has been trigered.
     */
    /* FIXME: currently timer only support oneshot type. */
    dst->data = 1;

    return 0;
}

int
evfilt_timer_knote_create(struct filter *filt, struct knote *kn)
{
    KQ_ABORT_IF(kn->kdata.opaque != NULL, "kn->kdata.opaque should be NULL.");
    int *pipefd = &kn->kdata.kn_eventfd[0];

    /* create a pipe and set the write end in non-blocking mode */
    if (pipe(pipefd) == -1) {
        dbg_perror("eventfd");
        return -1;
    }

    if (fcntl(pipefd[1], F_SETFL, O_NONBLOCK) == -1) {
        reset_pipe(filt, kn);
        dbg_perror("fcntl(F_SETFL)");
        return -1;
    }

    dbg_printf("pipefd[0] = %d, pipefd[1] = %d", pipefd[0], pipefd[1]);
    /* add the read end of pipe to kqueue's waiting fd list */
    posix_kqueue_setfd_read(filt->kf_kqueue, pipefd[0]);
    knote_map_insert(filt->knote_map, pipefd[0], kn);

    /* create sleeper thread, set the interval to 0 for one shot timer */
    if (!(kn->kev.flags & EV_ONESHOT)) {
        KQ_ABORT("FIXME: current timer only supports EV_ONESHOT type.");
    }

    volatile struct sleeper_info info = {
        .status = 0,
        .kn = kn,
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
        while (info.status == 0) /* wait info.status becomes !0 */;
        if (info.status == -1) {
            rv = -1;
        }
    }
    if (rv == -1) {
        reset_pipe(filt, kn);
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
    reset_pipe(filt, kn);
    return 0;
}

int
evfilt_timer_knote_enable(struct filter *filt, struct knote *kn)
{
    reset_pipe(filt, kn);
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
