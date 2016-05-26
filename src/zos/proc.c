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
//#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include "../common/queue.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>

#include <limits.h>

#include "sys/event.h"
#include "private.h"

static pthread_cond_t   wait_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t  wait_mtx = PTHREAD_MUTEX_INITIALIZER;

static int wait_thread_running_flag = 0;
static int wait_thread_dead_flag = 0;
static struct filter *wait_thread_filt = NULL;

struct evfilt_data {
    pthread_t       wthr_id;
};

static void *
wait_thread(void *arg)
{
    struct filter *filt = (struct filter *) arg;
    struct knote *kn;
    int status, result;
    pid_t pid;
    sigset_t sigmask;

    /* Block all signals */
    sigfillset (&sigmask);
    sigdelset(&sigmask, SIGCHLD);
    pthread_sigmask(SIG_BLOCK, &sigmask, NULL);

    for (;;) {

        /* Wait for a child process to exit(2) */
        if ((pid = waitpid(-1, &status, 0)) < 0) {
            if (errno == ECHILD) {
                struct timeval tv;
                struct timespec timeout;

                dbg_puts("got ECHILD, waiting for wakeup condition");
                dbg_printf("filt=0x%x", filt);

                pthread_mutex_lock(&wait_mtx);
                if (wait_thread_dead_flag) {
                    wait_thread_running_flag = 0;
                    pthread_mutex_unlock(&wait_mtx);
                    dbg_puts("wait_thread exiting...");
                    break;
                }
                
                gettimeofday(&tv, NULL);
                timeout.tv_sec = tv.tv_sec + 1;
                timeout.tv_nsec = 0;
#if 0
                pthread_cond_timedwait(&wait_cond, &wait_mtx, &timeout); //FIXME
#else
                pthread_cond_wait(&wait_cond, &wait_mtx);
#endif                
                pthread_mutex_unlock(&wait_mtx);

                dbg_puts("awoken from ECHILD-induced sleep");
                continue;
            }
            if (errno == EINTR)
                continue;
            dbg_printf("wait(2): %s", strerror(errno));
            break;
        } 

        dbg_printf("waitpid(2) returns %d", pid);

        pthread_mutex_lock(&wait_mtx);
        if (wait_thread_dead_flag) {
            wait_thread_running_flag = 0;
            pthread_mutex_unlock(&wait_mtx);
            dbg_puts("wait_thread exiting...");
            break;
        }
        filt = wait_thread_filt;
        pthread_mutex_unlock(&wait_mtx);


        /* Create a proc_event */
        if (WIFEXITED(status)) {
            result = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            /* FIXME: probably not true on BSD */
            result = WTERMSIG(status);
        } else {
            dbg_puts("unexpected code path");
            result = 234;           /* arbitrary error value */
        }

        dbg_printf("result = %d", result);

        /* Scan the wait queue to see if anyone is interested */
        pthread_rwlock_wrlock(&filt->kf_knote_mtx);
        kn = knote_lookup(filt, pid);
        dbg_printf("filt=0x%x, kn=0x%x", filt, kn);

        if (kn != NULL) {
            //kn->kev.data = result;
            //kn->kev.fflags = NOTE_EXIT; 
            //knote_delete(filt, kn);

            /* Indicate read(2) readiness */
            /* TODO: error handling */

            //kqops.eventfd_raise(&filt->kf_efd);
            
            dbg_puts("raising event level");
            if (write(filt->kf_efd.ef_wfd, &kn->kev.ident, sizeof(kn->kev.ident)) < 0) {
                /* FIXME: handle EAGAIN and EINTR */
                dbg_printf("write(2) on fd %d: %s", filt->kf_efd.ef_wfd, strerror(errno));
                return (NULL);
            }
        } else {
            dbg_puts("knote not found");
        }
        
        pthread_rwlock_unlock(&filt->kf_knote_mtx);
    }

    /* TODO: error handling */

    return (NULL);
}

