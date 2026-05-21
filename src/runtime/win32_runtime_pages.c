#include "win32_runtime.h"
#include "win32_i18n.h"

#include <commdlg.h>
#include <objbase.h>
#include <shlobj.h>

#include <stdio.h>

static void rt_progress_callback(const OsInstallProgressEvent *event, void *user_data)
{
    (void)user_data;

    if (event == NULL || !g_rt.progress_lock_ready) {
        return;
    }

    EnterCriticalSection(&g_rt.progress_lock);
    rt_copy(g_rt.progress_action, sizeof(g_rt.progress_action), event->action != NULL ? event->action : "");
    rt_copy(g_rt.progress_source, sizeof(g_rt.progress_source), event->source_path != NULL ? event->source_path : "");
    rt_copy(g_rt.progress_target, sizeof(g_rt.progress_target), event->target_path != NULL ? event->target_path : "");
    g_rt.progress_completed = event->completed_files;
    g_rt.progress_total = event->total_files;
    g_rt.progress_percent = event->percent;
    LeaveCriticalSection(&g_rt.progress_lock);

    PostMessageA(g_rt.window, RT_WM_PROGRESS_UPDATE, 0, 0);
}

static DWORD WINAPI rt_operation_thread(LPVOID param)
{
    (void)param;

    os_set_install_progress_callback(rt_progress_callback, NULL);
    if (g_rt.mode == RT_MODE_INSTALL) {
        g_rt.operation_ok = (g_rt.embedded_package
                                 ? os_install_embedded_package_with_options(g_rt.self_path,
                                                                            g_rt.operation_install_dir,
                                                                            g_rt.online_component_mask,
                                                                            g_rt.result,
                                                                            sizeof(g_rt.result))
                                 : os_install_package_with_options(g_rt.package_dir,
                                                                   g_rt.operation_install_dir,
                                                                   g_rt.online_component_mask,
                                                                   g_rt.result,
                                                                   sizeof(g_rt.result))) == 0;
    } else {
        g_rt.operation_ok = (g_rt.embedded_package
                                 ? os_uninstall_embedded_package(g_rt.self_path,
                                                                 g_rt.command_install_dir,
                                                                 g_rt.result,
                                                                 sizeof(g_rt.result))
                                 : os_uninstall_package(g_rt.package_dir,
                                                        g_rt.command_install_dir,
                                                        g_rt.result,
                                                        sizeof(g_rt.result))) == 0;
    }
    os_set_install_progress_callback(NULL, NULL);

    PostMessageA(g_rt.window, RT_WM_OPERATION_DONE, 0, 0);
    return 0;
}

static void rt_page_text(const char *title, const char *body)
{
    SetWindowTextA(g_rt.title, title);
    SetWindowTextA(g_rt.subtitle, body);
    SetWindowTextA(g_rt.body, body);
}

static void rt_hide_page_controls(void)
{
    size_t i;

    rt_show(g_rt.body, 1);
    rt_show(g_rt.install_dir, 0);
    rt_show(g_rt.browse, 0);
    rt_show(g_rt.license, 0);
    rt_show(g_rt.accept, 0);
    rt_show(g_rt.component_main, 0);
    rt_show(g_rt.component_reg, 0);
    for (i = 0; i < OS_MAX_ONLINE_COMPONENTS; ++i) {
        rt_show(g_rt.online_components[i], 0);
    }
    rt_show(g_rt.progress, 0);
    rt_show(g_rt.progress_text, 0);
    rt_show(g_rt.progress_detail, 0);
    rt_show(g_rt.progress_more, 0);
    rt_show(g_rt.progress_log, 0);
}

static void rt_show_progress_controls(void)
{
    rt_show(g_rt.progress, 1);
    rt_show(g_rt.progress_text, 1);
    rt_show(g_rt.progress_detail, 1);
    rt_show(g_rt.progress_more, 1);
    rt_show(g_rt.progress_log, g_rt.progress_expanded);
    SetWindowTextA(g_rt.progress_more, g_rt.progress_expanded ? "Hide details" : "Details");
}

static void rt_add_page(RtPageKind kind)
{
    if (g_rt.page_count < RT_MAX_PAGES) {
        g_rt.page_kinds[g_rt.page_count++] = kind;
    }
}

