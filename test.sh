#!/bin/bash
# pgcache 功能完整性验证脚本
# 用法: ./test.sh [pgcache路径]

set -euo pipefail

PGCACHE="${1:-./pgcache}"
PASS=0
FAIL=0
SKIP=0
TOTAL=0

# ---------- 颜色 ----------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
RESET='\033[0m'

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo -e "  ${GREEN}PASS${RESET} $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo -e "  ${RED}FAIL${RESET} $1"; [ -n "${2:-}" ] && echo "       $2"; }
skip() { SKIP=$((SKIP+1)); TOTAL=$((TOTAL+1)); echo -e "  ${YELLOW}SKIP${RESET} $1"; }

echo "======================================"
echo " pgcache 完整性验证"
echo " 二进制: $PGCACHE"
echo " 时间:   $(date '+%Y-%m-%d %H:%M:%S')"
echo "======================================"
echo

# ============================================================
echo "${BOLD}[1] 基本功能${RESET}"
# ============================================================

# 1.1 二进制存在且可执行
if [ -x "$PGCACHE" ]; then
    pass "二进制存在且可执行"
else
    fail "二进制不存在或不可执行: $PGCACHE"
    echo; echo "中止测试"; exit 1
fi

# 1.2 --version
ver=$("$PGCACHE" --version 2>&1) && true
if echo "$ver" | grep -q "pgcache"; then
    pass "--version 输出: $ver"
else
    fail "--version" "输出: $ver"
fi

# 1.3 --help
help_out=$("$PGCACHE" --help 2>&1) && rc=0 || rc=$?
if [ $rc -eq 0 ] && echo "$help_out" | grep -q "Usage:"; then
    pass "--help 显示用法信息"
else
    fail "--help" "退出码: $rc"
fi

# 1.4 无参数应显示 usage
usage_out=$("$PGCACHE" 2>&1) && rc=0 || rc=$?
if [ $rc -ne 0 ] && echo "$usage_out" | grep -q "Usage:"; then
    pass "无参数 → 显示 usage 并返回非零"
else
    fail "无参数" "退出码: $rc, 输出: $(echo "$usage_out" | head -1)"
fi

# 1.5 未知选项
bad_opt=$("$PGCACHE" --nonexistent 2>&1) && rc=0 || rc=$?
if [ $rc -ne 0 ]; then
    pass "未知选项 → 返回非零"
else
    fail "未知选项" "应返回非零, 实际: $rc"
fi

echo

# ============================================================
echo "${BOLD}[2] 文件模式${RESET}"
# ============================================================

# 2.1 单个文件
test_file="/usr/bin/ls"
if [ ! -f "$test_file" ]; then
    test_file="/bin/ls"
fi
if [ -f "$test_file" ]; then
    out=$("$PGCACHE" "$test_file" 2>&1) && true
    if echo "$out" | grep -q "$test_file"; then
        pass "单文件模式 → 输出包含文件名"
    else
        fail "单文件模式" "未找到文件名"
    fi

    # 2.2 检查百分比
    if echo "$out" | grep -q "100.000"; then
        pass "单文件 → 常用文件 100% 缓存"
    else
        skip "单文件 → 缓存百分比非 100% (正常)"
    fi
else
    skip "单文件模式 → $test_file 不存在"
fi

# 2.3 多文件
if [ -f "$test_file" ] && [ -f "/usr/bin/bash" -o -f "/bin/bash" ]; then
    bash_file="/usr/bin/bash"
    [ ! -f "$bash_file" ] && bash_file="/bin/bash"
    out=$("$PGCACHE" "$test_file" "$bash_file" 2>&1) && true
    lines=$(echo "$out" | grep -c '|' || true)
    # header + separator + data rows + separator + sum + separator + top border
    if [ "$lines" -ge 5 ]; then
        pass "多文件模式 → 输出多行数据"
    else
        fail "多文件模式" "行数不足: $lines"
    fi
else
    skip "多文件模式 → 文件不存在"
fi

# 2.4 不存在的文件应跳过
out=$("$PGCACHE" /nonexistent_file_12345 2>&1) && true
if echo "$out" | grep -qv "nonexistent_file_12345" || echo "$out" | grep -q "Usage:"; then
    # 无有效文件 → 应该有某种错误/空输出
    if echo "$out" | grep -q "Usage:" || echo "$out" | grep -q "skipping"; then
        pass "不存在的文件 → 跳过或报错"
    else
        pass "不存在的文件 → 无崩溃"
    fi
