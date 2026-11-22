#!/usr/bin/env bash
# 打包成 deb：程序 + Qt 运行库 + 平台插件装到 /opt/lumeshot，自包含。
set -euo pipefail

PKG=lumeshot
ARCH=amd64
TITLE="LumeShot"

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

# 版本号只从 CMakeLists 的 project(... VERSION x.y.z) 读取，避免多处维护
VERSION=$(sed -n 's/^project(LumeShot VERSION \([0-9][0-9.]*\).*/\1/p' "$ROOT_DIR/CMakeLists.txt" | head -1)
[[ -n "$VERSION" ]] || { echo "无法从 CMakeLists.txt 读取版本号" >&2; exit 1; }
BUILD_ROOT="$ROOT_DIR/build"
BUILD_DIR="$BUILD_ROOT/deb-build"
STAGE_DIR="$BUILD_ROOT/deb-stage"
OUT_DIR="$BUILD_ROOT/deb-out"
APP_DIR="$STAGE_DIR/opt/$PKG"

cd "$ROOT_DIR"

log() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }

# ---------- 1. 独立构建 ----------
# 单独目录 + 关闭 build rpath，避免打包后的二进制带着本机 ~/Qt 绝对路径
log "配置并构建 Release"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_SKIP_BUILD_RPATH=ON
cmake --build "$BUILD_DIR" -j"$(nproc)"

BINARY="$BUILD_DIR/lumeshot"
[[ -x "$BINARY" ]] || { echo "构建产物不存在: $BINARY" >&2; exit 1; }

# ---------- 2. 定位 Qt ----------
QT_PREFIX=$(sed -n 's/^Qt6_DIR:PATH=\(.*\)\/lib\/cmake\/Qt6$/\1/p' "$BUILD_DIR/CMakeCache.txt" | head -1)
QT_PREFIX=${QT_PREFIX:-"$HOME/Qt/6.8.3/gcc_64"}
QT_PLUGINS="$QT_PREFIX/plugins"
[[ -d "$QT_PLUGINS" ]] || { echo "找不到 Qt 插件目录: $QT_PLUGINS" >&2; exit 1; }
log "使用 Qt: $QT_PREFIX"

# ---------- 3. 布置目录 ----------
log "准备打包目录"
rm -rf "$STAGE_DIR" "$OUT_DIR"
mkdir -p "$STAGE_DIR/DEBIAN" \
         "$APP_DIR/bin" "$APP_DIR/lib" "$APP_DIR/plugins" \
         "$STAGE_DIR/usr/bin" \
         "$STAGE_DIR/usr/share/applications" \
         "$STAGE_DIR/usr/share/icons/hicolor/scalable/apps" \
         "$OUT_DIR"

install -m 0755 "$BINARY" "$APP_DIR/bin/lumeshot"
install -m 0644 "$ROOT_DIR/packaging/lumeshot.desktop" "$STAGE_DIR/usr/share/applications/$PKG.desktop"
install -m 0644 "$ROOT_DIR/packaging/lumeshot.svg" "$STAGE_DIR/usr/share/icons/hicolor/scalable/apps/$PKG.svg"

# 只保留 X11 需要的最小插件集；tls 用于检查更新的 HTTPS 请求
for sub in platforms xcbglintegrations platforminputcontexts tls; do
    [[ -d "$QT_PLUGINS/$sub" ]] || continue
    mkdir -p "$APP_DIR/plugins/$sub"
    if [[ "$sub" == "platforms" ]]; then
        cp -a "$QT_PLUGINS/$sub/libqxcb.so" "$APP_DIR/plugins/$sub/"
    else
        cp -a "$QT_PLUGINS/$sub/." "$APP_DIR/plugins/$sub/"
    fi
done

# 让程序按相对路径找插件
cat > "$APP_DIR/bin/qt.conf" <<'EOF'
[Paths]
Prefix=..
Plugins=plugins
EOF

# ---------- 4. 收集 Qt 运行库 ----------
# 按 ELF 的 DT_NEEDED 递归取，只从 Qt 目录里拷，系统库保持依赖关系
log "收集 Qt 运行库"
queue=("$BINARY")
while IFS= read -r -d '' f; do queue+=("$f"); done < <(find "$APP_DIR/plugins" -name '*.so' -type f -print0)

