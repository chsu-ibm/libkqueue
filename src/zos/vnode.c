#define _OPEN_SYS

#include <errno.h>
//#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include "../common/queue.h"
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>

#include <limits.h>



#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <termios.h>

#include "sys/event.h"

#include "private.h"

static pthread_cond_t   vnode_wait_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t  vnode_wait_mtx = PTHREAD_MUTEX_INITIALIZER;

static int vnode_wait_thread_running_flag = 0;
static int vnode_wait_thread_dead_flag = 0;
static struct filter *vnode_wait_thread_filt = NULL;

#define VNODE_MSGQ_KEY 12345
static int vnode_msgq_id = 0;

struct evfilt_data {
    pthread_t       wthr_id;
};


static void *
vnode_wait_thread(void *arg)
{
    struct filter *filt = (struct filter *) arg;
    struct knote *kn;
    int status, result;
    int size;
    int mtype;
    sigset_t sigmask;
    struct {
        long mtype;
        char mtext[512];
    } msgbuf;
    _RFIS rfis;
    _RFIM *rfim;

    /* Block all signals */
    sigfillset (&sigmask);
    sigdelset(&sigmask, SIGCHLD);
    pthread_sigmask(SIG_BLOCK, &sigmask, NULL);

    for (;;) {

        /* Wait for a child process to exit(2) */

        
        if ((size = msgrcv(vnode_msgq_id, &msgbuf, sizeof(msgbuf.mtext), 0L, 0)) < 0) {
            dbg_printf("msgrcv(2): %s", strerror(errno));
            break;
        } 

        dbg_printf("msgrcv(2) returns %d", size);

        pthread_mutex_lock(&vnode_wait_mtx);
        if (vnode_wait_thread_dead_flag) {
            vnode_wait_thread_running_flag = 0;
            pthread_mutex_unlock(&vnode_wait_mtx);
            dbg_puts("vnode_wait_thread exiting...");
            break;
        }
        filt = vnode_wait_thread_filt;
        pthread_mutex_unlock(&vnode_wait_mtx);

        rfim = (_RFIM *) &msgbuf;

        dbg_printf("rfim.__rfim_type=%d\n", (int)rfim->__rfim_type);
        dbg_printf("rfim.__rfim_event=%d\n", (int)rfim->__rfim_event);

        mtype = (int)rfim->__rfim_type;

        /* Scan the wait queue to see if anyone is interested */
        pthread_rwlock_wrlock(&filt->kf_knote_mtx);
        kn = knote_lookup(filt, mtype);
        dbg_printf("filt=0x%p, kn=0x%p", filt, kn);

        if (kn != NULL) {
            dbg_puts("raising event level");

            kn->data.vnode.mtype = mtype;

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
evfilt_vnode_init(struct filter *filt)
{
    struct evfilt_data *ed;

    filt->fd_to_ident = default_fd_to_ident;

    if (kqops.eventfd_init(&filt->kf_efd) < 0)
        return (-1);

    vnode_msgq_id = msgget(VNODE_MSGQ_KEY, IPC_CREAT | 0600);
    if (vnode_msgq_id < 0) {
        dbg_printf("msgget(2): %s", strerror(errno));
        return (-1);
    }

    filt->kf_pfd = kqops.eventfd_descriptor(&filt->kf_efd);

    posix_kqueue_setfd_read(filt->kf_kqueue, filt->kf_pfd);

    if ((ed = calloc(1, sizeof(*ed))) == NULL)
        return (-1);

#if 0
    pthread_mutex_lock(&vnode_wait_mtx);
    while (vnode_wait_thread_running_flag) {
        vnode_wait_thread_dead_flag = 1;
        pthread_mutex_unlock(&vnode_wait_mtx);
    }
#endif
    vnode_wait_thread_running_flag = 1;
    vnode_wait_thread_dead_flag = 0;
    vnode_wait_thread_filt = filt;

    pthread_mutex_unlock(&vnode_wait_mtx);

    if (pthread_create(&ed->wthr_id, NULL, vnode_wait_thread, filt) != 0) 
        goto errout;

    return (0);

errout:
    free(ed);
    return (-1);
}


int
evfilt_vnode_copyout(struct kevent *dst, struct knote *src, void *ptr UNUSED)
{
    struct knote *kn;
    int nevents = 0;
    struct filter *filt;
    char buf[1024];
    uintptr_t ident;
    struct stat sb;
    int fd;

    filt = (struct filter *)ptr;

    pthread_mutex_lock(&vnode_wait_mtx);
    pthread_cond_signal(&vnode_wait_cond);
    vnode_wait_thread_filt = filt;
    pthread_mutex_unlock(&vnode_wait_mtx);

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

    memcpy(dst, &src->kev, sizeof(*dst));

    fd = ident;
    if (fstat(fd, &sb) < 0) {
        dbg_puts("fstat failed");
        return (-1);
    }

    if (src->data.vnode.mtype == _RFIM_UNLINK && src->kev.fflags & NOTE_DELETE) 
        dst->fflags |= NOTE_DELETE;


    if (sb.st_size > src->data.vnode.size && src->kev.fflags & NOTE_WRITE) 
        dst->fflags |= NOTE_EXTEND;
    
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
evfilt_vnode_knote_create(struct filter *filt, struct knote *kn)
{
    struct stat sb;
    int fd;
    int ret;
    _RFIS rfis;
    
    fd = kn->kev.ident;

    if (fstat(fd, &sb) < 0) {
        dbg_puts("fstat failed");
        return (-1);
    }
    
    rfis.__rfis_cmd = _RFIS_REG;
    rfis.__rfis_flags = 0;
    rfis.__rfis_qid = vnode_msgq_id;
    rfis.__rfis_type = fd;

    kn->data.vnode.mqid = vnode_msgq_id;
    kn->data.vnode.size = sb.st_size;
    kn->data.vnode.mtype = 0;
    ret = w_ioctl(fd, _IOCC_REGFILEINT, sizeof(rfis), &rfis);
    dbg_printf("w_ioctl(2) fd=%d: ret=%d\n", fd, ret);
    if (ret < 0) {
        return (-1);
    }

    memcpy(kn->data.vnode.rftok, rfis.__rfis_rftok, sizeof(rfis.__rfis_rftok));

    pthread_mutex_lock(&vnode_wait_mtx);
    pthread_cond_signal(&vnode_wait_cond);
    vnode_wait_thread_filt = filt;
    pthread_mutex_unlock(&vnode_wait_mtx);

   
    return 0;
}

int
evfilt_vnode_knote_modify(struct filter *filt, struct knote *kn, 
        const struct kevent *kev)
{
    (void)filt;
    (void)kn;
    (void)kev;
    return (-1); /* FIXME - STUB */
}

int
evfilt_vnode_knote_delete(struct filter *filt, struct knote *kn)
{   
    struct stat sb;
    int fd;
    int ret;
    _RFIS rfis;

    pthread_mutex_lock(&vnode_wait_mtx);
    pthread_cond_signal(&vnode_wait_cond);
    vnode_wait_thread_filt = filt;
    pthread_mutex_unlock(&vnode_wait_mtx);
    
    fd = kn->kev.ident;

    if (fstat(fd, &sb) < 0) {
        dbg_puts("fstat failed");
        return (-1);
    }
    
    rfis.__rfis_cmd = _RFIS_UNREG;
    rfis.__rfis_flags = 0;
    rfis.__rfis_qid = vnode_msgq_id;
    rfis.__rfis_type = fd;
    memcpy(rfis.__rfis_rftok, kn->data.vnode.rftok, sizeof(rfis.__rfis_rftok));

    ret = w_ioctl(fd, _IOCC_REGFILEINT, sizeof(rfis), &rfis);
    dbg_printf("w_ioctl(2) fd=%d: ret=%d\n", fd, ret);
    if (ret < 0) {
        return (-1);
    }

    return (0);
}

int
evfilt_vnode_knote_enable(struct filter *filt, struct knote *kn)
{
    return evfilt_vnode_knote_create(filt, kn);
}

int
evfilt_vnode_knote_disable(struct filter *filt, struct knote *kn)
{
    return evfilt_vnode_knote_delete(filt, kn);
}

const struct filter evfilt_vnode = {
    EVFILT_VNODE,
    evfilt_vnode_init,
    NULL,
    evfilt_vnode_copyout,
    evfilt_vnode_knote_create,
    evfilt_vnode_knote_modify,
    evfilt_vnode_knote_delete,
    evfilt_vnode_knote_enable,
    evfilt_vnode_knote_disable,        
};
