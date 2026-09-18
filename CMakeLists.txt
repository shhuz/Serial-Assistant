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

# 关于打包：这里**故意不用** qt_generate_deploy_app_script。
# 它依赖 Qt 生成的 QtDeployTargets-<配置>.cmake，在部分环境（如 GitHub 的 windows-latest
# + Qt 6.7）下该文件不会生成，导致 cmake --install 直接失败：
#   include could not find requested file: QtDeployTargets-$<CONFIG>.cmake
# Linux 上它也不完善（会去改 Qt 自带插件的 RPATH 而报错）。
# 因此打包交给 CI 显式完成（见 .github/workflows/release.yml）：
#   Windows → windeployqt     Linux → 自己收 Qt 库 + INSTALL_RPATH
# macOS 上如需打包，用 macdeployqt 即可。
