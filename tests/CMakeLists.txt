# 单元测试只编译与界面无关的纯逻辑，避免引入 X11 / Widgets 依赖。
set(LUMESHOT_TEST_SUPPORT_SOURCES
    ${PROJECT_SOURCE_DIR}/src/capture/stitch.cpp
    ${PROJECT_SOURCE_DIR}/src/capture/scrollsession.cpp
    ${PROJECT_SOURCE_DIR}/src/update/version.cpp
    ${PROJECT_SOURCE_DIR}/src/config/appsettings.cpp
)

add_library(lumeshot_test_support STATIC ${LUMESHOT_TEST_SUPPORT_SOURCES})
target_include_directories(lumeshot_test_support PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(lumeshot_test_support PUBLIC Qt6::Core Qt6::Gui)

set(LUMESHOT_TESTS
    tst_stitch
    tst_scrollsession
    tst_version
    tst_appsettings
)

foreach(test_name IN LISTS LUMESHOT_TESTS)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE lumeshot_test_support Qt6::Test)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