void rt_build_page_flow(void)
{
    uint32_t flags;

    g_rt.page = 0;
    g_rt.page_count = 0;

    if (g_rt.mode == RT_MODE_UNINSTALL) {
        rt_add_page(RT_PAGE_UNINSTALL_WELCOME);
        rt_add_page(RT_PAGE_UNINSTALL_READY);
        rt_add_page(RT_PAGE_UNINSTALL_PROGRESS);
        rt_add_page(RT_PAGE_UNINSTALL_FINISH);
        return;
    }

    flags = g_rt.info.page_flags != 0 ? g_rt.info.page_flags : OS_PAGE_DEFAULT;
    if ((flags & OS_PAGE_WELCOME) != 0) {
        rt_add_page(RT_PAGE_WELCOME);
    }
    if (g_rt.has_license && (flags & OS_PAGE_LICENSE) != 0) {
        rt_add_page(RT_PAGE_LICENSE);
    }
    if ((flags & OS_PAGE_FOLDER) != 0) {
        rt_add_page(RT_PAGE_FOLDER);
    }
    if ((flags & OS_PAGE_COMPONENTS) != 0) {
        rt_add_page(RT_PAGE_COMPONENTS);
    }
    if ((flags & OS_PAGE_READY) != 0) {
        rt_add_page(RT_PAGE_READY);
    }
    if (g_rt.page_count == 0) {
        rt_add_page(RT_PAGE_READY);
    }
    rt_add_page(RT_PAGE_PROGRESS);
    if ((flags & OS_PAGE_FINISH) != 0) {
        rt_add_page(RT_PAGE_FINISH);
    }
}

RtPageKind rt_current_page_kind(void)
{
    if (g_rt.page < 0 || g_rt.page >= g_rt.page_count) {
        return g_rt.mode == RT_MODE_UNINSTALL ? RT_PAGE_UNINSTALL_FINISH : RT_PAGE_FINISH;
    }

    return g_rt.page_kinds[g_rt.page];
}

int rt_find_page_kind(RtPageKind kind)
{
    int i;

    for (i = 0; i < g_rt.page_count; ++i) {
        if (g_rt.page_kinds[i] == kind) {
            return i;
        }
    }

    return -1;
}

const char *rt_page_step_name(int index)
{
    if (index < 0 || index >= g_rt.page_count) {
        return "";
    }

    switch (g_rt.page_kinds[index]) {
    case RT_PAGE_WELCOME:
        return "Welcome";
    case RT_PAGE_LICENSE:
        return "License";
    case RT_PAGE_FOLDER:
        return "Location";
    case RT_PAGE_COMPONENTS:
        return "Components";
    case RT_PAGE_READY:
        return "Ready";
    case RT_PAGE_PROGRESS:
        return "Install";
    case RT_PAGE_FINISH:
        return "Finish";
    case RT_PAGE_UNINSTALL_WELCOME:
        return "Uninstall";
    case RT_PAGE_UNINSTALL_READY:
        return "Ready";
    case RT_PAGE_UNINSTALL_PROGRESS:
        return "Remove";
    case RT_PAGE_UNINSTALL_FINISH:
        return "Finish";
    default:
        return "";
    }
}

