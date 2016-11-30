/*
 * Copyright (c) 2011 Mark Heily <mark@heily.com>
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

#include "../common/private.h"

const uintptr_t INVALID_IDENT = (uintptr_t)0xABABABABABABABABULL;

static void
print_file_state(const struct stat *sb)
{
    printf("File type:                ");
    switch (sb->st_mode & S_IFMT) {
        case S_IFBLK:
            printf("block device\n");
            break;
        case S_IFCHR:
            printf("character device\n");
            break;
        case S_IFDIR:
            printf("directory\n");
            break;
        case S_IFIFO:
            printf("FIFO/pipe\n");
            break;
        case S_IFLNK:
            printf("symlink\n");
            break;
        case S_IFREG:
            printf("regular file\n");
            break;
        case S_IFSOCK:
            printf("socket\n");
            break;
        default:
            printf("unknown?\n");
            break;
    }
    printf("I-node number:            %ld\n", (long)sb->st_ino);
    printf("Preferred I/O block size: %ld bytes\n", (long)sb->st_blksize);
    printf("File size:                %lld bytes\n", (long long)sb->st_size);
    printf("Blocks allocated:         %lld\n", (long long)sb->st_blocks);
}

static int
new_kq_id(void)
{
    int kq_id = open("/dev/null", O_RDONLY);
    if (kq_id == -1) {
        perror("Fail opening /dev/null");
    }
    return kq_id;
}

const struct kqueue_vtable kqops = {
    posix_kqueue_init,
    posix_kqueue_free,
    posix_kevent_wait,
    posix_kevent_copyout,
    NULL,
    NULL,
    posix_eventfd_init,
    posix_eventfd_close,
    posix_eventfd_raise,
    posix_eventfd_lower,
    posix_eventfd_descriptor
};

int
zos_get_descriptor_type(struct knote *kn)
{
    socklen_t slen;
    struct stat sb;
    int i, lsock;
    int fd;

    fd = kn->kev.ident;
    /*
     * Test if the descriptor is a socket.
     */
    if (fstat(fd, &sb) < 0) {
        dbg_perror("fstat(2)");
        return (-1);
    }

    switch (sb.st_mode & S_IFMT) {
        case S_IFREG:
            kn->kn_flags |= KNFL_REGULAR_FILE;
            break;
        /* character or block device */
        case S_IFCHR:
        case S_IFBLK:
            kn->kn_flags |= KNFL_DEVICE;
            break;
        /* fifo is treated as a active socket. */
        case S_IFIFO:
            break;
        case S_IFSOCK:
            slen = sizeof(lsock);
            lsock = 0;
            if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, (char *)&lsock,
                           &slen) < 0) {
                switch (errno) {
                    case ENOTSOCK: /* same as lsock = 0 */
                        return (0);
                        break;
                    default:
                        dbg_perror("getsockopt(3)");
                        return (-1);
                }
            } else {
                if (lsock) kn->kn_flags |= KNFL_PASSIVE_SOCKET;
            }
            break;
        default:
            print_file_state(&sb);
            KQ_ABORT("fd %lu is neither a regular file nor a socket",
                     kn->kev.ident);
            break;
    }
}

int
posix_kqueue_init(struct kqueue *kq)
{
    kq->kq_id = new_kq_id();
    if (kq->kq_id == -1)
        return -1;

    if (filter_register_all(kq) < 0) {
        close(kq->kq_id);
        return (-1);
    }
    return (0);
}

void
posix_kqueue_free(struct kqueue *kq UNUSED)
{
    abort();
}

int
posix_eventfd_init(struct eventfd *e)
{
    int sd[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sd) < 0) {
        return (-1);
    }
    if ((fcntl(sd[0], F_SETFL, O_NONBLOCK) < 0) ||
            (fcntl(sd[1], F_SETFL, O_NONBLOCK) < 0)) {
        close(sd[0]);
        close(sd[1]);
        return (-1);
    }
    e->ef_wfd = sd[0];
    e->ef_id = sd[1];

    return (0);
}

void
posix_eventfd_close(struct eventfd *e)
{
    close(e->ef_id);
    close(e->ef_wfd);
    e->ef_id = -1;
}

int
posix_eventfd_raise(struct eventfd *e)
{
    dbg_puts("raising event level");
    if (write(e->ef_wfd, ".", 1) < 0) {
        /* FIXME: handle EAGAIN and EINTR */
        dbg_printf("write(2) on fd %d: %s", e->ef_wfd, strerror(errno));
        return (-1);
    }
    return (0);
}

int
posix_eventfd_lower(struct eventfd *e)
{
    char buf[1024];

    /* Reset the counter */
    dbg_puts("lowering event level");
    if (read(e->ef_id, &buf, sizeof(buf)) < 0) {
        /* FIXME: handle EAGAIN and EINTR */
        /* FIXME: loop so as to consume all data.. may need mutex */
        dbg_printf("read(2): %s", strerror(errno));
        return (-1);
    }
    return (0);
}

int
posix_eventfd_descriptor(struct eventfd *e)
{
    return (e->ef_id);
}

uintptr_t
default_fd_to_ident(struct filter *filt, int fd)
{
    return (uintptr_t)fd;
}
