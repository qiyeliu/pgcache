#ifndef PGCACHE_H
#define PGCACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sched.h>
#include <sys/ioctl.h>

#define PGCACHE_VERSION "1.0.0"

/* ---- Data structures ---- */

typedef struct {
    char    *name;          /* strdup'd file path */
    int64_t  size;          /* file size in bytes */
    time_t   timestamp;     /* time when mincore was called */
    time_t   mtime;         /* file modification time */
    int      pages;         /* total pages */
    int      cached;        /* pages in cache */
    int      uncached;      /* pages not in cache */
    double   percent;       /* cached/pages * 100.0 */
    char    *ppstat;        /* per-page status: one byte per page, 1=cached (NULL unless --histo/--pps) */
} cache_entry_t;

typedef struct {
    cache_entry_t *entries;
    int            count;
    int            capacity;
} entry_list_t;

typedef struct {
    int pid;              /* --pid PID */
    int top;              /* --top N */
    int nohdr;            /* --nohdr */
    int bname;            /* --bname */
    int terse;            /* --terse */
    int json;             /* --json */
    int histo;            /* --histo */
    int unicode;          /* --unicode */
    int pps;              /* --pps */
    int plain;            /* --plain */
} opts_t;

extern opts_t g_opts;

/* ---- process.c ---- */

int   *discover_pids(int *count);
int    get_pid_rss(int pid);
char **get_pid_maps(int pid, int *count);
void   switch_mount_ns(int pid);
int    dedup_strings(char **arr, int count);

/* ---- mincore.c ---- */

int   get_file_cache_status(const char *path, cache_entry_t *entry, int keep_ppstat);
char *human_size(int64_t bytes, char *buf);

/* ---- format.c ---- */

void sort_by_cached(entry_list_t *list);
int  max_name_len(const entry_list_t *list);
void format_stats(const entry_list_t *list);

/* ---- pgcache.c ---- */

void list_init(entry_list_t *list);
void list_push(entry_list_t *list, const cache_entry_t *entry);
void list_free(entry_list_t *list);

#endif /* PGCACHE_H */
