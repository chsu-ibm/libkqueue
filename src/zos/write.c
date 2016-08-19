
#include "private.h"

#include <sys/ioctl.h>

int
evfilt_socket_copyout(struct kevent *dst, struct knote *src, void *ptr)
{
    int data;

    memcpy(dst, &src->kev, sizeof(*dst));

    /* On return, data contains the the amount of space remaining in the write buffer */
    if (ioctl(dst->ident, TIOCOUTQ, &data) < 0) {
            /* race condition with socket close, so ignore this error */
            dbg_puts("ioctl(2) of socket failed");
            data = 0;
    }

    dst->data = data;

    return 1;
}

int
evfilt_socket_knote_create(struct filter *filt, struct knote *kn)
{
    struct kqueue *kq;
    int fd;

    kq = kn->kn_kq;    
    fd = (int)kn->kev.ident;

    if (zos_get_descriptor_type(kn) < 0)
        return (-1);

    /* TODO: return EBADF? */
    if (kn->kn_flags & KNFL_REGULAR_FILE)
        return (-1);

    posix_kqueue_setfd_write(kq, fd);

    return 0;
}

int
evfilt_socket_knote_modify(struct filter *filt, struct knote *kn, 
        const struct kevent *kev)
{
    (void) filt;
    (void) kn;
    (void) kev;
    return (-1); /* STUB */
}

int
evfilt_socket_knote_delete(struct filter *filt, struct knote *kn)
{
    struct kqueue *kq;
    int fd;

    if (kn->kev.flags & EV_DISABLE)
        return (0);

    kq = kn->kn_kq;    
    fd = (int)kn->kev.ident;

    fprintf(stderr, "evfilt_socket_knote_delete called\n");
    fprintf(stderr, "  fd=%d\n", fd);

    posix_kqueue_clearfd_write(kq, fd);

    return 0;
}

int
evfilt_socket_knote_enable(struct filter *filt, struct knote *kn)
{
    struct kqueue *kq;
    int fd;

    kq = kn->kn_kq;    
    fd = (int)kn->kev.ident;

    fprintf(stderr, "evfilt_socket_knote_enable is called. fd=%d\n", fd);

    posix_kqueue_setfd_write(kq, fd);

    return 0;
}

int
evfilt_socket_knote_disable(struct filter *filt, struct knote *kn)
{
    struct kqueue *kq;
    int fd;

    kq = kn->kn_kq;    
    fd = (int)kn->kev.ident;

    fprintf(stderr, "evfilt_socket_knote_disable is called. fd=%d\n", fd);

    posix_kqueue_clearfd_write(kq, fd);

    return 0;
}

int
evfilt_socket_init(struct filter *filt)
{
    filt->fd_to_ident = default_fd_to_ident;
    return 0;
}

const struct filter evfilt_write = {
    EVFILT_WRITE,
    evfilt_socket_init,
    NULL,
    evfilt_socket_copyout,
    evfilt_socket_knote_create,
    evfilt_socket_knote_modify,
    evfilt_socket_knote_delete,
    evfilt_socket_knote_enable,
    evfilt_socket_knote_disable,         
};
