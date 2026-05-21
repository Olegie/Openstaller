#include "openstaller_scripts_internal.h"

int os_scripts_write_windows(const OsProjectConfig *config,
                             const char *package_dir,
                             const char *safe_name,
                             uint64_t installer_id)
{
    char install_path[OS_MAX_PATH_LEN];
    char uninstall_path[OS_MAX_PATH_LEN];
    FILE *file;

    if (scripts_join(install_path, sizeof(install_path), package_dir, "install.bat") != 0 ||
        scripts_join(uninstall_path, sizeof(uninstall_path), package_dir, "uninstall.bat") != 0) {
        return -1;
    }

    file = fopen(install_path, "wb");
    if (file == NULL) {
        return -1;
    }

    fputs("@echo off\r\n"
          "setlocal EnableExtensions\r\n"
          "set \"SCRIPT_DIR=%~dp0\"\r\n"
          "set \"SOURCE_DIR=%SCRIPT_DIR%payload\"\r\n"
          "set \"APP_NAME=", file);
    scripts_write_bat_value(file, config->app_name);
    fputs("\"\r\nset \"APP_PUBLISHER=", file);
    scripts_write_bat_value(file, config->company_name[0] != '\0' ? config->company_name : config->app_name);
    fputs("\"\r\nset \"APP_VERSION=", file);
    scripts_write_bat_value(file, config->app_version);
    fputs("\"\r\nset \"INSTALL_DIR=", file);
    scripts_write_bat_value(file, config->install_dir);
    fputs("\"\r\nif not \"%~1\"==\"\" set \"INSTALL_DIR=%~1\"\r\n"
          "if \"%INSTALL_DIR%\"==\"\" exit /b 4\r\n"
          "if not exist \"%SOURCE_DIR%\" exit /b 5\r\n", file);

    if (config->include_license) {
        fputs("if exist \"%SCRIPT_DIR%LICENSE.txt\" (\r\n"
              "  type \"%SCRIPT_DIR%LICENSE.txt\"\r\n"
              "  echo.\r\n"
              "  set /p OPENSTALLER_ACCEPT=Type YES to accept the license: \r\n"
              "  if /I not \"%OPENSTALLER_ACCEPT%\"==\"YES\" exit /b 9\r\n"
              ")\r\n", file);
    }

    fputs("if not exist \"%INSTALL_DIR%\" mkdir \"%INSTALL_DIR%\"\r\n"
          "xcopy \"%SOURCE_DIR%\\*\" \"%INSTALL_DIR%\\\" /E /I /Y >nul\r\n"
          "if errorlevel 1 exit /b 10\r\n", file);

    if (config->register_system) {
        fputs("set \"UNINSTALL_KEY=HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\", file);
        scripts_write_bat_value(file, safe_name);
        fprintf(file, "-%016llx\"\r\n", (unsigned long long)installer_id);
        fputs("reg add \"%UNINSTALL_KEY%\" /v DisplayName /t REG_SZ /d \"%APP_NAME%\" /f >nul\r\n"
              "reg add \"%UNINSTALL_KEY%\" /v DisplayVersion /t REG_SZ /d \"%APP_VERSION%\" /f >nul\r\n"
              "reg add \"%UNINSTALL_KEY%\" /v InstallLocation /t REG_SZ /d \"%INSTALL_DIR%\" /f >nul\r\n"
              "reg add \"%UNINSTALL_KEY%\" /v Publisher /t REG_SZ /d \"%APP_PUBLISHER%\" /f >nul\r\n"
              "reg add \"%UNINSTALL_KEY%\" /v UninstallString /t REG_SZ /d \"\\\"%SCRIPT_DIR%uninstall.bat\\\" \\\"%INSTALL_DIR%\\\"\" /f >nul\r\n", file);
    }

    fputs("echo Installed %APP_NAME% to %INSTALL_DIR%\r\n"
          "exit /b 0\r\n", file);
    fclose(file);

    file = fopen(uninstall_path, "wb");
    if (file == NULL) {
        return -1;
    }

    fputs("@echo off\r\n"
          "setlocal EnableExtensions\r\n"
          "set \"APP_NAME=", file);
    scripts_write_bat_value(file, config->app_name);
    fputs("\"\r\nset \"INSTALL_DIR=", file);
    scripts_write_bat_value(file, config->install_dir);
    fputs("\"\r\nif not \"%~1\"==\"\" set \"INSTALL_DIR=%~1\"\r\n"
          "if \"%INSTALL_DIR%\"==\"\" exit /b 4\r\n"
          "if \"%INSTALL_DIR%\"==\"\\\" exit /b 4\r\n"
          "if exist \"%INSTALL_DIR%\" rmdir /S /Q \"%INSTALL_DIR%\"\r\n", file);

    if (config->register_system) {
        fputs("set \"UNINSTALL_KEY=HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\", file);
        scripts_write_bat_value(file, safe_name);
        fprintf(file, "-%016llx\"\r\n", (unsigned long long)installer_id);
        fputs("reg delete \"%UNINSTALL_KEY%\" /f >nul 2>nul\r\n", file);
    }

    fputs("echo Uninstalled %APP_NAME%\r\n"
          "exit /b 0\r\n", file);
    fclose(file);
    return 0;
}
