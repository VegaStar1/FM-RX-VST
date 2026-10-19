# Releasing FM RX

## How to release
1. Bump the version in `CMakeLists.txt` (`project(FmRx VERSION X.Y.Z ...)`) and update "What's new" in
   `packaging/RELEASE_NOTES.md` (`@VERSION@` and the download links are filled in automatically).
2. If the interface or features changed, update the manual:
   - screenshots: configure with `-DFMRX_BUILD_CHECK=ON`, run `FmRxCheck <station.wav> <out>` and copy
     `manual_*.png` to `docs/manual/img/*.jpg`;
   - PDFs: `python3 docs/manual/make_manual.py --version X.Y.Z` (needs reportlab, Pillow and DejaVu fonts).
3. Commit and push to `main`.
4. GitHub Actions builds and checks the installers (pluginval, test install and uninstall) and creates or updates a
   **draft** release `vX.Y.Z` with:
   - `FM-RX-X.Y.Z-Windows.zip` and `FM-RX-X.Y.Z-macOS.zip` — installer + both PDF manuals + `INSTALL.txt`;
   - `FM-RX-X.Y.Z-Windows-Setup.exe`, `FM-RX-X.Y.Z-macOS.pkg` and the PDFs on their own.

   Only repository collaborators can see a draft; every push to `main` refreshes its files and text.
5. Publish it: Releases → the draft → **Edit** → **Publish release** (the tag `vX.Y.Z` is created on publish).
   The download links in the description use the permanent `releases/download/vX.Y.Z/...` addresses, so they start
   working the moment the release is published. While it is still a draft, download the files from the **Assets**
   list at the bottom of the release page (GitHub cannot give a draft both a tag and stable file links — every edit
   with a tag changes the draft's temporary file address).
   A published release is never touched by later pushes — bump the version for the next one.

## What the installers do
| | Windows (.exe, Inno Setup) | macOS (.pkg) |
|---|---|---|
| Plug-in | `C:\Program Files\Common Files\VST3\FM RX.vst3` | `/Library/Audio/Plug-Ins/VST3/FM RX.vst3` |
| Manuals | `C:\Program Files\NUWMUN\FM RX\` + Start menu shortcut | `/Library/Application Support/NUWMUN/FM RX/` |
| Installer languages | English, Russian (follows the system language) | English, Russian (follows the system language) |
| Uninstall | Settings → Apps → FM RX | delete the two folders above |

The plug-in looks for the PDF in these folders (Full manual button in the help window).
The Windows build uses the static MSVC runtime, so no Visual C++ Redistributable is needed.
The macOS build is universal (arm64 + x86_64), macOS 10.13 or later.

## Code signing (recommended before selling)
Unsigned installers work, but:
- **Windows** SmartScreen shows "Unknown publisher" → More info → Run anyway.
- **macOS** refuses to open the package with a double click ("unidentified developer") → right-click → Open.

To avoid that, add repository secrets (Settings → Secrets and variables → Actions):

| Secret | What it is |
|---|---|
| `WIN_CERT_PFX_BASE64`, `WIN_CERT_PASSWORD` | Windows code-signing certificate (.pfx, base64) and its password |
| `MAC_CERTS_P12_BASE64`, `MAC_CERTS_PASSWORD` | *Developer ID Application* and *Developer ID Installer* certificates exported from Keychain into one .p12 (base64) and its password |
| `MAC_APP_SIGN_ID` | e.g. `Developer ID Application: Jane Doe (ABCDE12345)` |
| `MAC_INSTALLER_SIGN_ID` | e.g. `Developer ID Installer: Jane Doe (ABCDE12345)` |
| `NOTARY_APPLE_ID`, `NOTARY_PASSWORD`, `NOTARY_TEAM_ID` | Apple ID, app-specific password (appleid.apple.com) and Team ID — for notarization |

macOS certificates come with the Apple Developer Program (USD 99 / year). For Windows, any code-signing CA (OV/EV)
or Azure Trusted Signing. The signing steps are already in `.github/workflows/build.yml` and
`packaging/macos/build_pkg.sh`; they switch on automatically once the secrets exist.

## Licences before selling
- **JUCE 8**: the free Starter licence covers up to USD 20,000 annual revenue; above that, Indie (up to USD 300,000)
  or Pro. JUCE 8 does not require the splash screen (`JUCE_DISPLAY_SPLASH_SCREEN=0`).
- **VST3**: built with the VST3 SDK bundled with JUCE. Use of the VST name and logo follows Steinberg's guidelines.
- **radiofm** (MIT, © Henrik Forssell) — credited in the manual and `THIRD_PARTY_NOTICES.md`.