void rt_set_page(void)
{
    char text[OS_MAX_MESSAGE_LEN + OS_MAX_PATH_LEN];
    RtPageKind kind;
    int has_finish_page;

    if (g_rt.page_count == 0) {
        rt_build_page_flow();
    }
    kind = rt_current_page_kind();
    has_finish_page = g_rt.mode == RT_MODE_UNINSTALL
                          ? rt_find_page_kind(RT_PAGE_UNINSTALL_FINISH) >= 0
                          : rt_find_page_kind(RT_PAGE_FINISH) >= 0;

    rt_hide_page_controls();
    EnableWindow(g_rt.back, g_rt.page > 0 && !g_rt.operation_started);
    EnableWindow(g_rt.next, TRUE);
    EnableWindow(g_rt.cancel, TRUE);
    os_win32_set_window_text(g_rt.next, OS_WIN32_TEXT_NEXT);

    if (g_rt.mode == RT_MODE_UNINSTALL) {
        if (kind == RT_PAGE_UNINSTALL_WELCOME) {
            rt_page_text(g_rt.info.uninstall_title, g_rt.info.uninstall_text);
        } else if (kind == RT_PAGE_UNINSTALL_READY) {
            rt_page_text("Ready to Uninstall", "Setup is ready to remove the application from your computer.");
            snprintf(text, sizeof(text), "The following application will be removed:\r\n\r\n%s", g_rt.info.app_name);
            SetWindowTextA(g_rt.body, text);
        } else if (kind == RT_PAGE_UNINSTALL_PROGRESS) {
            rt_page_text("Removing", "Please wait while setup removes the application.");
            SetWindowTextA(g_rt.body, "Removing files and unregistering the application...");
            rt_show_progress_controls();
            rt_set_progress(g_rt.operation_done ? 100 : g_rt.progress_value,
                            g_rt.operation_done ? "Removal complete." : "Removing files and system registration...");
            SetWindowTextA(g_rt.next,
                           g_rt.operation_done
                               ? (has_finish_page ? os_win32_text(OS_WIN32_TEXT_NEXT) : os_win32_text(OS_WIN32_TEXT_FINISH))
                               : os_win32_text(OS_WIN32_TEXT_PLEASE_WAIT));
            EnableWindow(g_rt.next, g_rt.operation_done);
            EnableWindow(g_rt.back, FALSE);
            EnableWindow(g_rt.cancel, g_rt.operation_done);
        } else {
            rt_page_text("Uninstall Complete", g_rt.operation_ok ? "The application has been removed." : "The application could not be removed.");
            SetWindowTextA(g_rt.body, g_rt.result);
            os_win32_set_window_text(g_rt.next, OS_WIN32_TEXT_FINISH);
            EnableWindow(g_rt.back, FALSE);
        }
        if (rt_modern_style_enabled()) {
            rt_modern_layout(g_rt.window);
        }
        if (rt_legacy_style_enabled()) {
            rt_legacy_layout(g_rt.window);
        }
        return;
    }

    if (kind == RT_PAGE_WELCOME) {
        rt_page_text(g_rt.info.welcome_title, g_rt.info.welcome_text);
    } else if (kind == RT_PAGE_LICENSE) {
        rt_page_text("License Agreement", "Please review the license agreement before installing.");
        rt_show(g_rt.body, 0);
        rt_show(g_rt.license, 1);
        rt_show(g_rt.accept, 1);
        EnableWindow(g_rt.next, SendMessageA(g_rt.accept, BM_GETCHECK, 0, 0) == BST_CHECKED);
    } else if (kind == RT_PAGE_FOLDER) {
        rt_page_text(g_rt.info.folder_title, g_rt.info.folder_text);
        rt_show(g_rt.install_dir, 1);
        rt_show(g_rt.browse, 1);
    } else if (kind == RT_PAGE_COMPONENTS) {
        size_t i;

        rt_page_text(g_rt.info.components_title, g_rt.info.components_text);
        rt_show(g_rt.body, 0);
        rt_show(g_rt.component_main, 1);
        rt_show(g_rt.component_reg, 1);
        EnableWindow(g_rt.component_main, FALSE);
        EnableWindow(g_rt.component_reg, FALSE);
        for (i = 0; i < g_rt.info.online_component_count && i < OS_MAX_ONLINE_COMPONENTS; ++i) {
            const OsOnlineComponent *component = &g_rt.info.online_components[i];
            char label[OS_MAX_NAME_LEN + 48];

            if (component->url[0] == '\0') {
                continue;
            }
            snprintf(label,
                     sizeof(label),
                     "%s",
                     component->name[0] != '\0' ? component->name : "Online component");
            SetWindowTextA(g_rt.online_components[i], label);
            SendMessageA(g_rt.online_components[i],
                         BM_SETCHECK,
                         component->selected_by_default ? BST_CHECKED : BST_UNCHECKED,
                         0);
            EnableWindow(g_rt.online_components[i], TRUE);
            rt_show(g_rt.online_components[i], 1);
        }
    } else if (kind == RT_PAGE_READY) {
        char install_dir[OS_MAX_PATH_LEN];
        GetWindowTextA(g_rt.install_dir, install_dir, sizeof(install_dir));
        rt_page_text(g_rt.info.ready_title, g_rt.info.ready_text);
        snprintf(text, sizeof(text), "%s\r\n\r\nDestination folder:\r\n%s", g_rt.info.ready_text, install_dir);
        SetWindowTextA(g_rt.body, text);
        os_win32_set_window_text(g_rt.next, OS_WIN32_TEXT_INSTALL);
    } else if (kind == RT_PAGE_PROGRESS) {
        rt_page_text("Installing", "Please wait while setup installs the application.");
        SetWindowTextA(g_rt.body, "Installing files and writing uninstall metadata...");
        rt_show_progress_controls();
        rt_set_progress(g_rt.operation_done ? 100 : g_rt.progress_value,
                        g_rt.operation_done ? "Installation complete." : "Preparing embedded package...");
        SetWindowTextA(g_rt.next,
                       g_rt.operation_done
                           ? (has_finish_page ? os_win32_text(OS_WIN32_TEXT_NEXT) : os_win32_text(OS_WIN32_TEXT_FINISH))
                           : os_win32_text(OS_WIN32_TEXT_PLEASE_WAIT));
        EnableWindow(g_rt.next, g_rt.operation_done);
        EnableWindow(g_rt.back, FALSE);
        EnableWindow(g_rt.cancel, g_rt.operation_done);
    } else {
        rt_page_text(g_rt.info.finish_title, g_rt.operation_ok ? g_rt.info.finish_text : "Installation failed.");
        SetWindowTextA(g_rt.body, g_rt.result);
        os_win32_set_window_text(g_rt.next, OS_WIN32_TEXT_FINISH);
        EnableWindow(g_rt.back, FALSE);
    }

    if (rt_modern_style_enabled()) {
        rt_modern_layout(g_rt.window);
    }
    if (rt_legacy_style_enabled()) {
        rt_legacy_layout(g_rt.window);
    }
}

