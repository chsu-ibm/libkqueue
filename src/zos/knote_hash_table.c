#include "knote_hash_table.h"
#include <string.h> /* memset */
#include <pthread.h> /* pthread_mutex */
#include <stdlib.h> /* malloc */

#define KNOTE_MAP_ENTRIES 64
#define KNOTE_MAP_MASK (~(KNOTE_MAP_ENTRIES - 1))

struct knote_map_bucket_s {
    int fd;
    struct knote *kn;
    struct knote_map_bucket_s *next;
};

int
knote_map_hash(int fd)
{
    return fd & KNOTE_MAP_MASK;
}

/* This idea of free knote map comes from this post:
 * http://www.ibm.com/developerworks/aix/tutorials/au-memorymanager */
struct free_knote_map_s {
    struct free_knote_map_s *next;
};
struct free_knote_map_s *free_map_head;
struct knote_map_bucket_s *free_bucket_head;

static struct knote_map_bucket_s *
knote_map_bucket_s_allocate()
{
    const unsigned int N = 64;
    struct knote_map_bucket_s *pool;
    struct knote_map_bucket_s *I, *E;

    /* All buckets in this pool will be either allocated for use or they will be
     * in the free bucket list. No memory will leak. */
    pool = malloc(N * sizeof(*pool));

    /* link them together */
    for (I = pool, E = I + N; I != E; ++I) {
        I->next = I + 1;
    }
    (E - 1)->next = NULL;

    return pool;
}

/* this mutex is to lock operations for free_map_head and free_bucket_head */
pthread_mutex_t free_map_mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t *free_map_mutex = &free_map_mutex1;
#define LOCK(m) pthread_mutex_lock(m)
#define UNLOCK(m) pthread_mutex_unlock(m)

knote_map_t
knote_map_init()
{
    size_t size = KNOTE_MAP_ENTRIES * sizeof(void *);
    knote_map_t knote_map;
    LOCK(free_map_mutex);
    if (free_map_head) {
        knote_map = (knote_map_t)free_map_head;
        free_map_head = free_map_head->next;
        UNLOCK(free_map_mutex);
    } else {
        UNLOCK(free_map_mutex);
        knote_map = (knote_map_t)malloc(size);
    }
    memset(knote_map, 0, size);
    return knote_map;
}

void
knote_map_destroy(knote_map_t knote_map)
{
    if (knote_map) {
        LOCK(free_map_mutex);
        /* insert all buckets to free bucket list */
        unsigned int i;
        for (i = 0; i != KNOTE_MAP_ENTRIES; ++i) {
            if (knote_map[i]) {
                struct knote_map_bucket_s **I = &knote_map[i]->next;
                while (*I) I = &((*I)->next);
                *I = free_bucket_head;
                free_bucket_head = knote_map[i];
                knote_map[i] = NULL;
            }
        }

        /* insert map to free_knote_map */
        struct free_knote_map_s *map = (struct free_knote_map_s *)knote_map;
        map->next = free_map_head;
        free_map_head = map;
        UNLOCK(free_map_mutex);
    }
}

void
knote_map_insert(knote_map_t knote_map, int fd, struct knote *kn)
{
    int index;
    struct knote_map_bucket_s *bucket;

    LOCK(free_map_mutex);
    if (free_bucket_head == NULL) {
        free_bucket_head = knote_map_bucket_s_allocate();
    }
    bucket = free_bucket_head;
    free_bucket_head = free_bucket_head->next;
    UNLOCK(free_map_mutex);

    bucket->fd = fd;
    bucket->kn = kn;

    /* insert to the map */
    index = knote_map_hash(fd);
    bucket->next = knote_map[index];
    knote_map[index] = bucket;
}

void
knote_map_remove(knote_map_t knote_map, int fd)
{
    int index = knote_map_hash(fd);
    if (knote_map[index]) {
        struct knote_map_bucket_s *T;
        struct knote_map_bucket_s **I = &knote_map[index];
        for (T = *I; T; I = &T->next, T = *I) {
            if (T->fd == fd) {
                /* remove this T */
                *I = T->next;
                /* insert T to free_bucket_list */
                LOCK(free_map_mutex);
                T->next = free_bucket_head;
                free_bucket_head = T;
                UNLOCK(free_map_mutex);
                break;
            }
        }
    }
}

struct knote *
knote_map_lookup(knote_map_t knote_map, int fd)
{
    int index = knote_map_hash(fd);
    struct knote *kn = NULL;
    if (knote_map[index]) {
        struct knote_map_bucket_s *T;
        struct knote_map_bucket_s **I = &knote_map[index];
        for (T = *I; T; I = &T->next, T = *I) {
            if (T->fd == fd) {
                kn = T->kn;
                break;
            }
        }
    }
    return kn;
}
