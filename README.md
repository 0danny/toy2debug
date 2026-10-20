# Toy2Debug Loader

Toy2Debug is a modern compatibility and patching tool for **Toy Story 2 PC**, built to finally make the game playable on modern Windows systems without hacks, broken menus, or external wrappers.

Expect bugs. The goal is to fix Toy Story 2 once and for all so it can be enjoyed properly on modern PCs.

<p align="center">
<img width="595" height="502" alt="image" src="https://github.com/user-attachments/assets/bff5c6ba-6ab8-4c7b-8aad-292e0f0a85f9" />
</p>

## Features

- Real exclusive fullscreen, windowed, and borderless modes  
- Proper 60 FPS framerate fix  
- Widescreen support  
- Support for resolutions above 1080p including 1440p and 4K  
- Big render distance  
- Disable all cutscenes  
- Skip ESRB and copyright movies  
- Bypass registry key checks  
- Custom resolution selection without the original broken menu  

## Command line

Options override `settings.json` in the working directory. Configuration is saved when launching the game or closing the UI. Repeated options use the last value. `--auto-start` launches without clicking the UI; `--hidden` suppresses the launcher UI and console and implies auto-start. These startup switches are not saved.

```text
loader.exe --hidden --game-path "C:\Games\Toy Story 2\toy2.exe" --width auto --height auto --window-style borderless --bypass-registry-keys true
```

| Option | Value |
| --- | --- |
| `--game-path` | Game executable path; resolved absolute path must fit in 255 bytes |
| `--width`, `--height` | Positive integer or `auto` |
| `--window-style` | `fullscreen`, `windowed`, or `borderless` |
| `--use-32-bit-colors` | `true` or `false` |
| `--correct-framerate` | `true` or `false` |
| `--skip-copyright-esrb` | `true` or `false` |
| `--disable-movies` | `true` or `false` |
| `--widescreen-support` | `true` or `false` |
| `--big-render-distance` | `true` or `false` |
| `--bypass-registry-keys` | `true` or `false` |

Width and height default to the primary monitor's current resolution in physical pixels unless overridden by saved settings or flags. `auto` explicitly detects each requested dimension at startup. The detected numbers are saved with the other settings.

Use `--help` for usage. Invalid arguments exit with a nonzero status before creating launcher windows or starting the game. Relative paths resolve against the working directory. In Steam, set Start In to the game directory and put these arguments in Launch Options, without the executable name.

## Modern Window and Resolution Handling

The original resolution selection menu has been completely removed. The driver and window creation logic were rewritten from the ground up.

The game now boots directly using the resolution and mode you choose in the loader. This avoids all the issues caused by the original menu and fixes the strange resizing behaviour.

Because the window is fully controlled by the loader, you get correct behaviour in fullscreen, borderless, and windowed modes without flickering or forced scaling.

## High Resolution Support

Previously, when running without dgVoodoo and using only the primary GPU, the game was hard limited to 1080p. This is a real limitation of ddraw.dll render targets.

DirectDraw has been patched out, removing this limitation entirely. The game can now run at higher resolutions such as 1440p and 4K with real exclusive fullscreen or borderless modes.

## Release

Currently you will need to build the project, a release is coming soon.

## How It Works

This tool is NOT a dll injector, it uses a **Run PE from memory** approach.

What this means:

- The game executable is mapped into the loaders virtual memory and essentially sandboxed.
- The game runs directly from the loader process
- Much friendlier to Windows Defender  

## Requirements

- **English version of Toy Story 2 PC**  
  Multi-language support is planned  

- Windows PC
- May work on Linux with Wine (Untested)

## Status

If you encounter issues, crashes, odd behaviour or would like a feature request, Please open a pull request or send a bug report in.