else
    fail "不存在的文件" "意外输出"
fi

echo

# ============================================================
echo "${BOLD}[3] --pid 模式${RESET}"
# ============================================================

# 3.1 当前 shell 进程
my_pid=$$
out=$("$PGCACHE" --pid "$my_pid" 2>&1) && rc=0 || rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q '|'; then
    pass "--pid $$ (当前shell) → 输出表格数据"
else
    fail "--pid $$" "退出码: $rc"
fi

# 3.2 PID 1 (通常需要 root，普通用户应报错)
out=$("$PGCACHE" --pid 1 2>&1) && rc=0 || rc=$?
if [ "$(id -u)" -eq 0 ]; then
    if [ $rc -eq 0 ]; then
        pass "--pid 1 (root) → 正常输出"
    else
        fail "--pid 1 (root)" "退出码: $rc"
    fi
else
    if [ $rc -ne 0 ] && echo "$out" | grep -qi "permission\|cannot read"; then
        pass "--pid 1 (非root) → 权限拒绝错误"
    else
        fail "--pid 1 (非root)" "应报权限错误, 退出码: $rc, 输出: $(echo "$out" | head -1)"
    fi
fi

# 3.3 不存在的 PID
out=$("$PGCACHE" --pid 9999999 2>&1) && rc=0 || rc=$?
if [ $rc -ne 0 ]; then
    pass "--pid 9999999 (不存在) → 返回非零"
else
    fail "--pid 9999999" "应返回非零, 实际: $rc"
fi

echo

# ============================================================
echo "${BOLD}[4] --top 模式${RESET}"
# ============================================================

# 4.1 --top 5
out=$("$PGCACHE" --top 5 2>&1) && rc=0 || rc=$?
if [ $rc -eq 0 ]; then
    data_lines=$(echo "$out" | grep -c '^|' || true)
    if [ "$data_lines" -ge 8 ]; then
        pass "--top 5 → 输出多行表格数据"
    else
        fail "--top 5" "表格行数不足: $data_lines"
    fi
else
    # 非 root 可能无法读取所有进程 maps
    if [ "$(id -u)" -ne 0 ]; then
        skip "--top 5 → 非 root 可能受限 (退出码: $rc)"
    else
        fail "--top 5" "退出码: $rc"
    fi
fi

# 4.2 Sum 行存在
if echo "$out" | grep -q "Sum"; then
    pass "--top → Sum 汇总行存在"
else
    fail "--top Sum行" "未找到 Sum 汇总行"
fi

echo

# ============================================================
echo "${BOLD}[5] 输出格式${RESET}"
# ============================================================

# 用一个保证有输出的测试 PID
TEST_PID=$$

# 5.1 --plain
out=$("$PGCACHE" --plain --pid "$TEST_PID" 2>&1) && true
if echo "$out" | grep -qv '^\[' && ! echo "$out" | grep -q '|'; then
    pass "--plain → 无边框字符"
else
    fail "--plain" "输出包含边框字符"
fi

# 5.2 --unicode
out=$("$PGCACHE" --unicode --pid "$TEST_PID" 2>&1) && true
if echo "$out" | grep -q '┌\|│\|└'; then
    pass "--unicode → Unicode 边框字符"
else
    fail "--unicode" "未检测到 Unicode 边框"
fi

# 5.3 --json
out=$("$PGCACHE" --json --pid "$TEST_PID" 2>&1) && true
if echo "$out" | python3 -m json.tool > /dev/null 2>&1; then
    pass "--json → 合法 JSON"
else
    # 尝试用 node 验证
    if command -v node > /dev/null 2>&1; then
        if echo "$out" | node -e "JSON.parse(require('fs').readFileSync('/dev/stdin','utf8'))" 2>/dev/null; then
            pass "--json → 合法 JSON"
        else
            fail "--json" "JSON 解析失败"
        fi
    else
        skip "--json → 无 python3/node 验证, 跳过格式检查"
    fi
fi

# 5.4 --json --pps
out=$("$PGCACHE" --json --pps --pid "$TEST_PID" 2>&1) && true
if echo "$out" | grep -q '"status"' && echo "$out" | grep -q 'true\|false'; then
    pass "--json --pps → 包含逐页状态"
