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
- 全局热键：默认 `Ctrl+Alt+A`，在设置里点击按钮录制，录制时立即检测是否被占用。
- 开机自启：可写入 XDG autostart，登录桌面时自动运行。
- 系统托盘：托盘菜单提供全部入口；没有托盘时自动退回独立小窗口。
- 自动处理：截图后自动复制到剪贴板、自动保存为 PNG。
- 检查更新：在设置里检查新版本，可直接下载 deb 并通过系统授权窗口安装。
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
2. 选区外的边框会保留，侧边栏优先放在选区外侧（右、左、下、上）；
   选区几乎占满桌面、外侧放不下时，侧边栏提示 0.5 秒后移出屏幕，改用 `Enter` / `Esc` 操作。
3. 手动滚动页面，工具每 200ms 抓一帧。
4. 侧边栏顶部是实时缩略图，下方显示已截取高度。
5. 按 `Enter` 或点“完成”结束，按 `Esc` 或点“取消”放弃；
   侧边栏被移出屏幕时，可从托盘菜单“完成长截图 / 取消长截图”结束。
6. 结束后弹出预览，可确认、钉图或取消。

拼接规则：

- 每帧与已拼接结果的底部画面比对重叠行，左右分栏独立评分。
- 逐像素比对前先去掉行列均值，整体亮度变化不影响识别。
- 固定页头、小面积动画和右侧滚动条不参与有效证据。
- 重叠匹配可靠时才追加新增部分，不可靠时丢弃该帧，避免重复内容或硬拼错位。
- 横向抖动和反向滚动直接丢弃该帧。
- 低纹理画面判为不可靠，丢弃该帧；大段空白因此会被压缩。
- 连续 3 帧无法重叠且画面有纹理时，自动以当前帧重新锚点继续拼接。
- 页面到底后部分应用会回弹，此时露出的背景不追加，避免每滚一下拼一条空白。
- 拼接高度上限为 20000 物理像素。

已知限制：

- 大段空白会被压缩；空白之后若出现纹理，连续 3 帧失败后会重新锚点。
- 重新锚点的接缝处可能重复或跳过少量内容。
- 重复纹理存在多个近似吻合的偏移时取重叠最大的结果，可能压缩重复内容。

排查拼接问题时，打开调试导出后再复现一次：

```bash
LUMESHOT_SCROLL_DEBUG=1 ./build/lumeshot
```

每次长截图会话会生成一个带时间戳的目录（默认在系统临时目录下的 `lumeshot-scroll/`），里面包含：

| 内容 | 说明 |
| --- | --- |
| `frames/frame_0001.png` … | 每 200ms 抓到的原始画面，按顺序编号 |
| `scroll.log` | 每帧一行的判定结果：`outcome`、`overlap`、`misses`、区域尺寸、已拼接高度 |
| `result.png` | 本次会话最终合成的长图 |

把整个目录交给开发者后，可以完全离线重放，不需要再手动滚动页面：

```bash
./build/lumeshot-replay /tmp/lumeshot-scroll/20260101-120000
```

重放会输出与 `scroll.log` 相同格式的逐帧判定，并写出 `replay.png`。
另外可用 `LUMESHOT_SCROLL_DEBUG=/某个目录` 指定导出位置；导出 PNG 会带来额外开销，仅在排查问题时使用。

判定结果里 `outcome` 的含义：

| 取值 | 含义 |
| --- | --- |
| `appended` | 追加了新内容，`overlap` 是重合行数（`0` 表示重新锚点） |
| `no-scroll` | 画面没滚动；页面到底回弹露出的空白也归入此类 |
| `no-match` | 画面变了但找不到可靠重叠，该帧被丢弃 |
| `height-limit` | 达到拼接高度上限，停止追加 |
| `ignored` | 帧尺寸变化或为空，直接忽略 |


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

- 全局快捷键：点击按钮后按下新的组合键。
  - 必须包含 Ctrl、Alt 或 Super。
  - 支持字母、数字、F1-F24、方向键、编辑键和常用标点。
  - `Esc` 取消录制，`Backspace` 或 `Delete` 清除当前快捷键。
  - 录制时立即试注册，被系统或其他程序占用会就地弹窗提示。
- 截图后自动复制到剪贴板。
- 截图后自动保存为 PNG。
- 自动保存目录：默认 `~/Pictures/LumeShot`。
- 开机自动启动：登录桌面时自动运行，自启文件写入 `~/.config/autostart/lumeshot.desktop`。
- 检查更新：查询 GitHub 最新 Release，发现新版本可下载 deb 并弹出系统授权窗口安装。

自动复制和自动保存至少要开启一项。

打开设置界面时会先释放全局热键，避免旧快捷键被自身拦截。

## 环境依赖

- CMake 3.16+
- 支持 C++17 的编译器
- Qt 6.5+（Widgets、Gui、Network；运行单元测试还需 Test）
- X11 与 XInput2 开发库

安装示例：

```bash
# Debian / Ubuntu
sudo apt install build-essential cmake libx11-dev libxi-dev qt6-base-dev

# Fedora
sudo dnf install gcc-c++ cmake libX11-devel libXi-devel qt6-qtbase-devel

# Arch Linux
sudo pacman -S base-devel cmake libx11 libxi qt6-base
```