void rt_pick_folder(void)
{
    BROWSEINFOA browse;
    PIDLIST_ABSOLUTE item;
    char path[OS_MAX_PATH_LEN];

    memset(&browse, 0, sizeof(browse));
    browse.hwndOwner = g_rt.window;
    browse.lpszTitle = "Select installation folder";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

    item = SHBrowseForFolderA(&browse);
    if (item == NULL) {
        return;
    }

    if (SHGetPathFromIDListA(item, path)) {
        SetWindowTextA(g_rt.install_dir, path);
    }
    CoTaskMemFree(item);
}

void rt_run_operation(void)
{
    HANDLE thread;
    int components_page = rt_find_page_kind(RT_PAGE_COMPONENTS);

    if (g_rt.operation_done || g_rt.operation_started) {
        return;
    }

    g_rt.operation_started = 1;
    g_rt.progress_expanded = 0;
    g_rt.progress_has_real_events = 0;
    g_rt.progress_log_text[0] = '\0';
    SetWindowTextA(g_rt.progress_log, "");
    GetWindowTextA(g_rt.install_dir, g_rt.operation_install_dir, sizeof(g_rt.operation_install_dir));
    g_rt.online_component_mask = 0;
    if (g_rt.mode == RT_MODE_INSTALL) {
        size_t i;

        for (i = 0; i < g_rt.info.online_component_count && i < OS_MAX_ONLINE_COMPONENTS; ++i) {
            int selected = components_page >= 0 && g_rt.online_components[i] != NULL
                               ? SendMessageA(g_rt.online_components[i], BM_GETCHECK, 0, 0) == BST_CHECKED
                               : g_rt.info.online_components[i].selected_by_default;

            if (selected) {
                g_rt.online_component_mask |= ((uint64_t)1u << i);
            }
        }
    }
    if (g_rt.mode == RT_MODE_INSTALL &&
        rt_install_dir_needs_elevation(g_rt.operation_install_dir) &&
        !rt_is_process_elevated()) {
        if (rt_relaunch_elevated_for_install(g_rt.operation_install_dir)) {
            DestroyWindow(g_rt.window);
            return;
        }

        g_rt.operation_ok = 0;
        g_rt.operation_done = 1;
        snprintf(g_rt.result,
                 sizeof(g_rt.result),
                 "Windows elevation was required for this install folder, but the elevated setup could not be started.");
        rt_set_page();
        return;
    }

    g_rt.progress_floor = 8;
    g_rt.progress_ceiling = 92;
    rt_set_progress(g_rt.progress_floor,
                    g_rt.mode == RT_MODE_INSTALL ? "Preparing embedded installer archive..." : "Preparing removal plan...");
    {
        char detail[OS_MAX_PATH_LEN + 40];
        snprintf(detail,
                 sizeof(detail),
                 "Destination:\r\n%s",
                 g_rt.mode == RT_MODE_INSTALL ? g_rt.operation_install_dir : g_rt.command_install_dir);
        SetWindowTextA(g_rt.progress_detail, detail);
    }
    SetTimer(g_rt.window, RT_TIMER_PROGRESS, 80, NULL);

    thread = CreateThread(NULL, 0, rt_operation_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    } else {
        g_rt.operation_ok = 0;
        snprintf(g_rt.result, sizeof(g_rt.result), "Cannot start setup operation.");
        PostMessageA(g_rt.window, RT_WM_OPERATION_DONE, 0, 0);
    }
}

