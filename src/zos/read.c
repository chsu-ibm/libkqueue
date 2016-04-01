
#include "private.h"

int
evfilt_read_copyout(struct kevent *dst, struct knote *src, void *ptr)
{
    struct kqueue *kq;
    int fd;

    kq = src->kn_kq;    
    fd = (int)src->kev.ident;

    fprintf(stderr, "evfilt_read_copyout is called. fd=%d\n", fd);

    return -1;
}

int
evfilt_read_knote_create(struct filter *filt, struct knote *kn)
{
    struct kqueue *kq;
    int fd;

    kq = kn->kn_kq;    
    fd = (int)kn->kev.ident;

    fprintf(stderr, "evfilt_read_knote_create(struct filter *filt, struct knote *kn) called\n");
    fprintf(stderr, "  fd=%d\n", fd);

    FD_SET(fd, &kq->kq_fds);
    if (kq->kq_nfds <= fd)
        kq->kq_nfds = fd + 1;
    
    return 0;
}

int
evfilt_read_knote_modify(struct filter *filt, struct knote *kn, 
        const struct kevent *kev)
{
    (void) filt;
    (void) kn;
    (void) kev;
    return (-1); /* STUB */
}

int
evfilt_read_knote_delete(struct filter *filt, struct knote *kn)
{
    struct kqueue *kq;
    int fd;

    if (kn->kev.flags & EV_DISABLE)
        return (0);

    kq = kn->kn_kq;    
    fd = (int)kn->kev.ident;

    fprintf(stderr, "evfilt_read_knote_delete called\n");
    fprintf(stderr, "  fd=%d\n", fd);

    FD_CLR(fd, &kq->kq_fds);

    return 0;
}

int
evfilt_read_knote_enable(struct filter *filt, struct knote *kn)
{
    struct kqueue *kq;
    int fd;

    kq = kn->kn_kq;    
    fd = (int)kn->kev.ident;

    fprintf(stderr, "evfilt_read_knote_enable is called. fd=%d\n", fd);

    FD_SET(fd, &kq->kq_fds);
    if (kq->kq_nfds <= fd)
        kq->kq_nfds = fd + 1;

    return 0;
}

int
evfilt_read_knote_disable(struct filter *filt, struct knote *kn)
{
    struct kqueue *kq;
    int fd;

    fprintf(stderr, "evfilt_read_knote_disable is called. fd=%d\n", fd);

    kq = kn->kn_kq;    
    fd = (int)kn->kev.ident;

    FD_CLR(fd, &kq->kq_fds);

    return 0;
}

const struct filter evfilt_read = {
    EVFILT_READ,
    NULL,
    NULL,
    evfilt_read_copyout,
    evfilt_read_knote_create,
    evfilt_read_knote_modify,
    evfilt_read_knote_delete,
    evfilt_read_knote_enable,
    evfilt_read_knote_disable,         
};
