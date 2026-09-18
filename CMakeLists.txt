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

# 生成发布部署脚本（主要在 Windows/macOS 上使用）。
qt_generate_deploy_app_script(
    TARGET Serial-Assistant
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