processed=""
while [[ ${#queue[@]} -gt 0 ]]; do
    file="${queue[0]}"
    queue=("${queue[@]:1}")
    case " $processed " in *" $file "*) continue;; esac
    processed="$processed $file"

    while read -r soname; do
        [[ -n "$soname" ]] || continue
        [[ -e "$APP_DIR/lib/$soname" ]] && continue
        libpath="$QT_PREFIX/lib/$soname"
        [[ -e "$libpath" ]] || continue
        real=$(readlink -f "$libpath")
        cp -a "$real" "$APP_DIR/lib/"
        ln -sf "$(basename "$real")" "$APP_DIR/lib/$soname"
        queue+=("$real")
    done < <(readelf -d "$file" 2>/dev/null | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')
done
log "已收集 $(find "$APP_DIR/lib" -maxdepth 1 -type f | wc -l) 个 Qt 库文件"

# 缺库检查：运行时所有 NEEDED 都必须能找到
MISSING=$(LD_LIBRARY_PATH="$APP_DIR/lib" ldd "$APP_DIR/bin/lumeshot" | grep "not found" || true)
if [[ -n "$MISSING" ]]; then
    echo "以下依赖未找到，打包中止：" >&2
    echo "$MISSING" >&2
    exit 1
fi

# ---------- 5. 启动脚本 ----------
cat > "$STAGE_DIR/usr/bin/$PKG" <<'EOF'
#!/bin/sh
DIR=/opt/lumeshot
LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH
exec "$DIR/bin/lumeshot" "$@"
EOF
chmod 0755 "$STAGE_DIR/usr/bin/$PKG"

# ---------- 6. 依赖计算 ----------
log "计算系统依赖"
SHLIB_DIR="$BUILD_ROOT/deb-shlibdeps"
rm -rf "$SHLIB_DIR"
mkdir -p "$SHLIB_DIR/debian"
cat > "$SHLIB_DIR/debian/control" <<EOF
Source: $PKG
Section: graphics
Priority: optional
Maintainer: Louis <louis@localhost>
Standards-Version: 4.6.2

Package: $PKG
Architecture: any
Description: placeholder
EOF

PLUGIN_SOS=$(find "$APP_DIR/plugins" -name '*.so' -type f)
DEPENDS=$(cd "$SHLIB_DIR" && dpkg-shlibdeps --ignore-missing-info -O \
            "$APP_DIR/bin/lumeshot" $PLUGIN_SOS 2>/dev/null \
          | sed -n 's/^shlibs:Depends=//p' || true)
# Qt 与 ICU 已随包内置，不需要系统再装
DEPENDS=$(printf '%s' "$DEPENDS" | tr ',' '\n' | sed 's/^ *//; s/ *$//' \
          | grep -vE '^(libqt6|libicu)' | paste -sd, -)
DEPENDS=${DEPENDS:-"libc6 (>= 2.34), libstdc++6, libx11-6, libxcb1, libxcb-cursor0, libxkbcommon-x11-0, libgl1, libfontconfig1, libdbus-1-3"}
log "Depends: $DEPENDS"

# ---------- 7. 控制文件 ----------
INSTALLED_SIZE=$(du -sk "$STAGE_DIR/opt" "$STAGE_DIR/usr" | awk '{s+=$1} END {print s}')
cat > "$STAGE_DIR/DEBIAN/control" <<EOF
Package: $PKG
Version: $VERSION
Architecture: $ARCH
Maintainer: Louis <louis@localhost>
Installed-Size: $INSTALLED_SIZE
Depends: $DEPENDS
Section: graphics
Priority: optional
Description: $TITLE - X11 screenshot tool with scroll capture and pinning
 LumeShot captures screen regions, the full desktop and active windows,
 stitches scrolling pages into one long image, and pins screenshots on
 top of the desktop. It stays in the system tray and is triggered by a
 configurable global hotkey.
EOF

(cd "$STAGE_DIR" && find . -path ./DEBIAN -prune -o -type f -print0 | sort -z | xargs -0 md5sum > DEBIAN/md5sums)

# ---------- 8. 生成 deb ----------
log "打包"
DEB_FILE="$OUT_DIR/${PKG}_${VERSION}_${ARCH}.deb"
dpkg-deb --root-owner-group -Zxz --build "$STAGE_DIR" "$DEB_FILE" >/dev/null
(cd "$OUT_DIR" && sha256sum "$(basename "$DEB_FILE")" > "$(basename "$DEB_FILE").sha256")

log "完成: $DEB_FILE ($(du -h "$DEB_FILE" | cut -f1))"
dpkg-deb -I "$DEB_FILE" | sed -n '/Package:/,$p' | head -20
