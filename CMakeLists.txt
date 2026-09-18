cmake_minimum_required(VERSION 3.19)

# 生成 compile_commands.json，供 clangd 等工具做代码补全/跳转。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 版本号只在这里定义：程序内显示、发布产物命名、CI 校验 tag 都用它（单一来源）。
project(Serial_Assistant VERSION 1.0.0 LANGUAGES CXX)

# 依赖 Qt6 的 Core / Gui / Widgets / SerialPort 四个模块。
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets SerialPort)

# Qt 官方推荐的现代项目初始化（自动处理 AUTOMOC/AUTOUIC 等）。
qt_standard_project_setup()

# 声明可执行目标及其源文件（含 .ui 文件，构建时会自动生成 ui_*.h）。
qt_add_executable(Serial-Assistant
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    serialcontroller.cpp
    serialcontroller.h
)

target_link_libraries(Serial-Assistant PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
)

# 把版本号带进代码（main.cpp 里用 APP_VERSION 显示在「关于」对话框和标题栏）
target_compile_definitions(Serial-Assistant PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

install(TARGETS Serial-Assistant
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Linux 免安装包：把「查找 Qt 库」的路径写进可执行文件（$ORIGIN = 可执行文件所在目录），
# 这样拷到任何机器上、双击就能跑，不需要设置 LD_LIBRARY_PATH，也不需要目标机装 Qt。
# 两个路径都写上，覆盖两种包结构：
#   $ORIGIN/lib      便携包：本体在 runtime/，库在 runtime/lib/
#   $ORIGIN/../lib   AppImage：exe 在 usr/bin，库在 usr/lib
if(UNIX AND NOT APPLE)
    set_target_properties(Serial-Assistant PROPERTIES
        INSTALL_RPATH "$ORIGIN/lib;$ORIGIN/../lib")
endif()

# 包根目录的启动器：见 launcher/launcher.cpp。
# 有了它，各平台的包结构都能统一成「根目录一个可双击的入口 + runtime/ 装其它所有」。
if(WIN32)
    add_executable(Serial-Assistant-launcher WIN32 launcher/launcher.cpp)
elseif(UNIX AND NOT APPLE)
    add_executable(Serial-Assistant-launcher launcher/launcher.cpp)
endif()
if(TARGET Serial-Assistant-launcher)
    # 这个 target 不链接 Qt，不会自动带上 Qt 要求的 C++ 标准（MSVC 默认 C++14），
    # 所以显式声明一下，免得踩到「C++17 才有的 API 在 MSVC 上编不过」。
    target_compile_features(Serial-Assistant-launcher PRIVATE cxx_std_17)
endif()

# 关于打包：这里**故意不用** qt_generate_deploy_app_script。
# 它依赖 Qt 生成的 QtDeployTargets-<配置>.cmake，在部分环境（如 GitHub 的 windows-latest
# + Qt 6.7）下该文件不会生成，导致 cmake --install 直接失败：
#   include could not find requested file: QtDeployTargets-$<CONFIG>.cmake
# Linux 上它也不完善（会去改 Qt 自带插件的 RPATH 而报错）。
# 因此打包交给 CI 显式完成（见 .github/workflows/release.yml）：
#   Windows → windeployqt     Linux → 自己收 Qt 库 + INSTALL_RPATH
# macOS 上如需打包，用 macdeployqt 即可。

# ---- 单元测试（ctest）----
include(CTest) # 定义 BUILD_TESTING 选项，并 enable_testing()
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(tst_serialcontroller
        tests/tst_serialcontroller.cpp
        serialcontroller.cpp
        serialcontroller.h
    )
    # 测试要输出到控制台，Windows 上不能是 GUI 子系统程序
    set_target_properties(tst_serialcontroller PROPERTIES WIN32_EXECUTABLE FALSE)

    target_include_directories(tst_serialcontroller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_serialcontroller PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::SerialPort
    )

    add_test(NAME serialcontroller COMMAND tst_serialcontroller)
endif()
