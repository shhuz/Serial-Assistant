cmake_minimum_required(VERSION 3.19)

# 生成 compile_commands.json，供 clangd 等工具做代码补全/跳转。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Serial_Assistant LANGUAGES CXX)

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

install(TARGETS Serial-Assistant
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Linux 免安装包：把「查找 Qt 库」的路径写进可执行文件（$ORIGIN = 可执行文件所在目录），
# 这样拷到任何机器上、双击就能跑，不需要设置 LD_LIBRARY_PATH，也不需要目标机装 Qt。
if(UNIX AND NOT APPLE)
    set_target_properties(Serial-Assistant PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib")
endif()

# 生成发布部署脚本：Windows 上会调用 windeployqt 收齐 DLL 和插件，macOS 上是 macdeployqt。
# 只在 Windows / macOS 启用：Linux 上该脚本尚不完善（会去改 Qt 自带插件的 RPATH 而报错），
# Linux 的免安装包由 CI 自己收集 Qt 库（见 .github/workflows/release.yml）。
if(WIN32 OR APPLE)
    qt_generate_deploy_app_script(
        TARGET Serial-Assistant
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()
