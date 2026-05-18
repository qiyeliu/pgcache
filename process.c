#include "pgcache.h"

/* Enumerate all numeric directories in /proc. Returns malloc'd array. */
int *discover_pids(int *count) {
    DIR *d;
    struct dirent *ent;
    int *pids = NULL;
    int cap = 0, n = 0;

    *count = 0;
    d = opendir("/proc");
    if (!d) {
        perror("opendir /proc");
        exit(1);
    }

    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] < '0' || ent->d_name[0] > '9')
            continue;
        int pid = atoi(ent->d_name);
        if (pid <= 0)
            continue;
        if (n >= cap) {
            cap = cap == 0 ? 256 : cap * 2;
            pids = realloc(pids, cap * sizeof(int));
            if (!pids) { perror("realloc"); exit(1); }
        }
        pids[n++] = pid;
    }
    closedir(d);
    *count = n;
    return pids;
}

/* Parse /proc/[pid]/stat, return RSS in pages (field 24). Returns 0 on error. */
int get_pid_rss(int pid) {
    char path[64];
    FILE *f;
    char buf[4096];

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    f = fopen(path, "r");
    if (!f)
        return 0;
    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);

    /* Find closing paren of comm field: ") " */
    char *p = strrchr(buf, ')');
    if (!p)
        return 0;
    /* Skip ") ", then parse fields to find field 24 (rss) */
    p += 2; /* skip ') ' */

    /* Fields after comm: state(3) ppid(4) pgrp(5) sid(6) tty_nr(7) tpgid(8)
       flags(9) minflt(10) cminflt(11) majflt(12) cmajflt(13) utime(14)
       stime(15) cutime(16) cstime(17) priority(18) nice(19) num_threads(20)
       itrealvalue(21) starttime(22) vsize(23) rss(24)
       After comm close, we need to skip 21 fields to reach rss. */
    int field;
    for (field = 0; field < 21 && *p; field++) {
        while (*p && *p != ' ' && *p != '\n') p++;
        while (*p == ' ') p++;
    }
    if (!*p)
        return 0;

    return atoi(p);
}

/* Parse /proc/[pid]/maps, extract file paths from 6th column. */
char **get_pid_maps(int pid, int *count) {
    char path[64];
    FILE *f;
    char *line = NULL;
    size_t linecap = 0;
    char **files = NULL;
    int cap = 0, n = 0;

    *count = 0;
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    f = fopen(path, "r");
    if (!f)
        return NULL;

    while (getline(&line, &linecap, f) != -1) {
        /* Parse the 6th whitespace-delimited field.
         * Format: address perms offset dev inode pathname */
        char *p = line;
        int fld;

        /* Skip 5 fields */
        for (fld = 0; fld < 5; fld++) {
            while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
            while (*p == ' ' || *p == '\t') p++;
        }
        /* Skip whitespace before pathname */
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '\0' || *p == '\n')
            continue;

        /* Pathname must start with '/' */
        if (*p != '/')
            continue;

        /* Trim trailing whitespace/newline */
        char *pathname = p;
        while (*p && *p != '\n') p++;
        while (p > pathname && (*(p-1) == ' ' || *(p-1) == '\t'))
            p--;
        *p = '\0';

        /* Check if we've already seen this path for this pid (simple linear scan) */
        bool dup = false;
        for (int i = 0; i < n; i++) {
            if (strcmp(files[i], pathname) == 0) {
                dup = true;
                break;
            }
        }
        if (dup)
            continue;

        if (n >= cap) {
            cap = cap == 0 ? 64 : cap * 2;
            files = realloc(files, cap * sizeof(char *));
            if (!files) { perror("realloc"); exit(1); }
        }
        files[n++] = strdup(pathname);
    }

    fclose(f);
    free(line);

    *count = n;
    return files;
}

/* Switch mount namespace to target pid's (best-effort, silent on failure). */
void switch_mount_ns(int pid) {
    char ns_path[64];
    int fd;

    snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/mnt", pid);
    fd = open(ns_path, O_RDONLY);
    if (fd < 0)
        return; /* silent: process may have exited or EPERM */

    setns(fd, 0); /* best-effort */
    close(fd);
}

/* Deduplicate a sorted string array in-place. Returns new count. */
int dedup_strings(char **arr, int count) {
    if (count <= 1)
        return count;

    int j = 0;
    for (int i = 1; i < count; i++) {
        if (strcmp(arr[i], arr[j]) != 0) {
            arr[++j] = arr[i];
        }
    }
    return j + 1;
}
