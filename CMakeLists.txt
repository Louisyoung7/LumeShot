cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 版本号唯一来源：改这里即可，打包脚本和 CI 都读这个值
project(ScreenShot VERSION 0.1.0 LANGUAGES CXX)

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

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Gui)
find_package(X11 REQUIRED)

add_executable(screenshot
    src/main.cpp
    src/app/appcontroller.cpp
    src/app/longcapturesession.cpp
    src/app/screenshotoutput.cpp
    src/app/traycontroller.cpp
    src/capture/screencapture.cpp
    src/capture/scrollcapture.cpp
    src/config/appsettings.cpp
    src/ui/selection/selectionoverlay.cpp
    src/ui/pin/pinwindow.cpp
    src/ui/longcapture/longcaptureframe.cpp
    src/ui/longcapture/longcapturepreview.cpp
    src/ui/longcapture/longcapturesidebar.cpp
    src/ui/settings/hotkeyedit.cpp
    src/ui/settings/settingsdialog.cpp
    src/platform/globalhotkey.cpp
    src/platform/autostart.cpp
)

# src 作为头文件根，源文件里用 "app/xxx.h"、"capture/xxx.h" 标明所属模块
target_include_directories(screenshot PRIVATE src)

target_compile_definitions(screenshot PRIVATE APP_VERSION="${PROJECT_VERSION}")

target_link_libraries(screenshot PRIVATE
    Qt6::Widgets
    Qt6::Gui
    X11::X11
)
