#include "pgcache.h"
#include <getopt.h>

opts_t g_opts = {0};

void list_init(entry_list_t *list) {
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}

void list_push(entry_list_t *list, const cache_entry_t *entry) {
    if (list->count == list->capacity) {
        int new_cap = list->capacity == 0 ? 64 : list->capacity * 2;
        cache_entry_t *tmp = realloc(list->entries, new_cap * sizeof(cache_entry_t));
        if (!tmp) { perror("realloc"); exit(1); }
        list->entries = tmp;
        list->capacity = new_cap;
    }
    list->entries[list->count++] = *entry; /* struct copy */
}

void list_free(entry_list_t *list) {
    for (int i = 0; i < list->count; i++) {
        free(list->entries[i].name);
        free(list->entries[i].ppstat);
    }
    free(list->entries);
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}

static int cmp_str_ptr(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static void get_stats_from_files(char **files, int nfiles, entry_list_t *list) {
    list_init(list);
    for (int i = 0; i < nfiles; i++) {
        cache_entry_t entry;
        if (get_file_cache_status(files[i], &entry, g_opts.histo || g_opts.pps) == 0) {
            if (g_opts.bname) {
                char *slash = strrchr(entry.name, '/');
                if (slash) {
                    char *base = strdup(slash + 1);
                    free(entry.name);
                    entry.name = base;
                }
            }
            list_push(list, &entry);
        }
    }
}

static void do_top(int top_n) {
    int npids, nfiles = 0, files_cap = 0;
    char **all_files = NULL;
    entry_list_t list;

    int *pids = discover_pids(&npids);
    if (npids <= 0) {
        fprintf(stderr, "Cannot find any process.\n");
        free(pids);
        exit(1);
    }

    for (int i = 0; i < npids; i++) {
        if (get_pid_rss(pids[i]) == 0)
            continue;

        switch_mount_ns(pids[i]);

        int nmaps;
        char **maps = get_pid_maps(pids[i], &nmaps);
        if (!maps)
            continue;

        /* grow all_files to hold new entries */
        if (nfiles + nmaps > files_cap) {
            files_cap = (nfiles + nmaps) * 2;
            all_files = realloc(all_files, files_cap * sizeof(char *));
            if (!all_files) { perror("realloc"); exit(1); }
        }

        for (int j = 0; j < nmaps; j++) {
            all_files[nfiles++] = maps[j];
        }
        free(maps); /* free the array, not the strings (they're in all_files now) */
    }

    free(pids);

    if (nfiles == 0) {
        fprintf(stderr, "No mapped files found.\n");
        free(all_files);
        exit(1);
    }

    /* sort and deduplicate */
    qsort(all_files, nfiles, sizeof(char *), cmp_str_ptr);
    nfiles = dedup_strings(all_files, nfiles);

    /* get cache status for each file */
    get_stats_from_files(all_files, nfiles, &list);

    /* free file path strings */
    for (int i = 0; i < nfiles; i++)
        free(all_files[i]);
    free(all_files);

    /* sort by cached descending and truncate to top N */
    sort_by_cached(&list);
    if (top_n > 0 && top_n < list.count)
        list.count = top_n;

    format_stats(&list);
    list_free(&list);
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options] [file ...]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --pid PID     show all open maps for the given pid\n");
    fprintf(stderr, "  --top N       show top N cached files\n");
    fprintf(stderr, "  --terse       show terse output\n");
    fprintf(stderr, "  --nohdr       omit the header\n");
    fprintf(stderr, "  --json        return data in JSON format\n");
    fprintf(stderr, "  --unicode     return data with unicode box characters\n");
    fprintf(stderr, "  --plain       return data with no box characters\n");
    fprintf(stderr, "  --pps         include per-page status in JSON output\n");
    fprintf(stderr, "  --histo       print a simple histogram\n");
    fprintf(stderr, "  --bname       convert paths to basename\n");
    fprintf(stderr, "  --help        show this help\n");
    fprintf(stderr, "  --version     show version\n");
}

static struct option long_options[] = {
    {"pid",     required_argument, 0, 'p'},
    {"top",     required_argument, 0, 't'},
    {"terse",   no_argument,       0, 'T'},
    {"nohdr",   no_argument,       0, 'n'},
    {"json",    no_argument,       0, 'j'},
    {"unicode", no_argument,       0, 'u'},
    {"plain",   no_argument,       0, 'P'},
    {"pps",     no_argument,       0, 's'},
    {"histo",   no_argument,       0, 'H'},
    {"bname",   no_argument,       0, 'b'},
    {"help",    no_argument,       0, 'h'},
    {"version", no_argument,       0, 'v'},
    {0, 0, 0, 0}
};

int main(int argc, char **argv) {
    int opt;
    entry_list_t list;

    while ((opt = getopt_long(argc, argv, "p:t:TnjPsHbhv", long_options, NULL)) != -1) {
        switch (opt) {
        case 'p': g_opts.pid = atoi(optarg); break;
        case 't': g_opts.top = atoi(optarg); break;
        case 'T': g_opts.terse = 1; break;
        case 'n': g_opts.nohdr = 1; break;
        case 'j': g_opts.json = 1; break;
        case 'u': g_opts.unicode = 1; break;
        case 'P': g_opts.plain = 1; break;
        case 's': g_opts.pps = 1; break;
        case 'H': g_opts.histo = 1; break;
        case 'b': g_opts.bname = 1; break;
        case 'v':
            printf("pgcache %s\n", PGCACHE_VERSION);
            return 0;
        case 'h':
        default:
            usage(argv[0]);
            return (opt == 'h') ? 0 : 1;
        }
    }

    if (g_opts.top > 0) {
        do_top(g_opts.top);
        return 0;
    }

    /* Collect files from --pid and/or positional args */
    char **files = NULL;
    int nfiles = 0, files_cap = 0;
    int nfiles_from_maps = 0;
    int pid_specified = (g_opts.pid > 0);

    if (g_opts.pid > 0) {
        switch_mount_ns(g_opts.pid);
        int nmaps;
        char **maps = get_pid_maps(g_opts.pid, &nmaps);
        if (!maps) {
            fprintf(stderr, "cannot read maps for pid %d: %s\n",
                    g_opts.pid, strerror(errno));
            return 1;
        }
        if (nmaps > 0) {
            files_cap = nmaps;
            files = malloc(files_cap * sizeof(char *));
            if (!files) { perror("malloc"); exit(1); }
            for (int i = 0; i < nmaps; i++)
                files[nfiles++] = maps[i];
            nfiles_from_maps = nfiles;
        }
        free(maps);
    }

    /* Add positional arguments as filenames */
    for (int i = optind; i < argc; i++) {
        if (nfiles >= files_cap) {
            files_cap = files_cap == 0 ? 32 : files_cap * 2;
            files = realloc(files, files_cap * sizeof(char *));
            if (!files) { perror("realloc"); exit(1); }
        }
        files[nfiles++] = argv[i];
    }

    if (nfiles == 0) {
        if (pid_specified)
            fprintf(stderr, "pid %d has no mapped files\n", g_opts.pid);
        else
            usage(argv[0]);
        free(files);
        return 1;
    }

    get_stats_from_files(files, nfiles, &list);

    /* free strdup'd file paths from get_pid_maps (not argv strings) */
    for (int i = 0; i < nfiles_from_maps; i++)
        free(files[i]);
    free(files);

    sort_by_cached(&list);
    format_stats(&list);
    list_free(&list);

    return 0;
}