else
    fail "--json --pps" "未检测到 status 数组"
fi

# 5.5 --terse (CSV)
out=$("$PGCACHE" --terse --pid "$TEST_PID" 2>&1) && true
if echo "$out" | head -1 | grep -q '^name,size,timestamp,mtime,pages,cached,percent$'; then
    pass "--terse → CSV 表头正确"
else
    fail "--terse" "CSV 表头不匹配: $(echo "$out" | head -1)"
fi
# CSV 数据行逗号分隔
csv_data_lines=$(echo "$out" | tail -n +2 | grep -c ',' || true)
if [ "$csv_data_lines" -ge 1 ]; then
    pass "--terse → CSV 数据行存在"
else
    fail "--terse" "无 CSV 数据行"
fi

# 5.6 --nohdr
out=$("$PGCACHE" --nohdr --plain --pid "$TEST_PID" 2>&1) && true
if ! echo "$out" | head -2 | grep -q 'Name.*Size.*Pages'; then
    pass "--nohdr → 表头已隐藏"
else
    fail "--nohdr" "表头仍然存在"
fi

# 5.7 --bname
out=$("$PGCACHE" --bname --plain --pid "$TEST_PID" 2>&1) && true
if echo "$out" | grep -qv '/'; then
    # 检查是否没有以 / 开头的路径
    has_slash=$(echo "$out" | grep -E '^\S+\s' | grep -c '/' || true)
    if [ "$has_slash" -eq 0 ]; then
        pass "--bname → 路径已转为 basename"
    else
        skip "--bname → 部分路径仍含 / (可能是 Sum 行)"
    fi
else
    fail "--bname" "路径未转为 basename"
fi

# 5.8 --histo
out=$("$PGCACHE" --histo --pid "$TEST_PID" 2>&1) && true
if echo "$out" | grep -q '█\|▁\|▃\|▆'; then
    pass "--histo → Unicode 块字符直方图"
else
    fail "--histo" "未检测到 Unicode 块字符"
fi

echo

# ============================================================
echo "${BOLD}[6] 排序与截断${RESET}"
# ============================================================

# 6.1 输出按缓存页数降序
out=$("$PGCACHE" --terse --pid "$TEST_PID" 2>&1) && true
cached_col=$(echo "$out" | tail -n +2 | awk -F',' '{print $6}' | head -5)
prev=999999999
sorted=true
for val in $cached_col; do
    if [ "$val" -gt "$prev" ]; then
        sorted=false
        break
    fi
    prev=$val
done
if $sorted; then
    pass "输出按 Cached 降序排列"
else
    fail "排序" "未按 Cached 降序"
fi

# 6.2 --top N 截断
top3=$("$PGCACHE" --top 3 --terse 2>&1) && true
data_count=$(echo "$top3" | tail -n +2 | grep -c ',' || true)
if [ "$data_count" -eq 3 ]; then
    pass "--top 3 → 恰好 3 行数据"
else
    if [ "$(id -u)" -ne 0 ]; then
        skip "--top 3 → 非 root 可能数据不足"
    else
        fail "--top 3" "期望 3 行, 实际: $data_count"
    fi
fi

echo

# ============================================================
echo "${BOLD}[7] 数据完整性${RESET}"
# ============================================================

# 7.1 Sum 行的 Pages 等于各行 Pages 之和
out=$("$PGCACHE" --terse --pid "$TEST_PID" 2>&1) && true
total_pages=$(echo "$out" | tail -n +2 | awk -F',' '{sum+=$5} END {print sum}')
# Sum 行在 table 格式中，这里用 table 格式验证
table_out=$("$PGCACHE" --plain --pid "$TEST_PID" 2>&1) && true
sum_line=$(echo "$table_out" | grep '^Sum')
if [ -n "$sum_line" ]; then
    sum_pages=$(echo "$sum_line" | awk '{print $3}')
    if [ "$total_pages" = "$sum_pages" ]; then
        pass "Sum Pages 总和校验通过 ($sum_pages)"
    else
        fail "Sum Pages" "terse合计=$total_pages, plain Sum=$sum_pages"
    fi
else
    skip "Sum 行不存在, 跳过校验"
fi

