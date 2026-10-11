# Wind Launcher

The launcher of the [Wind engine](https://github.com/wismh/wind-engine): it lists your Wind projects and the editor SDKs installed on your machine, makes a new project from the SDK's template, and opens a project in the editor of the engine version it names. It is itself a Wind game project (`wind_project.toml`, `find_package(Wind)`, `engine_add_game`), built against a released SDK.

## Download

These links always point to the newest launcher (the `latest` release is rebuilt on every release; the version-less names stay the same):

| System | Download |
| --- | --- |
| Windows (x86_64) | [wind-launcher-windows-x86_64.zip](https://github.com/wismh/wind-laucnher/releases/latest/download/wind-launcher-windows-x86_64.zip) |
| Linux (x86_64) | [wind-launcher-linux-x86_64.tar.gz](https://github.com/wismh/wind-laucnher/releases/latest/download/wind-launcher-linux-x86_64.tar.gz) |
| macOS (Apple silicon, arm64) | [wind-launcher-macos-arm64.tar.gz](https://github.com/wismh/wind-laucnher/releases/latest/download/wind-launcher-macos-arm64.tar.gz) |

Every launcher version is also a release of its own, `v<major>.<minor>.<patch><stage>` (`a` alpha, `b` beta, none for production), on the [Releases](https://github.com/wismh/wind-laucnher/releases) page, with a `.sha256` file for each archive. The launcher versions are independent of the engine versions. Unpack the archive anywhere and run `wind_launcher` (`wind_launcher.exe`); `assets/` has to stay beside it.

Supported: Windows 10 and later (x86_64), Linux x86_64 with a desktop and OpenGL, macOS 14 and later on Apple silicon.

### Not signed

The launcher is not code-signed or notarized yet:

- **macOS**: Gatekeeper says the app cannot be opened because the developer cannot be verified. Right-click `wind_launcher` and choose **Open**, then **Open** again (or `xattr -d com.apple.quarantine wind_launcher` in the unpacked folder).
- **Windows**: SmartScreen shows "Windows protected your PC". Choose **More info**, then **Run anyway**.

## Where things are stored

| What | Windows | Linux | macOS |
| --- | --- | --- | --- |
| Installed SDKs | `%LOCALAPPDATA%\Programs\Wind\Sdks\<name>` | `$XDG_DATA_HOME/Wind/Sdks` or `~/.local/share/Wind/Sdks` | `~/.local/share/Wind/Sdks` |
| New projects (default folder) | `%USERPROFILE%\WindProjects` | `~/WindProjects` | `~/WindProjects` |
| The launcher's own lists (projects, located SDKs) | the user data directory `Wind/Launcher/launcher.txt` of the operating system | | |

An SDK anywhere else can be added with Locate on the SDKs page.

## Build from source

You need CMake 3.25 or newer, a C++23 compiler (Visual Studio 2026, GCC 13 or newer, or Apple Clang of Xcode 15 or newer), and a released Wind SDK of the version named in `wind_project.toml` (`engine = "..."`). Download the SDK for your system from the [engine releases](https://github.com/wismh/wind-engine/releases) and unpack it. The SDK and the launcher have to be built with the same compiler.

```sh
cmake -S . -B build -G "Ninja Multi-Config" -DWIND_EXPORT=ON \
  -DCMAKE_PREFIX_PATH=/path/to/wind-sdk -DWind_DIR=/path/to/wind-sdk/cmake -DCMAKE_CONFIGURATION_TYPES=Release
cmake --build build --config Release
ctest --test-dir build -C Release          # on Linux under xvfb-run, with SDL_AUDIODRIVER=dummy
build/bin/Release/wind_launcher
```

On Windows use `-G "Visual Studio 18 2026"` instead of Ninja. On Linux the SDK needs the same development packages as the engine (X11, Wayland, ALSA, Mesa; the list is in `.github/actions/build/action.yml`). The tests need no network and no display other than the virtual one.

## Releases

Every push to `main` builds, tests, and publishes a release (`.github/workflows/release.yml`). The version is `version.txt` (`0.1a`: major 0, minor 1, alpha) plus a patch counted from the existing tags. `ci.yml` builds and tests every push and pull request on the three systems. `gh workflow run release.yml --ref <branch> -f publish=false` is a dry run that publishes nothing.
