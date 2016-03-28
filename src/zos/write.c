
#include "private.h"

int
evfilt_socket_copyout(struct kevent *dst, struct knote *src, void *ptr)
{
    return (-1);
}

int
evfilt_socket_knote_create(struct filter *filt, struct knote *kn)
{
    return (-1);
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
    return (-1);
}

int
evfilt_socket_knote_enable(struct filter *filt, struct knote *kn)
{
    return (-1);
}

int
evfilt_socket_knote_disable(struct filter *filt, struct knote *kn)
{
    return (-1);
}

const struct filter evfilt_write = {
    EVFILT_WRITE,
    NULL,
    NULL,
    evfilt_socket_copyout,
    evfilt_socket_knote_create,
    evfilt_socket_knote_modify,
    evfilt_socket_knote_delete,
    evfilt_socket_knote_enable,
    evfilt_socket_knote_disable,         
};