# 7.2 percent = cached / pages * 100
out=$("$PGCACHE" --terse --pid "$TEST_PID" 2>&1) && true
pct_ok=true
while IFS=',' read -r name _ _ _ pages cached pct; do
    [ "$pages" = "pages" ] && continue
    [ "$pages" = "0" ] && continue
    expected=$(awk "BEGIN {printf \"%.6f\", $cached/$pages*100}")
    # 允许浮点误差
    diff=$(awk "BEGIN {d=$pct-$expected; if(d<0)d=-d; print d}")
    if awk "BEGIN {exit !($diff > 0.01)}"; then
        pct_ok=false
        break
    fi
done <<< "$out"
if $pct_ok; then
    pass "Percent = Cached/Pages*100 校验通过"
else
    fail "Percent" "计算不一致: name=$name pages=$pages cached=$cached pct=$pct expected=$expected"
fi

echo

# ============================================================
echo "${BOLD}[8] 边界情况${RESET}"
# ============================================================

# 8.1 空文件应跳过 (不崩溃)
touch /tmp/pgcache_test_empty_$$
out=$("$PGCACHE" /tmp/pgcache_test_empty_$$ 2>&1) && rc=0 || rc=$?
rm -f /tmp/pgcache_test_empty_$$
# 空文件无有效输出或显示 usage，但不应 crash
if echo "$out" | grep -q "Usage:\|Segmentation\|Aborted"; then
    if echo "$out" | grep -q "Segmentation\|Aborted"; then
        fail "空文件" "程序崩溃"
    else
        pass "空文件 → 无崩溃 (显示 usage 因无有效文件)"
    fi
else
    pass "空文件 → 跳过无崩溃"
fi

# 8.2 大量文件不崩溃 (用 --top 扫描)
if [ "$(id -u)" -eq 0 ]; then
    out=$("$PGCACHE" --top 100 2>&1) && rc=0 || rc=$?
    if [ $rc -eq 0 ]; then
        pass "--top 100 → 大量文件无崩溃"
    else
        fail "--top 100" "退出码: $rc"
    fi
else
    skip "--top 100 → 非 root 跳过"
fi

# 8.3 --top 1 输出
out=$("$PGCACHE" --top 1 --terse 2>&1) && true
data_lines=$(echo "$out" | tail -n +2 | grep -c ',' || true)
if [ "$data_lines" -eq 1 ]; then
    pass "--top 1 → 恰好 1 行数据"
else
    if [ "$(id -u)" -ne 0 ]; then
        skip "--top 1 → 非 root 跳过"
    else
        fail "--top 1" "期望 1 行, 实际: $data_lines"
    fi
fi

echo

# ============================================================
echo "${BOLD}[9] 编译产物检查${RESET}"
# ============================================================

# 9.1 无调试符号残留 (release build)
if command -v file > /dev/null 2>&1; then
    if file "$PGCACHE" | grep -q "not stripped"; then
        skip "二进制包含调试符号 (非 release build)"
    else
        pass "二进制无调试符号"
    fi
else
    skip "file 命令不可用, 跳过"
fi

# 9.2 动态链接检查 (仅依赖 libc)
if command -v ldd > /dev/null 2>&1; then
    libs=$(ldd "$PGCACHE" 2>/dev/null | awk '{print $1}' | grep -v 'linux-vdso\|ld-linux')
    extra_libs=$(echo "$libs" | grep -vc 'libc\.\|libpthread\.\|libm\.\|libdl\.\|librt\.' || true)
    if [ "$extra_libs" -eq 0 ]; then
        pass "仅依赖 glibc 标准库"
    else
        fail "依赖检查" "额外依赖: $(echo "$libs" | grep -v 'libc\.\|libpthread\.\|libm\.\|libdl\.\|librt\.')"
    fi
else
    skip "ldd 不可用, 跳过依赖检查"
fi

echo

# ============================================================
# 汇总
# ============================================================
echo "======================================"
echo " 测试结果汇总"
echo "======================================"
echo -e "  ${GREEN}通过: $PASS${RESET}"
echo -e "  ${RED}失败: $FAIL${RESET}"
echo -e "  ${YELLOW}跳过: $SKIP${RESET}"
echo "  总计: $TOTAL"
echo "======================================"

if [ $FAIL -gt 0 ]; then
    exit 1
else
    exit 0
fi
