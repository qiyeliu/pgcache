#include "pgcache.h"

static int cmp_cached_desc(const void *a, const void *b) {
    const cache_entry_t *ea = a, *eb = b;
    return eb->cached - ea->cached;
}

void sort_by_cached(entry_list_t *list) {
    qsort(list->entries, list->count, sizeof(cache_entry_t), cmp_cached_desc);
}

int max_name_len(const entry_list_t *list) {
    int max = 5;
    for (int i = 0; i < list->count; i++) {
        int len = strlen(list->entries[i].name);
        if (len > max) max = len;
    }
    return max;
}

/* Helper: compute cached_size from entry */
static int64_t cached_size_of(const cache_entry_t *e) {
    return (int64_t)((double)e->size * e->percent / 100.0);
}

/* ---- ASCII table (default) ---- */
static void format_text(const entry_list_t *list) {
    int mn = max_name_len(list);
    char pad_str[4096];
    char size_buf[32], cached_size_buf[32];
    int64_t size_sum = 0, page_sum = 0, cached_page_sum = 0, cached_size_sum = 0;

    memset(pad_str, '-', mn + 2);
    pad_str[mn + 2] = '\0';
    printf("+%s+----------------+-------------+----------------+-------------+---------+\n", pad_str);

    if (!g_opts.nohdr) {
        memset(pad_str, ' ', mn - 4);
        pad_str[mn - 4] = '\0';
        printf("| Name%s | Size           | Pages       | Cached Size    | Cached Pages| Percent |\n", pad_str);
        memset(pad_str, '-', mn + 2);
        pad_str[mn + 2] = '\0';
        printf("|%s+----------------+-------------+----------------+-------------+---------|\n", pad_str);
    }

    for (int i = 0; i < list->count; i++) {
        cache_entry_t *e = &list->entries[i];
        int64_t cs = cached_size_of(e);
        memset(pad_str, ' ', mn - (int)strlen(e->name));
        pad_str[mn - (int)strlen(e->name)] = '\0';
        printf("| %s%s | %-15s| %-12d| %-15s| %-12d| %-7.3f |\n",
            e->name, pad_str,
            human_size(e->size, size_buf), e->pages,
            human_size(cs, cached_size_buf), e->cached, e->percent);
        size_sum += e->size;
        page_sum += e->pages;
        cached_page_sum += e->cached;
        cached_size_sum += cs;
    }

    memset(pad_str, '-', mn + 2);
    pad_str[mn + 2] = '\0';
    printf("|%s+----------------+-------------+----------------+-------------+---------|\n", pad_str);
    memset(pad_str, ' ', mn - 3);
    pad_str[mn - 3] = '\0';
    double total_pct = page_sum > 0 ? (double)cached_page_sum / (double)page_sum * 100.0 : 0.0;
    printf("| Sum%s | %-15s| %-12ld| %-15s| %-12ld| %-7.3f |\n",
        pad_str, human_size(size_sum, size_buf), (long)page_sum,
        human_size(cached_size_sum, cached_size_buf), (long)cached_page_sum, total_pct);
    memset(pad_str, '-', mn + 2);
    pad_str[mn + 2] = '\0';
    printf("+%s+----------------+-------------+----------------+-------------+---------+\n", pad_str);
}

/* ---- Unicode box-drawing table ---- */
static void print_unicode_hline(int mn, int *col_widths, int ncols,
                                 const char *left, const char *mid, const char *right) {
    printf("%s", left);
    for (int i = 0; i < mn + 2; i++) printf("\xe2\x94\x80");
    for (int c = 0; c < ncols; c++) {
        printf("%s", mid);
        for (int i = 0; i < col_widths[c]; i++) printf("\xe2\x94\x80");
    }
    printf("%s\n", right);
}

