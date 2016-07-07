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

#ifndef  _KQUEUE_POSIX_PLATFORM_H
#define  _KQUEUE_POSIX_PLATFORM_H

// #define _OE_SOCKETS
#define _OPEN_MSGQ_EXT
/* Required by glibc for MAP_ANON */
#define __USE_MISC 1

#include "../../include/sys/event.h"
/*
#ifndef __MVS__
#  error zos only
#endif
*/
#include <assert.h>

/*
 * GCC-compatible atomic operations 
 */
#define atomic_inc(p)   __sync_add_and_fetch((p), 1)
#define atomic_dec(p)   __sync_sub_and_fetch((p), 1)
#define atomic_cas(p, oval, nval) __sync_val_compare_and_swap(p, oval, nval)
#define atomic_ptr_cas(p, oval, nval) __sync_val_compare_and_swap(p, oval, nval)
/*
__inline long long __zsync_val_compare_and_swap64 (long long * __p, long long  __compVal, long long  __exchVal ) {
   // This function compares the value of __compVal to the value of the variable that __p points to.
   // If they are equal, the value of __exchVal is stored in the address that is specified by __p;
   // otherwise, no operation is performed.
   // Return value The function returns the initial value of the variable that __p points to.
   long long initv;
   __asm( " csg %1,%3,%2 \n "
          " stg %1,%0 \n"
          : "=m"(initv), "+r"(__compVal), "+m"(*__p)
          : "r"(__exchVal)
          : );
   return initv;
}
__inline int __zsync_val_compare_and_swap32 ( int * __p, int __compVal, int __exchVal ) {
   // This function compares the value of __compVal to the value of the variable that __p points to.
   // If they are equal, the value of __exchVal is stored in the  address that is specified by __p;
   // otherwise, no operation is performed.
   // Return value The function returns the initial value of the variable that __p points to.
   int initv;
   __asm( " cs  %1,%3,%2 \n "
          " st %1,%0 \n"
          : "=m"(initv), "+r"(__compVal), "+m"(*__p)
          : "r"(__exchVal)
          : );
   return initv;
}

__inline void __atomic_inc32(int *p) {
   assert(!(((int )p) & 0x00000003)); // boundary alignment required
   assert(0x04 & *(const char *)205); // interlock-access fac 1 present
   __asm( " asi %0,1\n "
          : "=m"(*p)
          : );
   
}
__inline void __atomic_inc64(long long *p) {
   assert(!(((int )p) & 0x00000007)); // boundary alignment required
   assert(0x04 & *(const char *)205); // interlock-access fac 1 present
   __asm( " agsi %0,1\n "
          : "=m"(*p)
          : );
}
__inline void __atomic_dec32(int *p) {
   assert(!(((int )p) & 0x00000003)); // boundary alignment required
   assert(0x04 & *(const char *)205); // interlock-access fac 1 present
   __asm( " asi %0,-1\n "
          : "=m"(*p)
          : );
}
__inline void __atomic_dec64(long long *p) {
   assert(!(((int )p) & 0x00000007)); // boundary alignment required
   assert(0x04 & *(const char *)205); // interlock-access fac 1 present
   __asm( " agsi %0,-1\n "
          : "=m"(*p)
          : );
}

static inline void * atomic_ptr_cas(void **p, void *oval, void *nval)
{
#ifdef _LP64
  return (void *) __zsync_val_compare_and_swap64((long long *)p, (long long) oval, (long long) nval);
#else
  return (void *) __zsync_val_compare_and_swap32((int *)p, (int) oval, (int) nval);
#endif
}

// old static inline void *
// old atomic_ptr_cas(void **p, void *oval, void *nval)
// old {
// old #ifdef __64BIT__
// old     __cds1(&oval, p, &nval);
// old #else
// old     __cs1(&oval, p, &nval);
// old #endif
// old     return oval;
// old }
// old 
// old static inline unsigned int
// old __atomic_add(unsigned int *p, int val)
// old {
// old     unsigned int tmp, old;
// old     do {
// old         old = *p;
// old         tmp = old + val;
// old     } while (__cs1(&old, p, &tmp));
// old 
// old     return tmp;
// old }
// old 
// old #define atomic_inc(p)   (__atomic_add(p, 1))
// old #define atomic_dec(p)   (__atomic_add(p, -1))
// old 

__inline int atomic_inc(int * p) {
  int v0;
  int v1;
  do {
    v0 = *p;
    v1 = __zsync_val_compare_and_swap32((int *)p, v0 , v0+1);
  }
  while (v1 != v0);
  return v0;
}
__inline int atomic_dec(int * p) {
  int v0;
  int v1;
  do {
    v0 = *p;
    v1 = __zsync_val_compare_and_swap32((int *)p, v0 , v0-1);
  }
  while (v1 != v0);
  return v0;
}
*/

/*
 * GCC-compatible branch prediction macros
 */
#define fastpath(x)     __builtin_expect((x), 1)
#define slowpath(x)     __builtin_expect((x), 0)

/*
 * GCC-compatible attributes
 */
#ifdef __MVS__
#define _OPEN_MSGQ_EXT
#define VISIBLE     
#define HIDDEN      
#define UNUSED      
#else
#define VISIBLE         __attribute__((visibility("default")))
#define HIDDEN          __attribute__((visibility("hidden")))
#define UNUSED          __attribute__((unused))
#endif

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <poll.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>


/*
 * Additional members of 'struct eventfd'
 */
#define EVENTFD_PLATFORM_SPECIFIC \
    int ef_wfd; int ef_sig

/* forward declaration */
struct kqueue;
struct eventfd;
struct knote;

void    posix_kqueue_free(struct kqueue *);
int     posix_kqueue_init(struct kqueue *);

int     posix_kevent_wait(struct kqueue *, int nevents, const struct timespec *);
int     posix_kevent_copyout(struct kqueue *, int, struct kevent *, int);

int     posix_eventfd_init(struct eventfd *);
void    posix_eventfd_close(struct eventfd *);
int     posix_eventfd_raise(struct eventfd *);
int     posix_eventfd_lower(struct eventfd *);
int     posix_eventfd_descriptor(struct eventfd *);


typedef struct tlsflat {
   char buf1[64];
   char buf2[1024];
   char buf3[1024];
   char buf4[1024];
} tlsflat_t;

struct tlsflat * get_tls();

/* z/OS related prototypes */
int zos_get_descriptor_type(struct knote *kn);
void posix_kqueue_setfd(struct kqueue *kq, int fd);

#endif  /* ! _KQUEUE_POSIX_PLATFORM_H */
