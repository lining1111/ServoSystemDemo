#!/bin/bash
# 通用部署打包脚本：把程序及其非系统依赖库收集到目标目录，形成可拷贝运行的部署包
#
# 用法: ./install_app_linux.sh <程序路径> [配置文件名] [目标目录]
#   参数1 程序路径     必填，程序（可执行文件）路径
#   参数2 配置文件名   可为空，存在则一并拷贝到目标目录
#   参数3 目标目录     默认 install，lib 子目录放依赖库
#
# 步骤:
#   0. 清理目标目录（只清空目录内容，目录本身保留）
#   1. 程序拷贝到目标目录
#   2. 检查程序 RPATH 是否包含 $ORIGIN 和 $ORIGIN/lib，没有则用 patchelf 补上
#   3. 用 ldd 收集程序（及依赖库自身）的运行时依赖，三方库拷贝到 lib/ 下；
#      glibc 相关（libc/libm/libpthread/librt/加载器 ld-linux* 等）和编译器运行时
#      （libstdc++/libgcc_s）不拷贝，由目标机自带
#
# 注意: 补 RPATH 依赖 patchelf，未安装时请先 sudo apt install patchelf
set -e

PROG_SRC="$1"
CONFIG_FILE="$2"
DEST_DIR="${3:-install}"
LIB_DIR="$DEST_DIR/lib"
RUNPATH_TARGET='$ORIGIN:$ORIGIN/lib'

if [ -z "$PROG_SRC" ]; then
	echo "用法: $0 <程序路径> [配置文件名] [目标目录(默认install)]" >&2
	exit 1
fi
if [ ! -f "$PROG_SRC" ]; then
	echo "错误: 程序不存在: $PROG_SRC" >&2
	exit 1
fi

PROG_NAME=$(basename "$PROG_SRC")
PROG_DEST="$DEST_DIR/$PROG_NAME"

# ---------- 步骤0: 清理目标目录 ----------
mkdir -p "$DEST_DIR"
DEST_ABS=$(cd "$DEST_DIR" && pwd -P)
PROG_ABS=$(cd "$(dirname "$PROG_SRC")" && pwd -P)/$(basename "$PROG_SRC")

# 安全防护：拒绝高危清理目标，避免 rm -rf 误删
case "$DEST_ABS" in
	"/" | "/usr" | "/home" | "$HOME")
		echo "错误: 拒绝清理高危目录: $DEST_ABS" >&2
		exit 1
		;;