static void format_unicode_impl(const entry_list_t *list) {
    int mn = max_name_len(list);
    char pad_str[4096];
    char size_buf[32], cached_size_buf[32];
    int64_t size_sum = 0, page_sum = 0, cached_page_sum = 0, cached_size_sum = 0;
    int col_widths[] = {16, 13, 16, 13, 9};

    print_unicode_hline(mn, col_widths, 5,
        "\xe2\x94\x8c", "\xe2\x94\xac", "\xe2\x94\x90");

    if (!g_opts.nohdr) {
        memset(pad_str, ' ', mn - 4);
        pad_str[mn - 4] = '\0';
        printf("\xe2\x94\x82 Name%s \xe2\x94\x82 Size           "
               "\xe2\x94\x82 Pages       \xe2\x94\x82 Cached Size    "
               "\xe2\x94\x82 Cached Pages\xe2\x94\x82 Percent \xe2\x94\x82\n", pad_str);
        print_unicode_hline(mn, col_widths, 5,
            "\xe2\x94\x9c", "\xe2\x94\xbc", "\xe2\x94\xa4");
    }

    for (int i = 0; i < list->count; i++) {
        cache_entry_t *e = &list->entries[i];
        int64_t cs = cached_size_of(e);
        memset(pad_str, ' ', mn - (int)strlen(e->name));
        pad_str[mn - (int)strlen(e->name)] = '\0';
        printf("\xe2\x94\x82 %s%s \xe2\x94\x82 %-15s"
               "\xe2\x94\x82 %-12d\xe2\x94\x82 %-15s"
               "\xe2\x94\x82 %-12d\xe2\x94\x82 %-7.3f \xe2\x94\x82\n",
            e->name, pad_str,
            human_size(e->size, size_buf), e->pages,
            human_size(cs, cached_size_buf), e->cached, e->percent);
        size_sum += e->size;
        page_sum += e->pages;
        cached_page_sum += e->cached;
        cached_size_sum += cs;
    }

    print_unicode_hline(mn, col_widths, 5,
        "\xe2\x94\x9c", "\xe2\x94\xbc", "\xe2\x94\xa4");

    memset(pad_str, ' ', mn - 3);
    pad_str[mn - 3] = '\0';
    double total_pct = page_sum > 0 ? (double)cached_page_sum / (double)page_sum * 100.0 : 0.0;
    printf("\xe2\x94\x82 Sum%s \xe2\x94\x82 %-15s"
           "\xe2\x94\x82 %-12ld\xe2\x94\x82 %-15s"
           "\xe2\x94\x82 %-12ld\xe2\x94\x82 %-7.3f \xe2\x94\x82\n",
        pad_str, human_size(size_sum, size_buf), (long)page_sum,
        human_size(cached_size_sum, cached_size_buf), (long)cached_page_sum, total_pct);

    print_unicode_hline(mn, col_widths, 5,
        "\xe2\x94\x94", "\xe2\x94\xb4", "\xe2\x94\x98");
}

/* ---- Plain text (no box) ---- */
static void format_plain_impl(const entry_list_t *list) {
    int mn = max_name_len(list);
    char pad_str[4096];
    char size_buf[32], cached_size_buf[32];
    int64_t size_sum = 0, page_sum = 0, cached_page_sum = 0, cached_size_sum = 0;

    if (!g_opts.nohdr) {
        memset(pad_str, ' ', mn - 4);
        pad_str[mn - 4] = '\0';
        printf("Name%s  Size            Pages        Cached Size     Cached Pages Percent\n", pad_str);
    }

    for (int i = 0; i < list->count; i++) {
        cache_entry_t *e = &list->entries[i];
        int64_t cs = cached_size_of(e);
        memset(pad_str, ' ', mn - (int)strlen(e->name));
        pad_str[mn - (int)strlen(e->name)] = '\0';
        printf("%s%s  %-15s %-12d %-15s %-12d %-7.3f\n",
            e->name, pad_str,
            human_size(e->size, size_buf), e->pages,
            human_size(cs, cached_size_buf), e->cached, e->percent);
        size_sum += e->size;
        page_sum += e->pages;
        cached_page_sum += e->cached;
        cached_size_sum += cs;
    }

    memset(pad_str, ' ', mn - 3);
    pad_str[mn - 3] = '\0';
    double total_pct = page_sum > 0 ? (double)cached_page_sum / (double)page_sum * 100.0 : 0.0;
    printf("%s%s  %-15s %-12ld %-15s %-12ld %-7.3f\n",
        "Sum", pad_str, human_size(size_sum, size_buf), (long)page_sum,
        human_size(cached_size_sum, cached_size_buf), (long)cached_page_sum, total_pct);
}

