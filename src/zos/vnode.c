
#include "private.h"

int
evfilt_vnode_copyout(struct kevent *dst, struct knote *src, void *ptr UNUSED)
{
    return (-1);
}

int
evfilt_vnode_knote_create(struct filter *filt, struct knote *kn)
{
    return (-1);
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
    return (-1);
}

int
evfilt_vnode_knote_enable(struct filter *filt, struct knote *kn)
{
    return (-1);
}

int
evfilt_vnode_knote_disable(struct filter *filt, struct knote *kn)
{
    return (-1);
}

const struct filter evfilt_vnode = {
    EVFILT_VNODE,
    NULL,
    NULL,
    evfilt_vnode_copyout,
    evfilt_vnode_knote_create,
    evfilt_vnode_knote_modify,
    evfilt_vnode_knote_delete,
    evfilt_vnode_knote_enable,
    evfilt_vnode_knote_disable,        
};
