#include <windows.h>
#include <wchar.h>
#include <wctype.h>

/* 显示 Unicode 错误提示。 */
static void show_error(const wchar_t *message)
{
    MessageBoxW(NULL, message, L"启动失败", MB_OK | MB_ICONERROR);
}

/* directory 非 NULL 时创建映射；为 NULL 时删除映射。 */
static int run_subst(const wchar_t *drive, const wchar_t *directory)
{
    wchar_t command_line[MAX_PATH * 3];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 1;

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (directory != NULL)
    {
        /*
         * subst.exe X: "C:\程序所在目录"
         * lpApplicationName 指定 subst.exe，命令行使用宽字符。
         */
        if (wsprintfW(command_line, L"subst.exe %s \"%s\"",
                      drive, directory) >= (int)(sizeof(command_line) / sizeof(command_line[0])))
        {
            return 0;
        }
    }
    else
    {
        wsprintfW(command_line, L"subst.exe %s /D", drive);
    }

    if (!CreateProcessW(
            NULL,
            command_line,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &si,
            &pi))
    {
        return 0;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return exit_code == 0;
}

/* 拒绝绝对路径和 .. 路径段，只允许相对路径。 */
static int is_safe_relative_path(const wchar_t *path)
{
    const wchar_t *p;
    const wchar_t *segment;

    if (path == NULL || path[0] == L'\0')
        return 0;

    if (path[0] == L'\\' || path[0] == L'/' ||
        wcschr(path, L':') != NULL)
        return 0;

    segment = path;

    for (p = path; ; ++p)
    {
        if (*p == L'\\' || *p == L'/' || *p == L'\0')
        {
            if ((p - segment) == 2 &&
                segment[0] == L'.' &&
                segment[1] == L'.')
                return 0;

            if (*p == L'\0')
                break;

            segment = p + 1;
        }
    }

    return 1;
}

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE previous_instance,
    PWSTR command_line_args,
    int show_state)
{
    wchar_t module_path[MAX_PATH];
    wchar_t base_dir[MAX_PATH];
    wchar_t ini_path[MAX_PATH];
    wchar_t drive[16];
    wchar_t run_cmd[MAX_PATH];
    wchar_t drive_root[4];
    wchar_t exe_path[MAX_PATH];
    wchar_t exe_dir[MAX_PATH];
    wchar_t child_command_line[MAX_PATH * 2];
    wchar_t error_message[MAX_PATH + 128];
    wchar_t *slash;
    DWORD logical_drives;
    DWORD exit_code = 1;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    (void)instance;
    (void)previous_instance;
    (void)command_line_args;
    (void)show_state;

    if (GetModuleFileNameW(NULL, module_path, MAX_PATH) == 0)
    {
        show_error(L"无法获取启动器所在路径。");
        return 1;
    }

    if (wcslen(module_path) >= MAX_PATH)
    {
        show_error(L"启动器路径过长。");
        return 1;
    }

    wcscpy(base_dir, module_path);
    slash = wcsrchr(base_dir, L'\\');

    if (slash == NULL)
    {
        show_error(L"无法确定启动器所在目录。");
        return 1;
    }

    *slash = L'\0';

    if (wsprintfW(ini_path, L"%s\\config.ini", base_dir) >= MAX_PATH)
    {
        show_error(L"配置文件路径过长。");
        return 1;
    }

    GetPrivateProfileStringW(
        L"RUN", L"Vdisk", L"", drive,
        sizeof(drive) / sizeof(drive[0]), ini_path);

    GetPrivateProfileStringW(
        L"RUN", L"RunCmd", L"", run_cmd,
        sizeof(run_cmd) / sizeof(run_cmd[0]), ini_path);

    if (wcslen(drive) != 2 ||
        !iswalpha(drive[0]) ||
        drive[1] != L':')
    {
        show_error(L"配置错误：Vdisk 必须是类似 X: 的盘符。");
        return 1;
    }

    if (!is_safe_relative_path(run_cmd))
    {
        show_error(L"配置错误：RunCmd 必须是程序目录下的相对路径，且不能包含 .. 路径段。");
        return 1;
    }

    drive[0] = (wchar_t)towupper(drive[0]);

    drive_root[0] = drive[0];
    drive_root[1] = L':';
    drive_root[2] = L'\\';
    drive_root[3] = L'\0';

    /* 不覆盖已存在的盘符。 */
    logical_drives = GetLogicalDrives();

    if (logical_drives & (1UL << (drive[0] - L'A')))
    {
        wsprintfW(error_message,
                  L"盘符 %s 已存在，请在配置文件中改用其他盘符。",
                  drive);
        show_error(error_message);
        return 1;
    }

    if (!run_subst(drive, base_dir))
    {
        wsprintfW(error_message, L"创建 %s 映射失败。", drive);
        show_error(error_message);
        return 1;
    }

    if (wsprintfW(exe_path, L"%s%s", drive_root, run_cmd) >= MAX_PATH)
    {
        show_error(L"目标程序路径过长。");
        run_subst(drive, NULL);
        return 1;
    }

    if (GetFileAttributesW(exe_path) == INVALID_FILE_ATTRIBUTES)
    {
        wsprintfW(error_message, L"找不到要运行的文件：\n%s", exe_path);
        show_error(error_message);
        run_subst(drive, NULL);
        return 1;
    }

    wcscpy(exe_dir, exe_path);
    slash = wcsrchr(exe_dir, L'\\');

    if (slash != NULL)
        *slash = L'\0';

    if (wsprintfW(child_command_line, L"\"%s\"", exe_path) >=
        (int)(sizeof(child_command_line) / sizeof(child_command_line[0])))
    {
        show_error(L"启动命令行过长。");
        run_subst(drive, NULL);
        return 1;
    }

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    /*
     * 控制台子程序不显示控制台；
     * GUI 子程序仍显示它自己的窗口。
     */
    if (!CreateProcessW(
            exe_path,
            child_command_line,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            exe_dir,
            &si,
            &pi))
    {
        wsprintfW(error_message,
                  L"启动程序失败，错误码：%lu",
                  (unsigned long)GetLastError());
        show_error(error_message);
        run_subst(drive, NULL);
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    run_subst(drive, NULL);

    return (int)exit_code;
}
