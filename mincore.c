#include "pgcache.h"

/* Convert byte count to human-readable string (B/K/M/G/T/P). Returns buf. */
char *human_size(int64_t bytes, char *buf) {
    const int64_t KB = 1024;
    const int64_t MB = 1024 * KB;
    const int64_t GB = 1024 * MB;
    const int64_t TB = (int64_t)1024 * GB;
    const int64_t PB = (int64_t)1024 * TB;

    if (bytes >= PB)
        sprintf(buf, "%.3fP", (double)bytes / (double)PB);
    else if (bytes >= TB)
        sprintf(buf, "%.3fT", (double)bytes / (double)TB);
    else if (bytes >= GB)
        sprintf(buf, "%.3fG", (double)bytes / (double)GB);
    else if (bytes >= MB)
        sprintf(buf, "%.3fM", (double)bytes / (double)MB);
    else if (bytes >= KB)
        sprintf(buf, "%.3fK", (double)bytes / (double)KB);
    else
        sprintf(buf, "%ldB", (long)bytes);

    return buf;
}

/* Core function: open file, stat, mmap, mincore, count cached pages.
 * Returns 0 on success, -1 on error. */
int get_file_cache_status(const char *path, cache_entry_t *entry, int keep_ppstat) {
    int fd;
    struct stat st;
    void *addr;
    int page_size;
    int vecsz, pages, cached;
    unsigned char *vec;

    memset(entry, 0, sizeof(*entry));

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    if (fstat(fd, &st) < 0) {
        close(fd);
        return -1;
    }

    /* skip directories */
    if (S_ISDIR(st.st_mode)) {
        close(fd);
        return -1;
    }

    /* skip zero-size files (mmap of 0 bytes is undefined) */
    if (st.st_size == 0) {
        close(fd);
        return -1;
    }

    addr = mmap(NULL, st.st_size, PROT_NONE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "skipping %s: mmap: %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }

    page_size = sysconf(_SC_PAGESIZE);
    vecsz = (st.st_size + page_size - 1) / page_size;

    vec = malloc(vecsz);
    if (!vec) {
        perror("malloc");
        munmap(addr, st.st_size);
        close(fd);
        return -1;
    }

    if (mincore(addr, (size_t)st.st_size, vec) != 0) {
        fprintf(stderr, "skipping %s: mincore: %s\n", path, strerror(errno));
        free(vec);
        munmap(addr, st.st_size);
        close(fd);
        return -1;
    }

    /* count cached pages (LSB of each byte) */
    pages = vecsz;
    cached = 0;
    for (int i = 0; i < vecsz; i++) {
        if (vec[i] & 1)
            cached++;
    }

    /* fill entry */
    entry->name = strdup(path);
    entry->size = st.st_size;
    entry->timestamp = time(NULL);
    entry->mtime = st.st_mtime;
    entry->pages = pages;
    entry->cached = cached;
    entry->uncached = pages - cached;
    entry->percent = pages > 0 ? ((double)cached / (double)pages) * 100.0 : 0.0;

    if (keep_ppstat) {
        entry->ppstat = malloc(vecsz);
        if (entry->ppstat) {
            for (int i = 0; i < vecsz; i++)
                entry->ppstat[i] = (vec[i] & 1) ? 1 : 0;
        }
    } else {
        entry->ppstat = NULL;
    }

    free(vec);
    munmap(addr, st.st_size);
    close(fd);
    return 0;
}
