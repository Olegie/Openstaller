#include "openstaller/openstaller.h"

#include <string.h>

static int config_copy(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    if (dst_size == 0) {
        return -1;
    }

    len = src == NULL ? 0 : strlen(src);
    if (len >= dst_size) {
        dst[0] = '\0';
        return -1;
    }

    if (src == NULL) {
        dst[0] = '\0';
    } else {
        memcpy(dst, src, len + 1);
    }
    return 0;
}

void os_config_init(OsProjectConfig *config)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config_copy(config->app_name, sizeof(config->app_name), "Openstaller App");
    config_copy(config->company_name, sizeof(config->company_name), "");
    config_copy(config->app_version, sizeof(config->app_version), "0.1.0");
    config_copy(config->install_dir, sizeof(config->install_dir), "OpenstallerApp");
    config_copy(config->ui_font, sizeof(config->ui_font), "");
    config_copy(config->theme.accent, sizeof(config->theme.accent), "#0078D4");
    config_copy(config->theme.progress, sizeof(config->theme.progress), "#0078D4");
    config_copy(config->theme.sidebar, sizeof(config->theme.sidebar), "#154E9E");
    config_copy(config->theme.sidebar_dark, sizeof(config->theme.sidebar_dark), "#082766");
    config_copy(config->theme.background, sizeof(config->theme.background), "#F6F8FB");
    config_copy(config->theme.panel, sizeof(config->theme.panel), "#FFFFFF");
    config_copy(config->theme.text, sizeof(config->theme.text), "#000000");
    config_copy(config->theme.muted_text, sizeof(config->theme.muted_text), "#57606A");
    config_copy(config->theme.legacy_top, sizeof(config->theme.legacy_top), "#0012E8");
    config_copy(config->theme.legacy_bottom, sizeof(config->theme.legacy_bottom), "#000012");
    config_copy(config->welcome_title, sizeof(config->welcome_title), "Welcome to the Setup Wizard");
    config_copy(config->welcome_text,
                sizeof(config->welcome_text),
                "This wizard will guide you through the installation. Click Next to continue.");
    config_copy(config->folder_title, sizeof(config->folder_title), "Choose Install Location");
    config_copy(config->folder_text,
                sizeof(config->folder_text),
                "Choose the folder where the application will be installed.");
    config_copy(config->components_title, sizeof(config->components_title), "Choose Components");
    config_copy(config->components_text,
                sizeof(config->components_text),
                "Select the components you want to install.");
    config_copy(config->ready_title, sizeof(config->ready_title), "Ready to Install");
    config_copy(config->ready_text,
                sizeof(config->ready_text),
                "Setup is ready to begin installing the application.");
    config_copy(config->finish_title, sizeof(config->finish_title), "Installation Complete");
    config_copy(config->finish_text,
                sizeof(config->finish_text),
                "The application has been installed successfully.");
    config_copy(config->uninstall_title, sizeof(config->uninstall_title), "Uninstall Application");
    config_copy(config->uninstall_text,
                sizeof(config->uninstall_text),
                "This wizard will remove the application from your computer.");
    config->include_license = 0;
    config->register_system = 1;
    config->generate_native_exe = 1;
    config->generate_windows = 0;
    config->generate_unix = 0;
    config->installer_style = OS_INSTALLER_STYLE_CLASSIC;
    config->window_style = OS_WINDOW_STYLE_FIXED;
    config->page_flags = OS_PAGE_DEFAULT;
}