如果 Qt6 装在 `~/Qt/<version>/gcc_64`，CMake 会自动把最新的版本加入搜索路径。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/lumeshot
./build/lumeshot --version    # 打印当前版本
```

## 单元测试

测试只覆盖与界面无关的纯逻辑（拼接重叠检测、长截图会话状态机、版本比较、配置读写），无需图形环境：

```bash
ctest --test-dir build --output-on-failure
```

默认随 `BUILD_TESTING` 开启；只需构建主程序时可加 `-DBUILD_TESTING=OFF`。

## 打包 deb

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "$(nproc)"
cpack --config build/CPackConfig.cmake -B build/package
```

产物：`build/package/lumeshot_<版本>_<架构>.deb`

- 版本号从 `CMakeLists.txt` 的 `project(... VERSION x.y.z)` 读取，只需要改这一处。
- 程序、Qt 运行库和插件装到 `/opt/lumeshot`，`/usr/bin/lumeshot` 是指向它的软链接，不依赖系统 Qt 版本。
- 运行库与插件由 CMake 的 Qt 官方部署脚本 `qt_generate_deploy_app_script` 收集；输入法插件显式包含，eglfs 与 networkinformation 插件排除。
- `Depends` 在 `CMakeLists.txt` 里手工声明，内置的 Qt / ICU 不计入依赖。
- 安装：`sudo apt install ./build/package/lumeshot_0.3.0_amd64.deb`
- 卸载：`sudo apt remove lumeshot`
- 查版本：`lumeshot --version` 或 `dpkg -s lumeshot | grep Version`

## 版本号与发布

版本号只有两个需要人工维护的地方，值必须一致：

| 位置 | 形式 |
| --- | --- |
| `CMakeLists.txt` | `project(LumeShot VERSION 0.3.0 ...)` |
| git tag | `v0.3.0` |

发布流程：

```bash
# 1. 改版本号并提交
#    CMakeLists.txt: project(LumeShot VERSION 0.3.0 LANGUAGES CXX)
git commit -am "chore: 版本号 0.3.0"

# 2. 打 tag 并推送，触发 CI
git tag v0.3.0
git push origin main
git push origin v0.3.0

# 3. CI 产出草稿 Release，本地补发布说明后发布
gh release edit v0.3.0 --notes-file notes.md --draft=false
```

CI 行为（`.github/workflows/build.yml`）：

- push 到 `main` 或提交 PR：构建并上传 deb 作为 artifact，不发布。
- 推送 `v*` tag：先校验 tag 与 CMakeLists 版本一致，再构建；一致才创建草稿 Release 并附上 deb 和 sha256。
- 发布说明由本地生成，CI 只提供 `generate_release_notes` 的初始内容，不带任何模型密钥。

安装包兼容性：CI 在 Ubuntu 22.04 上构建，deb 依赖要求 glibc ≥ 2.34，可在 Ubuntu 22.04 / 24.04 安装。

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
├── main.cpp                    程序入口，检查平台并启动控制器
├── app/                        应用编排：主控制器、长截图会话
├── capture/                    屏幕抓取与长截图拼接算法
├── config/                     配置读写（QSettings）
├── log/                        运行日志：Qt 消息写入文件
├── output/                     截图输出：剪贴板与 PNG 保存
├── platform/                   X11 全局热键、XDG 开机自启
├── update/                     检查更新、下载与安装
└── ui/                         界面层，按功能拆分
    ├── common/                 绘制工具：选区边框、工具栏图标
    ├── tray/                   系统托盘与无托盘回退窗口
    ├── selection/              全屏框选遮罩与选区工具栏
    ├── longcapture/            长截图边框、实时侧边栏、结果预览
    ├── pin/                    贴图钉图窗口
    └── settings/               设置对话框与快捷键录制控件

tests/                          单元测试，不依赖图形环境
├── tst_stitch.cpp              长截图重叠检测
├── tst_scrollsession.cpp       长截图会话：追加、丢弃、高度上限
├── tst_version.cpp             版本号比较
└── tst_appsettings.cpp         配置读写与默认值
```

模块依赖单向：`app` 负责编排并依赖其余模块；`ui` 只依赖 `capture`、`config` 等底层能力，底层模块不反向引用界面层。

头文件统一使用从 `src` 出发的模块前缀路径，如 `app/xxx.h`、`ui/longcapture/xxx.h`。

## 已知限制

- 仅支持 X11。Wayland 会话下会提示并退出，可用 `QT_QPA_PLATFORM=xcb` 尝试运行。
- 长截图需要手动滚动页面，不做自动滚动。
- 长截图依赖画面重叠匹配，纯色或大面积重复内容可能降低识别准确度。

## 配置文件位置

配置由 QSettings 保存，默认路径：

```text
~/.config/LumeShot/LumeShot.conf
```

## 日志文件

运行时的 info / warning / error 消息会写入用户数据目录，默认路径：

```text
~/.local/share/LumeShot/logs/lumeshot.log
```

日志超过 1 MiB 时轮转为 `lumeshot.log.1`。需要换个位置时设置环境变量：

```bash
LUMESHOT_LOG_FILE=/tmp/lumeshot.log ./build/lumeshot
```

设为 `/dev/null` 可只保留控制台输出。长截图的逐帧导出仍由 `LUMESHOT_SCROLL_DEBUG` 控制，与这里的主日志互不覆盖。
