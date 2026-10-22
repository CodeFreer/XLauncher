#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void show_error(const char *message)
{
    wchar_t wide_message[1024];

    MultiByteToWideChar(
        CP_UTF8,
        0,
        message,
        -1,
        wide_message,
        sizeof(wide_message) / sizeof(wide_message[0]));

    MessageBoxW(NULL, wide_message, L"启动失败", MB_OK | MB_ICONERROR);
}

/* directory 非 NULL 时创建映射；为 NULL 时删除映射。 */
static int run_subst(const char *drive, const char *directory)
{
    char command_line[MAX_PATH * 3];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 1;

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (directory != NULL)
        sprintf(command_line, "subst.exe %s \"%s\"", drive, directory);
    else
        sprintf(command_line, "subst.exe %s /D", drive);

    if (!CreateProcessA(
            NULL, command_line, NULL, NULL, FALSE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        return 0;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return exit_code == 0;
}

/* 仅允许程序目录下的相对路径，拒绝绝对路径和 .. 路径段。 */
static int is_safe_relative_path(const char *path)
{
    const char *p;
    const char *segment;

    if (path == NULL || path[0] == '\0')
        return 0;

    if (path[0] == '\\' || path[0] == '/' ||
        strchr(path, ':') != NULL)
        return 0;

    segment = path;

    for (p = path; ; ++p)
    {
        if (*p == '\\' || *p == '/' || *p == '\0')
        {
            if ((p - segment) == 2 &&
                segment[0] == '.' &&
                segment[1] == '.')
                return 0;

            if (*p == '\0')
                break;

            segment = p + 1;
        }
    }

    return 1;
}

int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE previous_instance,
    LPSTR command_line_args,
    int show_state)
{
    char module_path[MAX_PATH];
    char base_dir[MAX_PATH];
    char ini_path[MAX_PATH];
    char drive[16];
    char run_cmd[MAX_PATH];
    char drive_root[4];
    char exe_path[MAX_PATH];
    char exe_dir[MAX_PATH];
    char child_command_line[MAX_PATH * 2];
    char error_message[MAX_PATH + 128];
    char *slash;
    DWORD logical_drives;
    DWORD exit_code = 1;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    (void)instance;
    (void)previous_instance;
    (void)command_line_args;
    (void)show_state;

    if (GetModuleFileNameA(NULL, module_path, MAX_PATH) == 0)
    {
        show_error("无法获取启动器所在路径。");
        return 1;
    }

    strcpy(base_dir, module_path);
    slash = strrchr(base_dir, '\\');

    if (slash == NULL)
    {
        show_error("无法确定启动器所在目录。");
        return 1;
    }

    *slash = '\0';
    sprintf(ini_path, "%s\\config.ini", base_dir);

    GetPrivateProfileStringA(
        "RUN", "Vdisk", "", drive, sizeof(drive), ini_path);

    GetPrivateProfileStringA(
        "RUN", "RunCmd", "", run_cmd, sizeof(run_cmd), ini_path);

    if (strlen(drive) != 2 ||
        !isalpha((unsigned char)drive[0]) ||
        drive[1] != ':')
    {
        show_error("配置错误：Vdisk 必须是类似 X: 的盘符。");
        return 1;
    }

    if (!is_safe_relative_path(run_cmd))
    {
        show_error("配置错误：RunCmd 必须是程序目录下的相对路径，且不能包含 .. 路径段。");
        return 1;
    }

    drive[0] = (char)toupper((unsigned char)drive[0]);

    drive_root[0] = drive[0];
    drive_root[1] = ':';
    drive_root[2] = '\\';
    drive_root[3] = '\0';

    /* 不覆盖已存在的盘符。 */
    logical_drives = GetLogicalDrives();
    if (logical_drives & (1UL << (drive[0] - 'A')))
    {
        sprintf(error_message,
                "盘符 %s 已存在，请在配置文件中改用其他盘符。",
                drive);
        show_error(error_message);
        return 1;
    }

    if (!run_subst(drive, base_dir))
    {
        sprintf(error_message, "创建 %s 映射失败。", drive);
        show_error(error_message);
        return 1;
    }

    sprintf(exe_path, "%s%s", drive_root, run_cmd);

    if (GetFileAttributesA(exe_path) == INVALID_FILE_ATTRIBUTES)
    {
        sprintf(error_message, "找不到要运行的文件：\n%s", exe_path);
        show_error(error_message);
        run_subst(drive, NULL);
        return 1;
    }

    strcpy(exe_dir, exe_path);
    slash = strrchr(exe_dir, '\\');
    if (slash != NULL)
        *slash = '\0';

    sprintf(child_command_line, "\"%s\"", exe_path);

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    /*
     * 控制台子程序不显示控制台窗口；
     * GUI 子程序仍显示自己的窗口。
     */
    if (!CreateProcessA(
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
        sprintf(error_message, "启动程序失败，错误码：%lu",
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
