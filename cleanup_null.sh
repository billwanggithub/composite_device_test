#!/bin/bash
# 自動刪除 null/nul 檔案的腳本
# 這些檔案通常是在 Git Bash 中使用 Windows 命令 ">nul" 時意外產生的

echo "========================================"
echo "清理 null/nul 檔案"
echo "========================================"

# 檢查並刪除 null 檔案
if [ -f "null" ]; then
    rm "null"
    echo "✓ 已刪除 null 檔案"
else
    echo "○ 找不到 null 檔案"
fi

# 檢查並刪除 nul 檔案
if [ -f "nul" ]; then
    rm "nul"
    echo "✓ 已刪除 nul 檔案"
else
    echo "○ 找不到 nul 檔案"
fi

echo "========================================"
echo "清理完成！"
echo ""
echo "提醒："
echo "  - 避免在 Git Bash 中使用 'ping -n 5 127.0.0.1 >nul'"
echo "  - 建議使用 'timeout /t 3 >nul' (Windows)"
echo "  - 或使用 'sleep 3' (Linux/Git Bash)"
echo "========================================"
