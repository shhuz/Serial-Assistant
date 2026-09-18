// ============================================================
//  launcher/launcher.cpp —— 包根目录的启动器
//
//  目标：让免安装包在三个平台上长得一模一样 ——
//      <包根>/
//      ├── Serial-Assistant(.exe)   ← 用户双击的入口
//      └── runtime/                 ← 本体 + 所有依赖，全在里面
//
//  Windows 为什么非要这个启动器？
//      PE 加载器只从「exe 所在目录 / 系统目录 / PATH」里找 DLL，
//      所以本体必须和 Qt6*.dll 待在同一层。启动器把 runtime/ 挂到 PATH 上，
//      再拉起 runtime/Serial-Assistant.exe，依赖就都能找到。
//      启动器本身不链接 Qt，没有任何依赖，放哪儿都能跑。
//
//  Linux 上其实可以直接把本体放根目录（靠 RPATH 找 lib/），
//      这里为了"各平台结构一致"也统一用启动器。
// ============================================================

#ifdef _WIN32
// ------------------------------------------------------------ Windows
#include <windows.h>

#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  wchar_t exePath[MAX_PATH] = {};
  if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0)
    return 1;

  std::wstring dir(exePath);
  dir.erase(dir.find_last_of(L"\\/") + 1); // 启动器所在目录（包根）
  const std::wstring runtimeDir = dir + L"runtime\\";
  const std::wstring appExe = runtimeDir + L"Serial-Assistant.exe";

  // 把 runtime 目录放到 PATH 最前面：子进程会继承这份环境变量，
  // 加载器就能在里面找到 Qt6Core.dll 等依赖。
  wchar_t oldPath[32767] = {};
  GetEnvironmentVariableW(L"PATH", oldPath, 32767);
  SetEnvironmentVariableW(L"PATH", (runtimeDir + L";" + oldPath).c_str());

  std::wstring cmdLine = L"\"" + appExe + L"\"";
  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};

  if (!CreateProcessW(appExe.c_str(), cmdLine.data(), nullptr, nullptr, FALSE, 0,
                      nullptr, runtimeDir.c_str(), &si, &pi)) {
    MessageBoxW(nullptr,
                L"启动失败：没有找到 runtime\\Serial-Assistant.exe。\n"
                L"请确认解压时整个文件夹都被解压出来了。",
                L"串口助手", MB_ICONERROR | MB_OK);
    return 1;
  }

  // 等本体退出，并把它的退出码带回去
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD exitCode = 0;
  GetExitCodeProcess(pi.hProcess, &exitCode);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return static_cast<int>(exitCode);
}

#else
// ---------------------------------------------------- Linux / 其它 Unix
#include <limits.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  // 自己的路径 → 包根目录（用 /proc/self/exe，比 argv[0] 可靠）
  char self[PATH_MAX] = {};
  const ssize_t len = readlink("/proc/self/exe", self, sizeof(self) - 1);
  if (len <= 0) {
    std::fprintf(stderr, "启动失败：无法定位自身路径\n");
    return 1;
  }
  std::string dir(self, static_cast<size_t>(len));
  dir.erase(dir.find_last_of('/') + 1);

  const std::string runtimeDir = dir + "runtime/";
  const std::string appExe = runtimeDir + "Serial-Assistant";

  // 插件目录指到包内（免得依赖 qt.conf 是否被一起解压出来）
  setenv("QT_PLUGIN_PATH", (runtimeDir + "plugins").c_str(), 1);

  // 库目录也带上：平台插件（libqxcb.so）是运行时 dlopen 进来的，
  // 它自己的依赖同样要走包内 lib/，光靠本体的 RPATH 不一定覆盖得到。
  const std::string libDir = runtimeDir + "lib";
  const char *oldLibPath = getenv("LD_LIBRARY_PATH");
  const std::string libPath =
      (oldLibPath && *oldLibPath) ? libDir + ":" + oldLibPath : libDir;
  setenv("LD_LIBRARY_PATH", libPath.c_str(), 1);
  if (chdir(runtimeDir.c_str()) != 0) {
    std::fprintf(stderr, "启动失败：找不到目录 %s\n", runtimeDir.c_str());
    return 1;
  }

  std::vector<char *> args;
  args.push_back(const_cast<char *>(appExe.c_str()));
  for (int i = 1; i < argc; ++i)
    args.push_back(argv[i]);
  args.push_back(nullptr);

  execv(appExe.c_str(), args.data()); // 成功的话这行不会返回
  std::fprintf(stderr, "启动失败：找不到 %s\n", appExe.c_str());
  return 1;
}
#endif
