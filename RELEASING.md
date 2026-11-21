# Releasing Graviton FX

How to publish a new version on GitHub. Run the commands in **Developer PowerShell for VS** in the repository folder.

## 1. Set the version

Change the version in `CMakeLists.txt`:

```cmake
project(GravitonFX VERSION 0.2.0 LANGUAGES CXX RC)
```

The app reads it at build time (`GFX_VERSION`): the About section shows it, and the update check compares it with GitHub's latest release.

## 2. Build and test

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Both builds must finish without warnings and every test must pass.

## 3. Package

```powershell
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

This makes, in `dist\`:

- `GravitonFX-<version>-portable.zip`, with `GravitonFX.exe` and `LICENSE.txt`.
- `GravitonFX-Setup-<version>.exe`, when Inno Setup 7 is installed (task 12.1).

## 4. Check the packages

- Unzip the portable zip into a new folder and run it: the dashboard appears, and About shows the new version.
- Run the installer over the previous version: it upgrades in place, and your settings and presets are kept.
- Uninstall from Settings › Apps: every file is removed, and you're asked whether to delete the settings too.

## 5. Tag and publish

Write the release notes in a file (for example `dist\notes.md`): a short list of what changed, in plain words. Then:

```powershell
git commit -am "Release v0.2.0"
git tag v0.2.0
git push origin main --tags
gh release create v0.2.0 dist\GravitonFX-Setup-0.2.0.exe dist\GravitonFX-0.2.0-portable.zip `
  --title "Graviton FX 0.2.0" --notes-file dist\notes.md
```

Don't mark it as a prerelease or a draft: the app's update check only counts regular, published releases.

## 6. Check the release

- The release page shows both downloads.
- An older version of the app, left running or started, shows the update notification once, plus the dot on the Settings button. To check without an old install, run a Debug build with `GFX_UPDATE_TEST_URL` pointed at a test server, or build with a lower version number.

## Notes

- The builds aren't code-signed, so Windows SmartScreen may warn on first run ("More info › Run anyway"). The README says so.
- If ASIO is enabled (`GFX_WITH_ASIO=ON`), the release uses the ASIO SDK under its GPLv3 option. Check the SDK's `LICENSE.txt` again before each release (design.md, D11).
