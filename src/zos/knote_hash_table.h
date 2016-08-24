/* 2016 IBM All Rights Reserved */
#ifndef KNOTE_HASH_TABLE_H
#define KNOTE_HASH_TABLE_H

struct knote;
/* knote_map maps fd to knote */
struct knote_map_bucket_s;
typedef struct knote_map_bucket_s **knote_map_t;
knote_map_t knote_map_init();
void knote_map_destroy(knote_map_t fd_map);
void knote_map_insert(knote_map_t knote_map, int fd, struct knote *knote);
void knote_map_remove(knote_map_t knote_map, int fd);
struct knote *knote_map_lookup(knote_map_t knote_map, int fd);

#endif /* KNOTE_HASH_TABLE_H */
