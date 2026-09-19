<img src="./resources/com.ysuraj.Tokri.png" width="128" height="128" alt="Tokri icon">

# Tokri

> A desktop basket to drag and drop text, URLs, images, and files.

- Shake while dragging to open the basket and drop items inside.
- Drag items **out** to move them
- Hold Ctrl (Windows/Linux) or ⌥ Option (macOS) while dragging to copy

## Images
![](./assets/image.gif)

## Text & URLs
![](./assets/text.gif)

## Files
![](./assets/files.gif)

## Download

### Windows
- Installer: [TokriSetup.exe](https://github.com/jarusll/tokri/releases/download/v2026.03.01/TokriSetup.exe)
- Portable (.zip): [Tokri.zip](https://github.com/jarusll/tokri/releases/download/v2026.03.01/Tokri.zip)

### macOS
- DMG installer: [Tokri.dmg](https://github.com/jarusll/tokri/releases/download/v2026.03.01/Tokri.dmg)
- Install via Homebrew:
    ```zsh
    brew tap jarusll/tap
    brew install --cask jarusll/tap/tokri
    ```

> **Note for macOS users**
>
> This app is **unsigned**, so macOS will block it.
>
> To run it:
>
> - Run in Terminal:
>   ```zsh
>   sudo /usr/bin/xattr -dr com.apple.quarantine /Applications/Tokri.app
>   ```
>
> Or allow it via:
> **System Settings → Privacy & Security → Open Anyway**

### Linux
- Flatpak bundle: [Tokri.flatpak](https://github.com/jarusll/tokri/releases/download/v2026.03.01/Tokri.flatpak)

> **Note for Linux users**
>
> This application reads from `/dev/input/*` to detect mouse activation gestures.
> Add your user to the `input` group:
>
> ```bash
> sudo usermod -aG input $USER
> ```
>
> Log out and log back in for the change to take effect.

> **Note for KDE users**
>
> Mouse shake activation does not work on **KDE Wayland**.
>
> Run the app under **XWayland** by disabling Wayland access:
> - Open **Flatseal**
> - Select the app
> - Remove the **Wayland** permission
>
> This forces XWayland and restores mouse shake activation.
>
> ![](./assets/flatseal.png)

## Building for Linux

#### Dependencies
- qt6-base

### Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Acknowledgements
- [KDAB](https://www.youtube.com/@KDABtv) for their awesome Qt learning resources
- 🎨 Design by [Akshay Majgaonkar](https://www.linkedin.com/in/akshay-majgaonkar/)