int
posix_evfilt_proc_init(struct filter *filt)
{
    struct evfilt_data *ed;

    if (kqops.eventfd_init(&filt->kf_efd) < 0)
        return (-1);

    filt->kf_pfd = kqops.eventfd_descriptor(&filt->kf_efd);

    posix_kqueue_setfd(filt->kf_kqueue, filt->kf_pfd);

    if ((ed = calloc(1, sizeof(*ed))) == NULL)
        return (-1);


    pthread_mutex_lock(&wait_mtx);
    while (wait_thread_running_flag) {
        wait_thread_dead_flag = 1;
        pthread_mutex_unlock(&wait_mtx);
    }
    
    wait_thread_running_flag = 1;
    wait_thread_dead_flag = 0;
    wait_thread_filt = filt;

    pthread_mutex_unlock(&wait_mtx);

    if (pthread_create(&ed->wthr_id, NULL, wait_thread, filt) != 0) 
        goto errout;

    return (0);

errout:
    free(ed);
    return (-1);
}

void
posix_evfilt_proc_destroy(struct filter *filt)
{
    kqops.eventfd_close(&filt->kf_efd);

    pthread_mutex_lock(&wait_mtx);
    wait_thread_dead_flag = 1;
    pthread_cond_signal(&wait_cond);
    pthread_mutex_unlock(&wait_mtx);

    return;
}


int
posix_evfilt_proc_knote_create(struct filter *filt, struct knote *kn)
{
    pthread_mutex_lock(&wait_mtx);
    pthread_cond_signal(&wait_cond);
    wait_thread_filt = filt;
    pthread_mutex_unlock(&wait_mtx);
    return (0);
}

int
posix_evfilt_proc_knote_modify(struct filter *filt, struct knote *kn, const struct kevent *kev)
{
    pthread_mutex_lock(&wait_mtx);
    pthread_cond_signal(&wait_cond);
    wait_thread_filt = filt;
    pthread_mutex_unlock(&wait_mtx);
    return (0);
}


#if 0
int
posix_evfilt_proc_copyin(struct filter *filt, 
        struct knote *dst, const struct kevent *src)
{
    if (src->flags & EV_ADD) {
        memcpy(&dst->kev, src, sizeof(*src));
        /* TODO: think about locking the mutex first.. */
        pthread_cond_signal(&wait_cond);
    }

    if (src->flags & EV_ADD || src->flags & EV_ENABLE) {
        /* Nothing to do.. */
    }

    return (0);
}
#endif


int
posix_evfilt_proc_copyout(struct kevent *dst, struct knote *src, void *ptr UNUSED)
{
    struct knote *kn;
    int nevents = 0;
    struct filter *filt;
    char buf[1024];
    uintptr_t ident;

    filt = (struct filter *)ptr;

    pthread_mutex_lock(&wait_mtx);
    pthread_cond_signal(&wait_cond);
    wait_thread_filt = filt;
    pthread_mutex_unlock(&wait_mtx);


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
  
#if 0
    if (src->kev.flags & EV_ADD) {
        dst->flags &= ~EV_ADD;
    }

    if (src->kev.flags & EV_DISPATCH) {
        dst->flags &=  ~EV_DISPATCH;
    }
#endif
    return (1);
}

int
posix_evfilt_proc_knote_delete(struct filter *filt, struct knote *kn)
{
    pthread_mutex_lock(&wait_mtx);
    pthread_cond_signal(&wait_cond);
    wait_thread_filt = filt;
    pthread_mutex_unlock(&wait_mtx);

    return (0);
}

int
posix_evfilt_proc_knote_enable(struct filter *filt, struct knote *kn)
{
    pthread_mutex_lock(&wait_mtx);
    pthread_cond_signal(&wait_cond);
    wait_thread_filt = filt;
    pthread_mutex_unlock(&wait_mtx);

    return (0);
}

int
posix_evfilt_proc_knote_disable(struct filter *filt, struct knote *kn)
{
    pthread_mutex_lock(&wait_mtx);
    pthread_cond_signal(&wait_cond);
    wait_thread_filt = filt;
    pthread_mutex_unlock(&wait_mtx);

    return (0);
}



const struct filter evfilt_proc = {
    EVFILT_USER,
    posix_evfilt_proc_init,
    posix_evfilt_proc_destroy,
    posix_evfilt_proc_copyout,
    posix_evfilt_proc_knote_create,
    posix_evfilt_proc_knote_modify,
    posix_evfilt_proc_knote_delete,
    posix_evfilt_proc_knote_enable,
    posix_evfilt_proc_knote_disable,
};
