#include "openstaller_scripts_internal.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

int os_scripts_write_unix(const OsProjectConfig *config,
                          const char *package_dir,
                          const char *safe_name,
                          uint64_t installer_id)
{
    char install_path[OS_MAX_PATH_LEN];
    char uninstall_path[OS_MAX_PATH_LEN];
    FILE *file;

    if (scripts_join(install_path, sizeof(install_path), package_dir, "install.sh") != 0 ||
        scripts_join(uninstall_path, sizeof(uninstall_path), package_dir, "uninstall.sh") != 0) {
        return -1;
    }

    file = fopen(install_path, "wb");
    if (file == NULL) {
        return -1;
    }

    fputs("#!/usr/bin/env sh\n"
          "set -eu\n"
          "SCRIPT_DIR=$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd)\n"
          "SOURCE_DIR=\"$SCRIPT_DIR/payload\"\n"
          "APP_NAME=", file);
    scripts_write_sh_single(file, config->app_name);
    fputs("\nAPP_COMPANY=", file);
    scripts_write_sh_single(file, config->company_name);
    fputs("\nAPP_VERSION=", file);
    scripts_write_sh_single(file, config->app_version);
    fputs("\nINSTALL_DIR=", file);
    scripts_write_sh_single(file, config->install_dir);
    fprintf(file, "\nAPP_KEY='%s-%016llx'\nAPP_LAUNCHER=", safe_name, (unsigned long long)installer_id);
    scripts_write_sh_single(file, config->launcher);
    fputs("\nif [ \"${1:-}\" != \"\" ]; then INSTALL_DIR=$1; fi\n"
          "case \"$INSTALL_DIR\" in\n"
          "  \"~/\"*) INSTALL_DIR=\"$HOME/${INSTALL_DIR#~/}\" ;;\n"
          "  \"\\$HOME/\"*) INSTALL_DIR=\"$HOME/${INSTALL_DIR#\\$HOME/}\" ;;\n"
          "esac\n"
          "UNAME=$(uname -s 2>/dev/null || printf '%s' unknown)\n"
          "if [ \"$UNAME\" = \"Darwin\" ]; then\n"
          "  OPENSTALLER_PLATFORM=macos\n"
          "  REG_ROOT=\"$HOME/Library/Application Support/Openstaller\"\n"
          "  APP_SHORTCUT_DIR=\"$HOME/Applications\"\n"
          "else\n"
          "  OPENSTALLER_PLATFORM=linux\n"
          "  DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}\n"
          "  REG_ROOT=\"$DATA_HOME/openstaller\"\n"
          "  APP_SHORTCUT_DIR=\"$DATA_HOME/applications\"\n"
          "fi\n"
          "if [ \"$INSTALL_DIR\" = \"\" ] || [ \"$INSTALL_DIR\" = \"/\" ]; then exit 4; fi\n"
          "if [ ! -d \"$SOURCE_DIR\" ]; then exit 5; fi\n", file);

    if (config->include_license) {
        fputs("if [ -f \"$SCRIPT_DIR/LICENSE.txt\" ]; then\n"
              "  cat \"$SCRIPT_DIR/LICENSE.txt\"\n"
              "  printf '\\nType YES to accept the license: '\n"
              "  read OPENSTALLER_ACCEPT\n"
              "  [ \"$OPENSTALLER_ACCEPT\" = \"YES\" ] || exit 9\n"
              "fi\n", file);
    }

    fputs("mkdir -p \"$INSTALL_DIR\"\n"
          "cp -R \"$SOURCE_DIR/.\" \"$INSTALL_DIR/\"\n", file);

    if (config->register_system) {
        fputs("REG_DIR=\"$REG_ROOT/$APP_KEY\"\n"
              "mkdir -p \"$REG_DIR\"\n"
              "DESKTOP_ENTRY=\n"
              "LAUNCH_COMMAND=\n"
              "UNINSTALL_COMMAND=\n"
              "if [ \"$OPENSTALLER_PLATFORM\" = \"macos\" ]; then\n"
              "  mkdir -p \"$APP_SHORTCUT_DIR\"\n"
              "  UNINSTALL_COMMAND=\"$APP_SHORTCUT_DIR/$APP_KEY-uninstall.command\"\n"
              "  cat > \"$UNINSTALL_COMMAND\" <<OPENSTALLER_UNINSTALL_COMMAND\n"
              "#!/bin/sh\n"
              "exec \"$SCRIPT_DIR/uninstall.sh\" \"$INSTALL_DIR\"\n"
              "OPENSTALLER_UNINSTALL_COMMAND\n"
              "  chmod +x \"$UNINSTALL_COMMAND\"\n"
              "  if [ \"$APP_LAUNCHER\" != \"\" ]; then\n"
              "    LAUNCH_COMMAND=\"$APP_SHORTCUT_DIR/$APP_KEY.command\"\n"
              "    cat > \"$LAUNCH_COMMAND\" <<OPENSTALLER_LAUNCH_COMMAND\n"
              "#!/bin/sh\n"
              "exec \"$INSTALL_DIR/$APP_LAUNCHER\" \"\\$@\"\n"
              "OPENSTALLER_LAUNCH_COMMAND\n"
              "    chmod +x \"$LAUNCH_COMMAND\"\n"
              "  fi\n"
              "elif [ \"$APP_LAUNCHER\" != \"\" ]; then\n"
              "  mkdir -p \"$APP_SHORTCUT_DIR\"\n"
              "  DESKTOP_ENTRY=\"$APP_SHORTCUT_DIR/$APP_KEY.desktop\"\n"
              "  cat > \"$DESKTOP_ENTRY\" <<OPENSTALLER_DESKTOP\n"
              "[Desktop Entry]\n"
              "Type=Application\n"
              "Name=$APP_NAME\n"
              "Exec=\"$INSTALL_DIR/$APP_LAUNCHER\"\n"
              "Terminal=false\n"
              "Categories=Utility;\n"
              "X-Openstaller-Package=$APP_KEY\n"
              "X-Openstaller-Publisher=$APP_COMPANY\n"
              "OPENSTALLER_DESKTOP\n"
              "  chmod 0644 \"$DESKTOP_ENTRY\"\n"
              "  command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database \"$APP_SHORTCUT_DIR\" >/dev/null 2>&1 || true\n"
              "fi\n"
              "cat > \"$REG_DIR/manifest\" <<OPENSTALLER_MANIFEST\n"
              "platform=$OPENSTALLER_PLATFORM\n"
              "name=$APP_NAME\n"
              "company=$APP_COMPANY\n"
              "version=$APP_VERSION\n"
              "install_dir=$INSTALL_DIR\n"
              "uninstall=$SCRIPT_DIR/uninstall.sh\n"
              "desktop_entry=$DESKTOP_ENTRY\n"
              "launch_command=$LAUNCH_COMMAND\n"
              "uninstall_command=$UNINSTALL_COMMAND\n"
              "OPENSTALLER_MANIFEST\n", file);
    }

    fputs("printf 'Installed %s to %s\\n' \"$APP_NAME\" \"$INSTALL_DIR\"\n", file);
    fclose(file);

    file = fopen(uninstall_path, "wb");
    if (file == NULL) {
        return -1;
    }

    fputs("#!/usr/bin/env sh\n"
          "set -eu\n"
          "APP_NAME=", file);
    scripts_write_sh_single(file, config->app_name);
    fputs("\nINSTALL_DIR=", file);
    scripts_write_sh_single(file, config->install_dir);
    fprintf(file, "\nAPP_KEY='%s-%016llx'\n", safe_name, (unsigned long long)installer_id);
    fputs("if [ \"${1:-}\" != \"\" ]; then INSTALL_DIR=$1; fi\n"
          "case \"$INSTALL_DIR\" in\n"
          "  \"~/\"*) INSTALL_DIR=\"$HOME/${INSTALL_DIR#~/}\" ;;\n"
          "  \"\\$HOME/\"*) INSTALL_DIR=\"$HOME/${INSTALL_DIR#\\$HOME/}\" ;;\n"
          "esac\n"
          "UNAME=$(uname -s 2>/dev/null || printf '%s' unknown)\n"
          "if [ \"$UNAME\" = \"Darwin\" ]; then\n"
          "  OPENSTALLER_PLATFORM=macos\n"
          "  REG_ROOT=\"$HOME/Library/Application Support/Openstaller\"\n"
          "  APP_SHORTCUT_DIR=\"$HOME/Applications\"\n"
          "else\n"
          "  OPENSTALLER_PLATFORM=linux\n"
          "  DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}\n"
          "  REG_ROOT=\"$DATA_HOME/openstaller\"\n"
          "  APP_SHORTCUT_DIR=\"$DATA_HOME/applications\"\n"
          "fi\n"
          "if [ \"$INSTALL_DIR\" = \"\" ] || [ \"$INSTALL_DIR\" = \"/\" ]; then exit 4; fi\n"
          "rm -rf -- \"$INSTALL_DIR\"\n", file);

    if (config->register_system) {
        fputs("rm -rf -- \"$REG_ROOT/$APP_KEY\"\n"
              "if [ \"$OPENSTALLER_PLATFORM\" = \"macos\" ]; then\n"
              "  rm -f -- \"$APP_SHORTCUT_DIR/$APP_KEY.command\"\n"
              "  rm -f -- \"$APP_SHORTCUT_DIR/$APP_KEY-uninstall.command\"\n"
              "else\n"
              "  rm -f -- \"$APP_SHORTCUT_DIR/$APP_KEY.desktop\"\n"
              "  command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database \"$APP_SHORTCUT_DIR\" >/dev/null 2>&1 || true\n"
              "fi\n", file);
    }

    fputs("printf 'Uninstalled %s\\n' \"$APP_NAME\"\n", file);
    fclose(file);

#if !defined(_WIN32)
    chmod(install_path, 0755);
    chmod(uninstall_path, 0755);
#endif

    return 0;
}
