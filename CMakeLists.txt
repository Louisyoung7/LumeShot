cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 版本号唯一来源：改这里即可，打包脚本和 CI 都读这个值
project(LumeShot VERSION 0.5.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 本地安装的 Qt6（~/Qt/<version>/gcc_64）不在系统搜索路径里，这里补一个提示
find_package(Qt6 6.5 QUIET COMPONENTS Widgets)
if(NOT Qt6_FOUND AND EXISTS "$ENV{HOME}/Qt")
    file(GLOB _qt6_roots LIST_DIRECTORIES true "$ENV{HOME}/Qt/*/gcc_64")
    if(_qt6_roots)
        list(SORT _qt6_roots)
        list(REVERSE _qt6_roots)
        list(GET _qt6_roots 0 _qt6_root)
        list(PREPEND CMAKE_PREFIX_PATH "${_qt6_root}")
    endif()
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Gui Network)
find_package(X11 REQUIRED)

# 源文件按模块分组，目录即模块边界：
#   app      应用编排：主控制器、长截图会话
#   capture  抓屏与长截图拼接
#   config   配置读写
#   log      运行日志：捕获 Qt 消息并写入用户数据目录
#   output   截图输出：剪贴板与 PNG 保存
#   platform 平台适配：X11 全局热键、XDG 开机自启
#   update   检查更新、下载与安装
#   ui       界面层：托盘、框选、长截图、钉图、设置
set(LUMESHOT_APP_SOURCES
    src/app/appcontroller.cpp
    src/app/longcapturesession.cpp
)

set(LUMESHOT_CAPTURE_SOURCES
    src/capture/screencapture.cpp
    src/capture/scrollcapture.cpp
    src/capture/scrollsession.cpp
    src/capture/stitch.cpp
)

set(LUMESHOT_CONFIG_SOURCES
    src/config/appsettings.cpp
)

set(LUMESHOT_LOG_SOURCES
    src/log/filelogger.cpp
)

set(LUMESHOT_OUTPUT_SOURCES
    src/output/screenshotoutput.cpp
)

set(LUMESHOT_PLATFORM_SOURCES
    src/platform/autostart.cpp
    src/platform/globalhotkey.cpp
)

set(LUMESHOT_UPDATE_SOURCES
    src/update/releasemetadata.cpp
    src/update/updatechecker.cpp
    src/update/updateinstaller.cpp
    src/update/version.cpp
)

set(LUMESHOT_UI_SOURCES
    src/ui/tray/traycontroller.cpp
    src/ui/selection/selectionoverlay.cpp
    src/ui/pin/pinwindow.cpp
    src/ui/longcapture/longcaptureframe.cpp
    src/ui/longcapture/longcapturepreview.cpp
    src/ui/longcapture/longcapturesidebar.cpp
    src/ui/settings/hotkeyedit.cpp
    src/ui/settings/settingsdialog.cpp
)

qt_add_executable(lumeshot
    src/main.cpp
    ${LUMESHOT_APP_SOURCES}
    ${LUMESHOT_CAPTURE_SOURCES}
    ${LUMESHOT_CONFIG_SOURCES}
    ${LUMESHOT_LOG_SOURCES}
    ${LUMESHOT_OUTPUT_SOURCES}
    ${LUMESHOT_PLATFORM_SOURCES}
    ${LUMESHOT_UPDATE_SOURCES}
    ${LUMESHOT_UI_SOURCES}
)

# src 作为头文件根，源文件里用 "app/xxx.h"、"ui/tray/xxx.h" 标明所属模块
target_include_directories(lumeshot PRIVATE src)

target_compile_definitions(lumeshot PRIVATE APP_VERSION="${PROJECT_VERSION}")

target_link_libraries(lumeshot PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Network
    X11::X11
    ${X11_Xi_LIB}
)

# 输入法插件不在 Qt 默认部署集合里，需显式包含；桌面端用不到 eglfs 与 networkinformation 插件
qt_import_plugins(lumeshot
    INCLUDE Qt6::QIbusPlatformInputContextPlugin Qt6::QComposePlatformInputContextPlugin
    EXCLUDE_BY_TYPE egldeviceintegrations
    EXCLUDE_BY_TYPE networkinformation
)

# ---------- 安装 ----------
include(GNUInstallDirs)

# 安装后的程序从 ../lib 找内置 Qt 库；Qt 官方插件自带 $ORIGIN/../../lib
set_target_properties(lumeshot PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

install(TARGETS lumeshot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/lumeshot.desktop DESTINATION /usr/share/applications)
install(FILES packaging/lumeshot.svg DESTINATION /usr/share/icons/hicolor/scalable/apps)

# /usr/bin 下放软链接，实际程序在 /opt/lumeshot
install(CODE [[
    file(MAKE_DIRECTORY "$ENV{DESTDIR}/usr/bin")
    file(CREATE_LINK "${CMAKE_INSTALL_PREFIX}/bin/lumeshot"
         "$ENV{DESTDIR}/usr/bin/lumeshot" SYMBOLIC)
]])

# 安装时由 Qt 官方部署脚本收集运行库与插件，不再手写 DT_NEEDED 遍历
qt_generate_deploy_app_script(
    TARGET lumeshot
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS
)
install(SCRIPT ${deploy_script})

# 离线复现工具：重放 LUMESHOT_SCROLL_DEBUG 导出的帧序列，用于定位拼接问题
add_executable(lumeshot-replay
    tools/scrollreplay.cpp
    src/capture/scrollsession.cpp
    src/capture/stitch.cpp
)
target_include_directories(lumeshot-replay PRIVATE src)
target_link_libraries(lumeshot-replay PRIVATE Qt6::Gui)

# 单元测试：ctest --test-dir build --output-on-failure
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

# ---------- CPack：deb 安装包 ----------
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "lumeshot")
set(CPACK_PACKAGE_VENDOR "lumeshot")
set(CPACK_PACKAGE_CONTACT "Louis <louis@localhost>")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "X11 screenshot tool with scroll capture and pinning")
set(CPACK_PACKAGE_DESCRIPTION
    "LumeShot captures screen regions, the full desktop and active windows, stitches scrolling pages into one long image, and pins screenshots on top of the desktop. It stays in the system tray and is triggered by a configurable global hotkey.")
set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/lumeshot")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
# Qt 与 ICU 已随包内置，Qt 库的系统依赖由下面手工声明
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libc6 (>= 2.34), libstdc++6, libgcc-s1, \
libegl1, libgl1, libglx0, libopengl0, \
libx11-6, libx11-xcb1, libxext6, libxi6, \
libxcb1, libxcb-cursor0, libxcb-glx0, libxcb-icccm4, libxcb-image0, \
libxcb-keysyms1, libxcb-randr0, libxcb-render0, libxcb-render-util0, \
libxcb-shape0, libxcb-shm0, libxcb-sync1, libxcb-xfixes0, libxcb-xkb1, \
libxkbcommon0, libxkbcommon-x11-0")

execute_process(COMMAND dpkg --print-architecture
                OUTPUT_VARIABLE LUMESHOT_DEB_ARCH
                OUTPUT_STRIP_TRAILING_WHITESPACE)
if(LUMESHOT_DEB_ARCH)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${LUMESHOT_DEB_ARCH}")
endif()

include(CPack)

