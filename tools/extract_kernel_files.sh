#!/bin/bash

# 脚本功能：提取内核编译过程中用到的源码和头文件
# 1. 找到所有的*.o.cmd文件
# 2. 提取source_*后面的源文件
# 3. 提取deps_*后面依赖的文件，相对路径转换为绝对路径
# 4. 过滤不需要的文件（.mod.c, /usr/include/*）
# 5. 验证文件是否存在，写入filelist.txt
# 6. 在指定目录，建立软链接

if [ $# -lt 3 ]; then
    echo "用法: $0 <输出目录> <内核编译目录> <内核源码目录>"
    echo "示例: $0 /path/to/output/dir /path/to/build/dir /path/to/kernel/source"
    exit 1
fi

LN_DIR=$1
BUILD_DIR=$2
SRC_DIR=$3

# 去掉最后的斜杠
LN_DIR="${LN_DIR%/}"
BUILD_DIR="${BUILD_DIR%/}"
SRC_DIR="${SRC_DIR%/}"

# 获取当前工作目录的绝对路径
WORK_DIR=$(pwd)
OUTPUT_FILE="filelist.txt"

# 临时文件用于存储所有提取的文件路径（去重用）
TEMP_FILE=$(mktemp)

echo "开始扫描 *.o.cmd 文件..."

# 统计文件数量
total_files=$(find "$WORK_DIR" -name "*.o.cmd"  -o -name "*.dtb.cmd" -o -name "*.lds.cmd" -type f | wc -l)
echo "找到 $total_files 个 .o.cmd 文件"

processed=0

# 查找所有的 .o.cmd 文件并处理
find "$WORK_DIR" -name "*.o.cmd"  -o -name "*.dtb.cmd" -o -name "*.lds.cmd" -type f | while read -r cmd_file; do
    processed=$((processed + 1))
    if [ $((processed % 100)) -eq 0 ]; then
        echo "已处理 $processed / $total_files 个文件..."
    fi
    
    # 提取 source_* 后面的源文件
    grep "^source_" "$cmd_file" 2>/dev/null | sed 's/^source_[^ ]* := //' | while read -r src_file; do
        # 如果不是绝对路径，转换为绝对路径
        if [[ "$src_file" != /* ]]; then
            src_file="$WORK_DIR/$src_file"
        fi
        
        # 过滤条件：
        # 1. 跳过 .mod.c 文件
        # 2. 跳过 /usr/include 开头的文件
        # 3. 跳过 /usr/lib 开头的文件
        if [[ "$src_file" == *.mod.c ]]; then
            continue
        fi
        if [[ "$src_file" == /usr/include/* ]] || [[ "$src_file" == /usr/lib/* ]]; then
            continue
        fi
        
        # 验证文件是否存在
        if [ -f "$src_file" ]; then
            echo "$src_file" >> "$TEMP_FILE"
        fi
    done
    
    # 提取 deps_* 后面的依赖文件
    awk '
        /^deps_/ {
            in_deps = 1
            next
        }
        in_deps {
            # 如果遇到空行或新的section，停止
            if (/^$/ || /^[a-zA-Z_]/) {
                in_deps = 0
                next
            }
            # 移除行尾的反斜杠和行首空白
            gsub(/\\$/, "")
            gsub(/^[ \t]+/, "")
            
            # 如果包含 $(wildcard ...)，提取括号内的文件路径
            if ($0 ~ /\$\(wildcard/) {
                # 提取 $(wildcard 和 ) 之间的内容
                match($0, /\$\(wildcard[ \t]+([^)]+)\)/, arr)
                if (arr[1] != "") {
                    print arr[1]
                }
            } else {
                # 普通文件路径直接输出
                print $0
            }
        }
    ' "$cmd_file" 2>/dev/null | while read -r dep_file; do
        # 跳过空行
        [ -z "$dep_file" ] && continue
        
        # 如果不是绝对路径，转换为绝对路径
        if [[ "$dep_file" != /* ]]; then
            dep_file="$WORK_DIR/$dep_file"
        fi
        
        # 过滤条件：
        # 1. 跳过 .mod.c 文件
        # 2. 跳过 /usr/include 开头的文件
        # 3. 跳过 /usr/lib 开头的文件
        if [[ "$dep_file" == *.mod.c ]]; then
            continue
        fi
        if [[ "$dep_file" == /usr/include/* ]] || [[ "$dep_file" == /usr/lib/* ]]; then
            continue
        fi
        # 4. 跳过编译目录下的include/config/开头的文件
        if [[ "$dep_file" == $BUILD_DIR/include/config/* ]]; then
            continue
        fi
        
        # 验证文件是否存在
        if [ -f "$dep_file" ]; then
            echo "$dep_file" >> "$TEMP_FILE"
        fi
    done
done

# 单独处理 .missing-syscalls.d
MISSING_SYSCALLS_FILE="$WORK_DIR/.missing-syscalls.d"
if [ -f "$MISSING_SYSCALLS_FILE" ]; then
    echo "处理 .missing-syscalls.d 文件..."
    cat "$MISSING_SYSCALLS_FILE"  |  tr ' \\' '\n' | sed '/^$/d' | while read -r miss_file; do
        # 如果不是绝对路径，转换为绝对路径
        if [[ "$miss_file" != /* ]]; then
            miss_file="$WORK_DIR/$miss_file"
        fi
        
        # 过滤条件：
        # 1. 跳过 .mod.c 文件
        # 2. 跳过 /usr/include 开头的文件
        # 3. 跳过 /usr/lib 开头的文件
        if [[ "$miss_file" == *.mod.c ]]; then
            continue
        fi
        if [[ "$miss_file" == /usr/include/* ]] || [[ "$miss_file" == /usr/lib/* ]]; then
            continue
        fi

        
        # 验证文件是否存在
        if [ -f "$miss_file" ]; then
            echo "$miss_file" >> "$TEMP_FILE"
        fi
    done
fi

echo "正在去重并排序..."

# 去重、排序并写入最终文件
sort -u "$TEMP_FILE" > "$OUTPUT_FILE"

# 清理临时文件
rm -f "$TEMP_FILE"

# 逐个查找文件，如果是编译目录下的文件，但是在源码目录中存在同名文件，则使用源码目录中的文件
echo "正在验证文件路径，优先使用源码目录中的文件..."
final_temp_file=$(mktemp)
while read -r file_path; do
    if [[ "$file_path" == $BUILD_DIR/* ]]; then
        relative_path="${file_path#$BUILD_DIR/}"
        src_file="$SRC_DIR/$relative_path"
        if [ -f "$src_file" ]; then
            echo "$src_file" >> "$final_temp_file"
        else
            echo "$file_path" >> "$final_temp_file"
        fi
    else
        echo "$file_path" >> "$final_temp_file"
    fi
done < "$OUTPUT_FILE"

mv "$final_temp_file" "$OUTPUT_FILE"
# 统计结果
file_count=$(wc -l < "$OUTPUT_FILE")
echo "完成！共提取 $file_count 个文件，结果已保存到 $OUTPUT_FILE"


# 创建软链接目录结构
echo ""
echo "开始创建软链接目录结构..."
echo "目标目录: $LN_DIR"

rm -rf "$LN_DIR"

# 创建根目录
mkdir -p "$LN_DIR"

# 读取文件列表并创建软链接
link_count=0
while read -r file_path; do
    # 如果是源码目录下的文件，保持相对路径
    if [[ "$file_path" == $SRC_DIR/* ]]; then
        relative_path="${file_path#$SRC_DIR}"
    else 
        if [[ "$file_path" == $BUILD_DIR/* ]]; then
            relative_path="${file_path#$BUILD_DIR}"
        else
            # 否则，使用文件的完整路径作为相对路径，去掉前面的斜杠
            relative_path="${file_path#/}"
        fi
    fi
    link_path="$LN_DIR/$relative_path"
    link_dir=$(dirname "$link_path")
    
    # 创建目录
    mkdir -p "$link_dir"
    
    # 创建软链接
    ln -sf "$file_path" "$link_path"
    
    link_count=$((link_count + 1))
    if [ $((link_count % 500)) -eq 0 ]; then
        echo "已创建 $link_count / $file_count 个软链接..."
    fi
done < "$OUTPUT_FILE"

echo "完成！共创建 $link_count 个软链接"
echo "软链接目录: $LN_DIR"


# 更新 compile_commands.json 中的路径
echo ""
echo "开始更新 compile_commands.json 中的路径..."
COMPILE_COMMANDS_FILE="compile_commands.json"
if [ ! -f "$COMPILE_COMMANDS_FILE" ]; then
    echo "警告: $COMPILE_COMMANDS_FILE 文件不存在，跳过路径更新步骤"
    exit 0
fi

cp "$COMPILE_COMMANDS_FILE" "$LN_DIR/$COMPILE_COMMANDS_FILE"

cd "$LN_DIR" || exit 1

# 替换内核源码路径
OLD_KERNEL_PATH="$SRC_DIR"
NEW_KERNEL_PATH="$LN_DIR"
sed -i "s|$OLD_KERNEL_PATH|$NEW_KERNEL_PATH|g" "$COMPILE_COMMANDS_FILE"

# 替换编译目录路径
OLD_BUILD_DIR=$(grep -o '"directory": "[^"]*"' "$COMPILE_COMMANDS_FILE" | head -1 | cut -d'"' -f4 | sed 's/\/$//')
NEW_BUILD_DIR="$LN_DIR"
if [ -n "$OLD_BUILD_DIR" ]; then
    sed -i "s|$OLD_BUILD_DIR|$NEW_BUILD_DIR|g" "$COMPILE_COMMANDS_FILE"
else
    echo "警告: 无法识别旧的编译目录路径"
fi

echo "路径替换完成!"
echo ""
echo "替换统计:"
echo "- 内核源码路径: $OLD_KERNEL_PATH -> $NEW_KERNEL_PATH"
echo "- 编译目录路径: $OLD_BUILD_DIR -> $NEW_BUILD_DIR"
cd "$WORK_DIR" || exit 1

# 同步内核配置文件和设备树文件
echo "manual sync kernel config and dtb files..."
