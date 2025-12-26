# ScreenFilter
* A useless application that limits your screen colors
* A fork of the [dwm_lut](https://github.com/lauralex/dwm_lut) which is a fork of the [original dwm_lut](https://github.com/ledoge/dwm_lut)

## Dependencies
- Visual C++ runtime (https://www.techpowerup.com/download/visual-c-redistributable-runtime-package-all-in-one/)

# About
This tool applies color-limiting filters to the Windows desktop by hooking into DWM. It works in both SDR and ~~HDR modes~~ (not really great).

Right now, it should work on any 20H2 or 21H1 build of Windows 10, and also the current build of Windows 11, and I'll try to update it whenever a new version breaks it.

# Usage
Run `DwmLutGUI.exe` and click Apply.

Minimizing the GUI will make it disappear from the taskbar, and you can use the context menu of the tray icon to quickly apply or disable all LUTs. For automation, you can start the exe with any (sensible) combination of `-apply`,  `-disable`, `-minimize` and `-exit` as arguments.

Note: DirectFlip and MPO get force disabled on monitors with an active filter. These features are designed to improve performance for some windowed applications by allowing them to bypass DWM (and therefore also the LUT). This ensures that screen filters get applied properly to all applications (except exclusive fullscreen ones).

# Compiling
Install [vcpkg](https://vcpkg.io/en/getting-started.html) for C++ dependency management:

- Create and switch to your desired install folder (e.g. _%LOCALAPPDATA%\vcpkg_)
- `git clone https://github.com/Microsoft/vcpkg.git .`
- `.\bootstrap-vcpkg.bat`
- `vcpkg integrate install`

Just open the projects in Visual Studio and compile a x64 Release build.
