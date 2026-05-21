#include "openstaller/openstaller.h"

#include <stdio.h>
#include <string.h>

static int expect_hash(void)
{
    const unsigned char data[] = "Openstaller";
    uint64_t hash = os_hash_bytes(data, strlen((const char *)data), 14695981039346656037ull);

    if (hash != 0x2d1390a0d90efaeeull) {
        fprintf(stderr, "unexpected hash: %016llx\n", (unsigned long long)hash);
        return 1;
    }

    printf("hash backend: %s\n", os_hash_backend_name());
    return 0;
}

static int expect_defaults(void)
{
    OsProjectConfig config;

    os_config_init(&config);
    if (strcmp(config.app_name, "Openstaller App") != 0) {
        fprintf(stderr, "bad default name\n");
        return 1;
    }

    if (!config.generate_native_exe || config.generate_windows || config.generate_unix || !config.register_system) {
        fprintf(stderr, "bad default switches\n");
        return 1;
    }

    if (config.installer_style != OS_INSTALLER_STYLE_CLASSIC) {
        fprintf(stderr, "bad default installer style\n");
        return 1;
    }

    if (config.window_style != OS_WINDOW_STYLE_FIXED || config.ui_font[0] != '\0') {
        fprintf(stderr, "bad default UI settings\n");
        return 1;
    }

    if (config.page_flags != OS_PAGE_DEFAULT ||
        strcmp(config.theme.accent, "#0078D4") != 0 ||
        strcmp(config.theme.legacy_bottom, "#000012") != 0) {
        fprintf(stderr, "bad default theme or page flow settings\n");
        return 1;
    }

    if (config.online_component_count != 0) {
        fprintf(stderr, "bad default online component settings\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    if (expect_hash() != 0) {
        return 1;
    }

    if (expect_defaults() != 0) {
        return 1;
    }

    return 0;
}