esac
# 程序本身若在待清理目录内，先清理会把源程序删掉
case "$PROG_ABS" in
	"$DEST_ABS"/*)
		echo "错误: 程序位于待清理的目标目录内，清理会删除源程序: $PROG_ABS" >&2
		echo "      请把目标目录换到程序所在目录之外" >&2
		exit 1
		;;
esac

# 只清空目录内容，保留目录本身
if [ -d "$DEST_DIR" ]; then
	find "$DEST_DIR" -mindepth 1 -maxdepth 1 -exec rm -rf {} +
	echo "[0/3] 目标目录已清理: $DEST_ABS"
fi

mkdir -p "$LIB_DIR"

# ---------- 步骤1: 拷贝程序和配置 ----------
cp "$PROG_SRC" "$PROG_DEST"
chmod +x "$PROG_DEST"
echo "[1/3] 程序已拷贝: $PROG_SRC -> $PROG_DEST"

if [ -n "$CONFIG_FILE" ]; then
	if [ -f "$CONFIG_FILE" ]; then
		cp "$CONFIG_FILE" "$DEST_DIR/"
		echo "      配置文件已拷贝: $CONFIG_FILE -> $DEST_DIR/"
	else
		echo "警告: 配置文件不存在，跳过: $CONFIG_FILE" >&2
	fi
fi

# ---------- 步骤2: 检查/补 RPATH ----------
# RUNPATH 或 RPATH 都算有效；要求同时包含 $ORIGIN 和 $ORIGIN/lib
current_rpath=$(readelf -d "$PROG_DEST" 2>/dev/null |
	sed -n 's/.*Library runpath: \[\(.*\)\]/\1/p; s/.*Library rpath: \[\(.*\)\]/\1/p' |
	head -1)

rpath_ok=false
if [ -n "$current_rpath" ]; then
	case "$current_rpath" in
		*'$ORIGIN:'*'$ORIGIN/lib'*|*'$ORIGIN/lib:'*'$ORIGIN'*) rpath_ok=true ;;
		*'$ORIGIN/lib'*'$ORIGIN:'*) rpath_ok=true ;;
	esac
fi

if [ "$rpath_ok" = true ]; then
	echo "[2/3] RPATH 检查通过: [$current_rpath]"
else
	if ! command -v patchelf >/dev/null 2>&1; then
		echo "错误: RPATH 缺少 \$ORIGIN:\$ORIGIN/lib（当前: [${current_rpath:-无}]），且未安装 patchelf" >&2
		echo "      请执行: sudo apt install patchelf 后重试" >&2
		exit 1
	fi
	patchelf --set-rpath "$RUNPATH_TARGET" "$PROG_DEST"
	echo "[2/3] RPATH 已修正为 [$RUNPATH_TARGET]（原: [${current_rpath:-无}]）"
fi

# ---------- 步骤3: 收集非系统依赖库 ----------
# 判定基础系统库（不拷贝）：
#   - glibc 家族：libc/libm/libdl/libpthread/librt/libresolv/libnsl/libutil/libcrypt/libanl/
#     libmvec/libBrokenLocale/libthread_db/libnss_* 等，以及动态加载器 ld-linux*/ld.so*
#   - 编译器运行时：libgcc_s / libstdc++（目标机自带，如需极致兼容可从下面删掉对应分支）
# 其余库（含装在 /usr/lib 下的三方库，如 aravis/glib/Poco/opencv/grpc）一律按三方库拷贝
is_base_system_lib() {
	local name
	name=$(basename "$1")
	case "$name" in
		# glibc 家族
		libc.so* | libc-*.so | libm.so* | libm-*.so | libmvec.so* | libdl.so* | libpthread.so* | \
			librt.so* | libresolv.so* | libnsl.so* | libutil.so* | libcrypt.so* | libanl.so* | \
			libBrokenLocale.so* | libthread_db.so* | libnss_*.so* | libmcheck.so*)
			return 0 ;;
		# 动态加载器
		ld-linux*.so* | ld64.so* | ld.so.* | ld-*.so*)
			return 0 ;;
		# 编译器运行时
		libgcc_s.so* | libstdc++.so*)
			return 0 ;;
		*)
			return 1 ;;
	esac
}

# BFS：对程序及其拷贝下来的每个依赖库都跑一遍 ldd，保证传递依赖也被收集
declare -A VISITED=()
QUEUE=("$PROG_DEST")
COPIED=0

while [ ${#QUEUE[@]} -gt 0 ]; do
	CURRENT="${QUEUE[0]}"
	QUEUE=("${QUEUE[@]:1}")

	while IFS= read -r line; do
		case "$line" in
			*"=>"*)
				# 形如: libfoo.so.1 => /path/libfoo.so.1 (0x...) 或 libfoo.so.1 => not found
				LIB_PATH=$(echo "$line" | awk '{print $3}')
				if [ "$LIB_PATH" = "not" ] || [ "$LIB_PATH" = "not," ] || [ "$LIB_PATH" = "not." ] ||
					echo "$line" | grep -q "not found"; then
					LIB_NAME=$(echo "$line" | awk '{print $1}')
					echo "警告: 依赖库缺失（not found）: $LIB_NAME" >&2
					continue
				fi
				;;
			/)
				# 形如: /lib64/ld-linux-x86-64.so.2 (0x...)，路径在第一列
				LIB_PATH=$(echo "$line" | awk '{print $1}')
				;;
			*)
				# linux-vdso 等无文件路径的条目
				continue
				;;
		esac

		[ -f "$LIB_PATH" ] || continue
		[ -n "${VISITED[$LIB_PATH]}" ] && continue
		VISITED[$LIB_PATH]=1

		if is_base_system_lib "$LIB_PATH"; then
			continue
		fi

		# 三方库：拷贝到 lib/ 并继续追它的依赖
		if [ ! -f "$LIB_DIR/$(basename "$LIB_PATH")" ]; then
			cp -L "$LIB_PATH" "$LIB_DIR/"
			COPIED=$((COPIED + 1))
			echo "      拷贝依赖库: $LIB_PATH"
		fi
		QUEUE+=("$LIB_DIR/$(basename "$LIB_PATH")")
	done < <(ldd "$CURRENT" 2>/dev/null)
done

echo "[3/3] 依赖库收集完成: 共拷贝 $COPIED 个三方库到 $LIB_DIR/（已忽略 glibc 及相关基础库）"
echo "部署包就绪: $DEST_DIR/  （程序: $PROG_NAME，可直接整目录拷贝到目标机器运行）"