void rt_progress_tick(void)
{
    char text[192];

    if (!g_rt.operation_started || g_rt.operation_done) {
        return;
    }

    if (g_rt.progress_has_real_events) {
        return;
    }

    if (g_rt.progress_value < g_rt.progress_ceiling) {
        int step;

        if (g_rt.info.file_count > 200) {
            step = g_rt.progress_value < 70 ? 1 : 0;
        } else if (g_rt.info.file_count > 40) {
            step = g_rt.progress_value < 50 ? 2 : 1;
        } else {
            step = g_rt.progress_value < 45 ? 3 : g_rt.progress_value < 75 ? 2 : 1;
        }

        if (step < 1) {
            step = 1;
        }
        g_rt.progress_value += step;
        if (g_rt.progress_value > g_rt.progress_ceiling) {
            g_rt.progress_value = g_rt.progress_ceiling;
        }
    }

    if (g_rt.mode == RT_MODE_INSTALL) {
        if (g_rt.progress_value < 25) {
            snprintf(text, sizeof(text), "Reading %s package identity...", g_rt.info.app_name);
        } else if (g_rt.progress_value < 58) {
            if (g_rt.info.file_count > 0) {
                snprintf(text,
                         sizeof(text),
                         "Extracting %llu embedded file(s)...",
                         (unsigned long long)g_rt.info.file_count);
            } else {
                snprintf(text, sizeof(text), "Extracting embedded files...");
            }
        } else if (g_rt.progress_value < 82) {
            if (g_rt.info.company_name[0] != '\0') {
                snprintf(text,
                         sizeof(text),
                         "Registering publisher: %s...",
                         g_rt.info.company_name);
            } else {
                snprintf(text, sizeof(text), "Writing uninstaller and system metadata...");
            }
        } else {
            snprintf(text, sizeof(text), "Finalizing %s Setup...", g_rt.info.app_name);
        }
    } else {
        if (g_rt.progress_value < 35) {
            snprintf(text, sizeof(text), "Finding installed %s files...", g_rt.info.app_name);
        } else if (g_rt.progress_value < 70) {
            if (g_rt.info.file_count > 0) {
                snprintf(text,
                         sizeof(text),
                         "Removing %llu installed file(s)...",
                         (unsigned long long)g_rt.info.file_count);
            } else {
                snprintf(text, sizeof(text), "Removing application files...");
            }
        } else {
            snprintf(text, sizeof(text), "Cleaning system registration...");
        }
    }

    rt_set_progress(g_rt.progress_value, text);
}

void rt_operation_done(void)
{
    KillTimer(g_rt.window, RT_TIMER_PROGRESS);
    g_rt.operation_done = 1;
    rt_set_progress(100, g_rt.operation_ok ? "Complete." : "Stopped with an error.");
    rt_set_page();
}

void rt_next(void)
{
    RtPageKind kind = rt_current_page_kind();
    int install_finish = rt_find_page_kind(RT_PAGE_FINISH);
    int uninstall_finish = rt_find_page_kind(RT_PAGE_UNINSTALL_FINISH);

    if (kind == RT_PAGE_FINISH || kind == RT_PAGE_UNINSTALL_FINISH) {
        DestroyWindow(g_rt.window);
        return;
    }

    if ((kind == RT_PAGE_PROGRESS || kind == RT_PAGE_UNINSTALL_PROGRESS) && !g_rt.operation_done) {
        return;
    }

    if (kind == RT_PAGE_PROGRESS && g_rt.operation_done && install_finish < 0) {
        DestroyWindow(g_rt.window);
        return;
    }
    if (kind == RT_PAGE_UNINSTALL_PROGRESS && g_rt.operation_done && uninstall_finish < 0) {
        DestroyWindow(g_rt.window);
        return;
    }
    if (g_rt.page + 1 >= g_rt.page_count) {
        DestroyWindow(g_rt.window);
        return;
    }

    g_rt.page++;
    rt_set_page();

    kind = rt_current_page_kind();
    if (kind == RT_PAGE_PROGRESS || kind == RT_PAGE_UNINSTALL_PROGRESS) {
        rt_run_operation();
        rt_set_page();
    }
}

void rt_back(void)
{
    if (g_rt.page > 0 && !g_rt.operation_started) {
        g_rt.page--;
        rt_set_page();
    }
}
