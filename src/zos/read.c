
#include "private.h"

#include <sys/ioctl.h>

        
/*
 * Return the offset from the current position to end of file.
 */
static intptr_t
get_eof_offset(int fd)
{
    off_t curpos;
    struct stat sb;

    curpos = lseek(fd, 0, SEEK_CUR);
    if (curpos == (off_t) -1) {
        dbg_perror("lseek(2)");
        curpos = 0;
    }
    if (fstat(fd, &sb) < 0) {
        dbg_perror("fstat(2)");
        sb.st_size = 1;
    }

    dbg_printf("curpos=%zu size=%zu\n", (size_t)curpos, (size_t)sb.st_size);
    return (sb.st_size - curpos); //FIXME: can overflow
}

int
evfilt_read_copyout(struct kevent *dst, struct knote *src, void *ptr)
{
    struct kqueue *kq;
    int fd;

    kq = src->kn_kq;
    fd = (int)src->kev.ident;

    /* Special case: for regular files, return the offset from current position to end of file */
    if (src->kn_flags & KNFL_REGULAR_FILE) {
        memcpy(dst, &src->kev, sizeof(*dst));
        dst->data = get_eof_offset(src->kev.ident);

        if (dst->data == 0) {
            dst->filter = 0;    /* Will cause the kevent to be discarded */
            FD_CLR(fd, &kq->kq_fds);
            return 0;
        }
    
        return 1;
    }
    
    memcpy(dst, &src->kev, sizeof(*dst));

    fprintf(stderr, "evfilt_read_copyout is called. fd=%d\n", fd);

    if (src->kn_flags & KNFL_PASSIVE_SOCKET) {
        /* On return, data contains the length of the 
           socket backlog. This is not available under Linux.
         */
        dst->data = 1;
    } else {
        /* On return, data contains the number of bytes of protocol
           data available to read.
         */
        int data;

        if (ioctl(dst->ident, FIONREAD, &data) < 0) {
            /* race condition with socket close, so ignore this error */
            dbg_puts("ioctl(2) of socket failed");
            dst->data = 0;
        } else {
            dst->data = data;
            if (dst->data == 0)
                dst->flags |= EV_EOF;
        }
    }

    return 1;
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

    if (zos_get_descriptor_type(kn) < 0)
        return (-1);

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

    kq = kn->kn_kq;    
    fd = (int)kn->kev.ident;

    fprintf(stderr, "evfilt_read_knote_disable is called. fd=%d\n", fd);

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