/* ---- CSV (terse) ---- */
static void format_terse_impl(const entry_list_t *list) {
    if (!g_opts.nohdr)
        printf("name,size,timestamp,mtime,pages,cached,percent\n");

    for (int i = 0; i < list->count; i++) {
        cache_entry_t *e = &list->entries[i];
        printf("%s,%ld,%ld,%ld,%d,%d,%g\n",
            e->name, (long)e->size, (long)e->timestamp, (long)e->mtime,
            e->pages, e->cached, e->percent);
    }
}

/* ---- JSON ---- */
static void json_escape(FILE *f, const char *s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\')
            fputc('\\', f);
        fputc(*s, f);
    }
}

static void format_json_impl(const entry_list_t *list) {
    printf("[");
    for (int i = 0; i < list->count; i++) {
        cache_entry_t *e = &list->entries[i];
        if (i > 0) printf(",");
        printf("{\"filename\":\"");
        json_escape(stdout, e->name);
        printf("\",\"size\":%ld,\"timestamp\":%ld,\"mtime\":%ld,"
               "\"pages\":%d,\"cached\":%d,\"uncached\":%d,\"percent\":%g",
            (long)e->size, (long)e->timestamp, (long)e->mtime,
            e->pages, e->cached, e->uncached, e->percent);

        if (g_opts.pps && e->ppstat) {
            printf(",\"status\":[");
            for (int j = 0; j < e->pages; j++) {
                if (j > 0) printf(",");
                printf("%s", e->ppstat[j] ? "true" : "false");
            }
            printf("]");
        } else {
            printf(",\"status\":[]");
        }
        printf("}");
    }
    printf("]\n");
}

/* ---- Histogram ---- */
static int get_term_cols(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        return 80;
    return ws.ws_col;
}

static void format_histogram_impl(const entry_list_t *list) {
    int mn = max_name_len(list);
    int cols = get_term_cols();
    int buckets = (cols - mn) / 2 - 10;
    char pad_str[4096];

    const char *blocks[] = {
        "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
        "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88",
    };

    for (int i = 0; i < list->count; i++) {
        cache_entry_t *e = &list->entries[i];
        memset(pad_str, ' ', mn - (int)strlen(e->name));
        pad_str[mn - (int)strlen(e->name)] = '\0';
        printf("%s%s %8d ", e->name, pad_str, e->pages);

        if (!e->ppstat) {
            printf("\n");
            continue;
        }

        if (buckets <= 0) buckets = 1;

        if (buckets >= e->pages) {
            for (int j = 0; j < e->pages; j++)
                printf("%s", e->ppstat[j] ? blocks[7] : blocks[0]);
        } else {
            int bsz = e->pages / buckets;
            if (bsz <= 0) bsz = 1;
            double total = 0.0;
            for (int j = 0; j < e->pages; j++) {
                if (e->ppstat[j])
                    total += 1.0;

                if ((j + 1) % bsz == 0 || j == e->pages - 1) {
                    double avg = total / (double)bsz;
                    int idx;
                    if (total == 0) idx = 0;
                    else if (avg < 0.16) idx = 1;
                    else if (avg < 0.33) idx = 2;
                    else if (avg < 0.50) idx = 3;
                    else if (avg < 0.66) idx = 4;
                    else if (avg < 0.83) idx = 5;
                    else if (avg < 1.00) idx = 6;
                    else idx = 7;
                    printf("%s", blocks[idx]);
                    total = 0.0;
                }
            }
        }
        printf("\n");
    }
}

/* ---- Dispatcher ---- */
void format_stats(const entry_list_t *list) {
    if (g_opts.json)
        format_json_impl(list);
    else if (g_opts.terse)
        format_terse_impl(list);
    else if (g_opts.histo)
        format_histogram_impl(list);
    else if (g_opts.unicode)
        format_unicode_impl(list);
    else if (g_opts.plain)
        format_plain_impl(list);
    else
        format_text(list);
}
