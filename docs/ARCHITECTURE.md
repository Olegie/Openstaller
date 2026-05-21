# Openstaller Architecture

Openstaller is split into three planes:

1. **Control plane**: the C frontends collect user intent through CLI arguments
   or a native Win32 interface.
2. **Generation plane**: the C core walks the payload, writes manifests,
   creates optional fallback scripts, and seals the native EXE archive.
3. **Identity plane**: the Assembly kernel folds byte streams into deterministic
   package identifiers through a stable C-callable ABI.

This keeps platform integration and memory ownership in C while assigning the
byte-level identity loop to Assembly, where the code is small, explicit, and
mechanically testable.

## Deterministic Package Identity

The payload hash is built from:

- normalized relative file paths
- raw file bytes
- application name
- company/publisher name
- application version
- default install directory
- page-flow and theme settings
- online component metadata

The resulting installer ID is used by native uninstall registration and optional
fallback scripts. A package therefore has a stable identity that changes when
either the payload, installation metadata, or optional online component plan
changes.

## Generated Bundle

For an application named `Example App` with version `1.0.0`, Openstaller emits:

```text
dist/example-app-1.0.0/
  installer.exe
```

The default package is self-contained: payload, manifest, license text, optional
side-panel artwork, optional page-background artwork, and the native
uninstaller executable are embedded into `installer.exe`. During installation,
the embedded uninstaller is extracted into the installed application folder and
registration metadata points to that installed copy. If fallback scripts are
enabled, Openstaller also keeps the staged `payload/`, `payload.files`,
`manifest.openstaller`, script files, optional `LICENSE.txt`, and optional
standalone native uninstaller beside the installer.

On Windows, the generated installer and embedded uninstaller receive icon
resources and product-specific version resources before Openstaller seals the
embedded archive. If the user provides `.ico` files, those are used. If not,
Openstaller writes built-in transparent installer and uninstaller icons so the
setup and installed removal executable are visually distinct in Explorer and
the title bar. Generated version metadata uses the target application name,
version, company, and setup role instead of reporting only the Openstaller
runtime identity.

Embedded archives use a versioned footer/header format. Version 1 archives
remain readable. Version 2 entries can mark payload files as deflate-compressed
when zlib is available at build time; non-payload resources remain stored
plainly so metadata can be read without decompressing the whole package.

Wizard artwork is stored as separate non-payload resources. The side image is
stretched into the left rail, while the page-background image is stretched under
the installer body area to reproduce the older setup-wizard style without
placing user files beside the generated EXE.

The manifest also carries generated UI settings: installer style, UI font,
window behavior, page flow, and theme colors. `classic` is the default wizard
runtime: compact window, side rail, normal page flow. `modern` switches the
Win32 runtime into a separate sidebar/card layout module with a clean content
area, native resizable window, selected font face, and circular progress
visualization fed by the same structured progress events. `legacy` switches
into a separate full-screen layout module with a large product title, optional
full-window background art, old setup-style meter decoration, and a
bottom-right progress panel. The three style modules remain the layout base;
theme fields only recolor accent, progress, sidebars, panels, text, and legacy
gradient colors.

Online install components are kept out of the main install file path. Their
manifest fields are parsed into `OsOnlineComponent` records, the runtime exposes
them as selectable component checkboxes, and `src/core/openstaller_online.c`
owns URL normalization, safe relative target validation, downloading, and the
final rollback-protected file move. Windows uses WinINet for direct HTTP/HTTPS
URLs, including redirected Dropbox, Hugging Face, and GitHub release links.
Unix-like builds use `curl` through `fork`/`exec` when an online component is
selected.

On Unix-like builds the native launcher is named `installer`, and the embedded
uninstaller is written into the install tree as `uninstaller`. The script files
remain inspectable fallback artifacts when requested.

## Registration Semantics

Windows registration targets:

```text
HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\<safe-name>-<installer-id>
```

Windows Start Menu shortcuts are written into the current user's Programs
folder. If a launcher path is configured, Openstaller creates an application
shortcut beside an uninstall shortcut. The uninstall shortcut points to the
uninstaller extracted into the installed application folder. Both shortcuts are
removed by the generated uninstaller.

Linux registration targets:

```text
$XDG_DATA_HOME/openstaller/<safe-name>-<installer-id>
$XDG_DATA_HOME/applications/<safe-name>-<installer-id>.desktop
```

If `XDG_DATA_HOME` is not set, Openstaller falls back to
`$HOME/.local/share`. macOS uses the user's Library and Applications folders:

```text
$HOME/Library/Application Support/Openstaller/<safe-name>-<installer-id>
$HOME/Applications/<safe-name>-<installer-id>.command
$HOME/Applications/<safe-name>-<installer-id>-uninstall.command
```

The generated uninstaller removes the same metadata it creates.

Install operations maintain a rollback journal while files are being written.
When a payload file replaces an existing file, the original file is copied to a
temporary rollback store first. If extraction or registration fails, Openstaller
restores replaced files and removes newly created files instead of deleting the
whole target directory blindly. Online components join the same journal: the
download is first written to a temporary file, then moved into the install tree
only after the target path has been captured for rollback.

The install path also emits structured progress events through the public C
callback hook. The Win32 runtime uses those events to show the active operation,
the current payload file, the destination path, exact file counts, and an
expandable activity log instead of relying only on a synthetic timer.

Native installers and uninstallers perform the same registration operations from
compiled C code. Windows writes uninstall metadata through the registry API.
Linux writes removal metadata through XDG data paths, while macOS writes it
under `~/Library/Application Support/Openstaller`.
When company branding is provided, it is stored in the manifest, shown by the
wizard, included in the package identity, and written as the Windows `Publisher`
or Unix registration `company` value.

## Legacy Windows Mode

`OPENSTALLER_WIN2000_COMPAT=ON` builds Windows targets against a Windows 2000
API floor. The mode exists for users who need old Win32 compatibility, not for
the default release profile.

The compatibility mode sets the Win32 version macros to `0x0500`, requests a
Windows subsystem version of `5.00`, disables UAC relaunch behavior, and avoids
newer registry helpers such as `RegDeleteTreeA`. Uninstall key removal falls
back to recursive deletion built from `RegOpenKeyExA`, `RegEnumKeyExA`, and
`RegDeleteKeyA`.

Actual Windows 2000 runtime support still depends on the compiler runtime. A
32-bit legacy-capable MinGW/MSVCRT toolchain is the intended path for that
target; modern MSVC runtimes should be treated as build-host tooling rather
than proof of Windows 2000 runtime compatibility.

## Security Posture

Openstaller does not pretend that generated shell scripts are a sandbox. The
security model is intentionally inspectable:

- fallback scripts are plain text when requested
- payload file hashes are written into `payload.files` for fallback packages
- package identity is reproducible
- online component targets cannot escape the install directory
- license acceptance is explicit
- uninstall registration is scoped to the current user by default

Future hardening can add signature verification, per-file uninstall manifests,
and archive emitters without changing the public C/Assembly identity contract.
