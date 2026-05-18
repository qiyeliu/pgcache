# pgcache

A Linux command-line tool that shows which files are currently cached in the kernel's page cache, powered by the `mincore(2)` syscall. Written in pure C with zero external dependencies.

## How It Works

pgcache uses a three-stage pipeline to identify cached files system-wide:

```
Process Discovery → File Collection → Cache Detection
  scan /proc         read maps files    mmap + mincore
  filter by RSS      extract paths      count cached pages
```

**Core algorithm** — for each file:

1. `open()` + `fstat()` to get the file size
2. `mmap(PROT_NONE, MAP_SHARED)` to map the file into virtual address space
3. `mincore()` syscall — the kernel fills a byte vector where **LSB = 1** means the page is in page cache
4. Count cached pages, compute percentage

## Features

- **System-wide scan**: `--top N` finds the top N largest cached files across all processes
- **Per-process view**: `--pid PID` shows cache status for a specific process's mapped files
- **Direct file check**: Pass file paths directly to check individual files
- **6 output formats**: ASCII table, Unicode table, plain text, CSV, JSON, histogram
- **Container support**: Automatic mount namespace switching via `setns()`
- **Zero dependencies**: Pure C11 + POSIX/Linux API, links only against glibc

## Build

```bash
git clone https://github.com/<your-username>/pgcache.git
cd pgcache
make
```

**Requirements**: GCC, Linux kernel 2.6.19+, glibc 2.19+

Install system-wide:

```bash
sudo make install    # installs to /usr/local/bin/pgcache
```

## Usage

```
pgcache [options] [file ...]
```

### System-wide: Top cached files

```bash
$ sudo pgcache --top 10
+------------------------------------------------------+----------------+-------------+----------------+-------------+---------+
| Name                                                 | Size           | Pages       | Cached Size    | Cached Pages| Percent |
|------------------------------------------------------+----------------+-------------+----------------+-------------+---------+
| /opt/google/chrome/chrome                            | 256.019M       | 65541       | 256.019M       | 65541       | 100.000 |
| /usr/lib/libreoffice/program/libmergedlo.so          | 84.431M        | 21615       | 66.744M        | 17087       | 79.052  |
| /usr/lib/x86_64-linux-gnu/libwebkit2gtk-4.0.so      | 87.038M        | 22282       | 58.972M        | 15097       | 67.754  |
| ...
+------------------------------------------------------+----------------+-------------+----------------+-------------+---------+
| Sum                                                  | 2.029G         | 531960      | 1.538G         | 403260      | 75.806  |
+------------------------------------------------------+----------------+-------------+----------------+-------------+---------+
```

> **Note**: pgcache only reports mmap'd files (shared libraries, data files). It does not count `read()/write()` file cache, tmpfs, or buffer cache. Use `free -h` for total cache usage.

### Per-process: Files mapped by a specific process

```bash
$ sudo pgcache --pid 1
$ sudo pgcache --pid $(pgrep firefox)
```

### Direct file check

```bash
$ pgcache /usr/bin/ls /usr/bin/bash /usr/bin/vim
```

### Histogram: Visual per-page cache status

```bash
$ sudo pgcache --pid 1 --histo --bname --pps
systemd                       25 █████████████████████████
libcrypto.so.3              1297 ██████████████████▇▇███████▃▁▄█████▆██▄▁▁▁▁▁▁▁▁▁▃█████▆█████████▅█████████▇▁▁▁▁███████▄
libc.so.6                    519 ▇██████████████████▇▁▃█████████████████████████████████████████▆██████████████████████▅
```

Each character represents a group of pages. `█` = fully cached, `▁` = not cached.

### JSON output

```bash
$ pgcache --json --pps /usr/bin/ls
```

```json
[{
  "filename": "/usr/bin/ls",
  "size": 142312,
  "timestamp": 1779069856,
  "mtime": 1750000000,
  "pages": 35,
  "cached": 35,
  "uncached": 0,
  "percent": 100,
  "status": [true, true, true, true, ...]
}]
```

### CSV output

```bash
$ pgcache --terse --top 5
```

```csv
name,size,timestamp,mtime,pages,cached,percent
/opt/google/chrome/chrome,268455128,1779069856,1759949937,65541,65541,100
```

## Options

| Option | Description |
|--------|-------------|
| `--top N` | Show top N cached files system-wide |
| `--pid PID` | Show cache status for a specific process |
| `--terse` | CSV output |
| `--json` | JSON output |
| `--pps` | Include per-page status in JSON output |
| `--histo` | Visual histogram using Unicode block characters |
| `--unicode` | Unicode box-drawing table |
| `--plain` | Plain text, no box characters |
| `--nohdr` | Suppress column headers |
| `--bname` | Show basename only |
| `--version` | Show version |
| `--help` | Show help |

## Project Structure

```
├── pgcache.h      — Data structures and function declarations
├── pgcache.c      — main(), argument parsing, orchestration
├── process.c      — /proc enumeration, maps parsing, mount namespace
├── mincore.c      — Core mincore logic (open/mmap/mincore)
├── format.c       — All 6 output formatters
├── test.sh        — Automated test suite (32 test cases)
└── Makefile       — Build system
```

## Testing

```bash
./test.sh ./pgcache
```

Covers: basic functionality, file mode, `--pid` mode, `--top` mode, all output formats, sorting, data integrity, edge cases, and binary analysis.

## Comparison

| Tool | Language | Feature |
|------|----------|---------|
| **pgcache** | C | mmap'd file cache, `--top` global scan, 6 output formats, zero deps |
| [hcache](https://github.com/tobert/pcstat) | Go | Same features, depends on pcstat library |
| [pcstat](https://github.com/tobert/pcstat) | C | Single file/process, no `--top` |
| [fincore](https://man7.org/linux/man-pages/man1/fincore.1.html) | C | Coreutils, no global scan |
| [vmtouch](https://hoytech.com/vmtouch/) | C | Also supports cache eviction/locking |

## Permissions

- Checking your own processes: no special permissions needed
- `--top` / `--pid` on other users' processes: requires `sudo`
- Mount namespace switching: requires `CAP_SYS_ADMIN` (auto-fallback if unavailable)

## License

MIT
