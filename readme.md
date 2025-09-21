# ScreenFilter
* Inefficient and useless application that limits your screen colors
* Based on [WindowsDesktopDuplicationSample](https://github.com/bmharper/WindowsDesktopDuplicationSample)
* Performance is not that great, but I think it's not 'unusable'
* Run with `/h` to see the command line options
* This is basically the same as VNC client's color depth option. It just filters the colors of the true color desktop screen.
* Won't work with remote desktop softwares that cannot capture `SetWindowDisplayAffinity` protected windows. This includes almost every remote desktop software that isn't RDP.
* Also not visible in screen recorders that use GDI BitBlt, DXGI Desktop Duplication, or Windows Graphics Capture APIs. (OBS Studio, ShareX, Xbox Game Bar, Bandicam, etc.)
* Press Ctrl+Shift+F12, or use the tray icon menu to exit.

## Supported color modes
* True color: No filtering
* High color (16-bit): 5-6-5
* High color (15-bit): 5-5-5
* 256 colors: 3-3-2
* 256 colors with dithering: 3-3-2 with Bayer dithering
* 256 colors with dynamic palette: palette is generated from the current screen content in every 30 frames
* 256 grays: Just use the Windows built-in grayscale filter if you only want this mode
* 64 colors: 2-2-2
* 64 colors with dithering: 2-2-2 with Bayer dithering
* 20 colors
* 20 colors with Windows palette: how non-256-aware Windows apps display in 256-color mode
* 16 colors
* 16 colors with dithering
* 16 grays
* 16 colors with EGA palette
* 16 colors with Windows palette
* 16 colors with Windows palette (alt): adjusted to VMware/86Box VGA palette
* 8 colors
* 8 colors with darker palette: UltraVNC palette
* 8 grays
* 4 grays
* Monochrome: black and white
* Monochrome with dithering: black and white with Bayer dithering
* Inverted monochrome: inverted black and white