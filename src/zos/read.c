
#include "private.h"

int
evfilt_read_copyout(struct kevent *dst, struct knote *src, void *ptr)
{
    return -1;
}

int
evfilt_read_knote_create(struct filter *filt, struct knote *kn)
{
    return -1;
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
    return (-1);
}

int
evfilt_read_knote_enable(struct filter *filt, struct knote *kn)
{
    return (-1);
}

int
evfilt_read_knote_disable(struct filter *filt, struct knote *kn)
{
    return (-1);
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
