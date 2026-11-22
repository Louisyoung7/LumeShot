# LumeShot

Linux X11 下的截屏工具，基于 Qt6 / C++17 编写。

支持区域截图、全屏截图、窗口截图、滚动长截图和贴图钉图。

常驻系统托盘，全局快捷键一键唤起。

## 功能

- 区域截图：全屏框选，保留选区，侧边工具栏提供确认、钉图、长截图、取消。
- 全屏截图：一次抓取整个虚拟桌面。
- 当前窗口截图：抓取活动窗口所在区域。
- 滚动长截图：框选区域后手动滚动页面，逐帧拼接成长图。
- 贴图钉图：把截图固定在桌面最顶层，随用随看。
- 全局热键：默认 `Ctrl+Alt+A`，可在设置中修改。
- 系统托盘：托盘菜单提供全部入口；没有托盘时自动退回独立小窗口。
- 自动处理：截图后自动复制到剪贴板、自动保存为 PNG。
- 配置持久化：通过 QSettings 保存，重启后保留。

## 使用说明

### 框选截图

1. 按全局热键，或点托盘菜单里的“区域截图”。
2. 按住左键拖出选区。
3. 点选区旁工具栏的按钮：
   - 确认：完成截图，按设置复制和保存。
   - 钉图：截取选区并贴在桌面上。
   - 长截图：进入滚动长截图流程。
   - 取消：退出框选。

框选界面用黑色半透明外圈加亮蓝内圈的边框，四角有 L 形把手。

### 滚动长截图

1. 在框选工具栏点“长截图”。
2. 选区外的边框会保留，选区右侧出现侧边栏。
3. 手动滚动页面，工具每 200ms 抓一帧。
4. 侧边栏顶部是实时缩略图，下方显示已截取高度。
5. 按 `Enter` 或点“完成”结束，按 `Esc` 或点“取消”放弃。
6. 结束后弹出预览，可确认、钉图或取消。

拼接规则：

- 每帧与已拼接结果的底部画面比对重叠行。
- 重叠匹配可靠时才追加新增部分。
- 匹配不可靠时丢弃该帧，避免重复内容或硬拼错位。
- 拼接高度上限为 20000 物理像素。

### 钉图

| 操作 | 效果 |
| --- | --- |
| 按住左键拖动 | 移动贴图窗口 |
| 滚轮 | 以光标为基准缩放 |
| 双击 | 关闭 |
| 右键 | 弹出菜单，可关闭 |

贴图窗口始终置顶，边框画在图片外侧，不遮挡内容。

### 设置

托盘菜单 → “设置…”，可修改：

- 全局快捷键：必须包含 Ctrl、Alt 或 Super。
- 截图后自动复制到剪贴板。
- 截图后自动保存为 PNG。
- 自动保存目录：默认 `~/Pictures/Screenshots`。
- 开机自动启动：登录桌面时自动运行，自启文件写入 `~/.config/autostart/lumeshot.desktop`。

自动复制和自动保存至少要开启一项。

打开设置界面时会先释放全局热键，避免旧快捷键被自身拦截。

## 环境依赖

- CMake 3.16+
- 支持 C++17 的编译器
- Qt 6.5+（Widgets、Gui）
- X11 开发库

安装示例：

```bash
# Debian / Ubuntu
sudo apt install build-essential cmake libx11-dev qt6-base-dev

# Fedora
sudo dnf install gcc-c++ cmake libX11-devel qt6-qtbase-devel

# Arch Linux
sudo pacman -S base-devel cmake libx11 qt6-base
```

如果 Qt6 装在 `~/Qt/<version>/gcc_64`，CMake 会自动把最新的版本加入搜索路径。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/screenshot
```

## 打包 deb

```bash
./packaging/build-deb.sh                  # 默认版本 0.1.0
VERSION=0.2.0 ./packaging/build-deb.sh    # 指定版本
```

产物：`build/deb-out/lumeshot_<版本>_amd64.deb`

- 程序、Qt 运行库和 xcb 插件装到 `/opt/lumeshot`，包内自带，不依赖系统 Qt 版本。
- `Depends` 由实际链接的库自动计算，已内置的 Qt / ICU 会被排除。
- 安装：`sudo apt install ./build/deb-out/lumeshot_0.1.0_amd64.deb`
- 卸载：`sudo apt remove lumeshot`

## 键盘快捷键

| 场景 | 按键 | 作用 |
| --- | --- | --- |
| 全局 | `Ctrl+Alt+A` | 开始区域截图（可修改） |
| 框选 | `Enter` | 确认选区 |
| 框选 | `Esc` | 取消截图 |
| 长截图侧边栏 | `Enter` | 完成拼接 |
| 长截图侧边栏 | `Esc` | 取消拼接 |
| 长截图预览 | `Enter` | 确认结果 |
| 长截图预览 | `Esc` | 取消结果 |

## 项目结构

```text
src/
├── main.cpp                程序入口，检查平台并启动控制器
├── app/                    应用编排：托盘、热键、截图流程、保存与剪贴板
├── capture/                屏幕抓取与长截图拼接算法
├── config/                 配置读写（QSettings）
├── platform/               X11 全局热键、XDG 开机自启
└── ui/                     框选遮罩、钉图窗口、长截图组件、设置对话框
```

模块依赖单向：`ui` / `app` 依赖 `capture`、`config`、`platform`，反向不引用。

头文件统一用 `app/xxx.h`、`capture/xxx.h` 这类模块前缀。

## 已知限制

- 仅支持 X11。Wayland 会话下会提示并退出，可用 `QT_QPA_PLATFORM=xcb` 尝试运行。
- 长截图需要手动滚动页面，不做自动滚动。
- 长截图依赖画面重叠匹配，纯色或大面积重复内容可能降低识别准确度。

## 配置文件位置

配置由 QSettings 保存，默认路径：

```text
~/.config/ScreenShot/ScreenShot.conf
```
