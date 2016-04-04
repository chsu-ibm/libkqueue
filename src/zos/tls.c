#include "private.h"
#include "platform.h"

static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_key_t tlskey;

static void process_cleanup_tls();

static void des(void *value) {
  free(value);
  pthread_setspecific(tlskey, 0);
}

static void do_once (void) {
  pthread_key_create(&tlskey, des);
  atexit(process_cleanup_tls);
}

struct tlsflat * get_tls() {
  void * p;
  int rc = pthread_once(&once,do_once);
  if (rc != 0 ) return 0;
  p = pthread_getspecific(tlskey);
  if (p == 0) {
    p = calloc(1,sizeof(tlsflat_t));
    if (p) {
      rc = pthread_setspecific(tlskey,p);
      if (rc == 0) return p;
    }
    return 0;
  }
  return((struct tlsflat*)p);
}

static void process_cleanup_tls() {
  	pthread_key_delete(tlskey);
        fprintf(stderr,"tlskey %lld\n",*(long long *)&tlskey);
}
