#!/bin/bash

# 定义源文件夹和目标文件夹的路径
SOURCE_DIR="/mnt/hgfs/shared"
TARGET_DIR="/home/book/esp32/esp32_test/watch/watch1/components/ui"

# 检查源文件夹是否存在
if [ ! -d "$SOURCE_DIR" ]; then
    echo "错误：源文件夹不存在！请检查路径：$SOURCE_DIR"
    exit 1
fi

# 检查目标文件夹是否存在
if [ ! -d "$TARGET_DIR" ]; then
    echo "错误：目标文件夹不存在！请检查路径：$TARGET_DIR"
    exit 1
fi

# 1. 删除目标文件夹下的所有内容（不删除文件夹本身）
# 使用 rm -rf 是危险操作，请再次确认路径！
echo "正在删除目标文件夹下的内容：$TARGET_DIR/*"
rm -rf "$TARGET_DIR"/*

# 检查删除操作是否成功
if [ $? -ne 0 ]; then
    echo "错误：删除操作失败！"
    exit 1
fi

# 2. 从源文件夹拷贝内容到目标文件夹
echo "正在从源文件夹拷贝内容到目标文件夹..."
cp -r "$SOURCE_DIR"/* "$TARGET_DIR"/

# 检查拷贝操作是否成功
if [ $? -ne 0 ]; then
    echo "错误：拷贝操作失败！"
    exit 1
fi

echo "操作完成！内容已成功从 $SOURCE_DIR 迁移到 $TARGET_DIR"
