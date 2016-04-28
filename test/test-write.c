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


#include "common.h"

void
test_kevent_write_add(struct test_context *ctx)
{
    struct kevent kev;

    kevent_add(ctx->kqfd, &kev, ctx->server_fd, EVFILT_WRITE, EV_ADD, 0, 0, &ctx->server_fd);
}


void
test_kevent_write_add_without_ev_add(struct test_context *ctx)
{
    struct kevent kev;

    /* Try to add a kevent without specifying EV_ADD */
    EV_SET(&kev, ctx->server_fd, EVFILT_WRITE, 0, 0, 0, &ctx->server_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) == 0)
        die("kevent should have failed");

    kevent_socket_fill(ctx);
    test_no_kevents(ctx->kqfd);
    kevent_socket_drain(ctx);

    /* Try to delete a kevent which does not exist */
    kev.flags = EV_DELETE;
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) == 0)
        die("kevent should have failed");
}

void
test_kevent_write_get(struct test_context *ctx)
{
    struct kevent kev, ret;

    EV_SET(&kev, ctx->server_fd, EVFILT_WRITE, EV_ADD, 0, 0, &ctx->server_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        die("kevent:1");

    kevent_socket_fill(ctx);

    kev.data = 0;
    kevent_get(&ret, ctx->kqfd);
    kevent_cmp(&kev, &ret);

    kevent_socket_drain(ctx);
    //test_no_kevents(ctx->kqfd);

    kev.flags = EV_DELETE;
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        die("kevent:2");
}

void
test_kevent_write_clear(struct test_context *ctx)
{
    struct kevent kev, ret;

    test_no_kevents(ctx->kqfd);
    kevent_socket_drain(ctx);

    EV_SET(&kev, ctx->server_fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, &ctx->server_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        die("kevent1");

    kevent_socket_fill(ctx);
    kevent_socket_fill(ctx);

    kev.data = 0;

    kevent_get(&ret, ctx->kqfd);
    kevent_cmp(&kev, &ret); 

    /* We filled twice, but drain once. Edge-triggered would not generate
       additional events.
     */
    kevent_socket_drain(ctx);
    test_no_kevents(ctx->kqfd);

    kevent_socket_drain(ctx);
    EV_SET(&kev, ctx->server_fd, EVFILT_WRITE, EV_DELETE, 0, 0, &ctx->server_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        die("kevent2");
}

void
test_kevent_write_disable_and_enable(struct test_context *ctx)
{
    struct kevent kev, ret;

    /* Add an event, then disable it. */
    EV_SET(&kev, ctx->server_fd, EVFILT_WRITE, EV_ADD, 0, 0, &ctx->server_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        die("kevent");
    EV_SET(&kev, ctx->server_fd, EVFILT_WRITE, EV_DISABLE, 0, 0, &ctx->server_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        die("kevent");

    kevent_socket_fill(ctx);
    test_no_kevents(ctx->kqfd);

    /* Re-enable the knote, then see if an event is generated */
    kev.flags = EV_ENABLE;
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        die("kevent");
    kev.flags = EV_ADD;
    kev.data = 0;
    kevent_get(&ret, ctx->kqfd);
    kevent_cmp(&kev, &ret); 

    kevent_socket_drain(ctx);

    kev.flags = EV_DELETE;
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        die("kevent");
}

void
test_kevent_write_del(struct test_context *ctx)
{
    struct kevent kev;

    EV_SET(&kev, ctx->server_fd, EVFILT_WRITE, EV_DELETE, 0, 0, &ctx->server_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        die("kevent");

    kevent_socket_fill(ctx);
    test_no_kevents(ctx->kqfd);
    kevent_socket_drain(ctx);
}

void
test_kevent_write_oneshot(struct test_context *ctx)
{
    struct kevent kev, ret;

    /* Re-add the watch and make sure no events are pending */
    kevent_add(ctx->kqfd, &kev, ctx->server_fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, &ctx->server_fd);
    //test_no_kevents(ctx->kqfd);

    kevent_socket_fill(ctx);
    kev.data = 0;
    kevent_get(&ret, ctx->kqfd);
    kevent_cmp(&kev, &ret); 

    test_no_kevents(ctx->kqfd);

    /* Verify that the kernel watch has been deleted */
    kevent_socket_fill(ctx);
    test_no_kevents(ctx->kqfd);
    kevent_socket_drain(ctx);

    /* Verify that the kevent structure does not exist. */
    kev.flags = EV_DELETE;
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) == 0)
        die("kevent() should have failed");
}



void
test_evfilt_write(struct test_context *ctx)
{
    unsigned short port = 23457;
    char * str = getenv("TEST_PORT2");
    if (str) {
       unsigned short v = atoi(str);
       printf("port %u is used\n", v);
       port = v; 
    }
    else {
       printf("TEST_PORT not set using %u\n", port);
    }
    create_socket_connection(&ctx->client_fd, &ctx->server_fd, ctx->iteration + port );

    test(kevent_write_add, ctx, "dummy");
    test(kevent_write_del, ctx, "dummy");
    test(kevent_write_add_without_ev_add, ctx, "dummy");
    test(kevent_write_get, ctx, "dummy");
    test(kevent_write_disable_and_enable, ctx, "dummy");
    test(kevent_write_oneshot, ctx, "dummy");
    test(kevent_write_clear, ctx, "dummy");
    close(ctx->client_fd);
    close(ctx->server_fd);
}
