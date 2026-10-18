#!/usr/bin/env bash
# Установщик FM RX для macOS (.pkg): VST3 -> /Library/Audio/Plug-Ins/VST3, PDF-инструкции ->
# /Library/Application Support/NUWMUN/FM RX.
#
#   packaging/macos/build_pkg.sh <version> <path/to/FM RX.vst3> <docs/manual dir> <out dir>
#
# Подпись и нотаризация - если заданы переменные окружения (иначе пакет без подписи, бандл с ad-hoc подписью):
#   MAC_APP_SIGN_ID        "Developer ID Application: Name (TEAMID)"   - подпись плагина
#   MAC_INSTALLER_SIGN_ID  "Developer ID Installer: Name (TEAMID)"     - подпись .pkg
#   NOTARY_APPLE_ID, NOTARY_PASSWORD (app-specific password), NOTARY_TEAM_ID - нотаризация и staple
set -euo pipefail

VERSION="$1"; PLUGIN="$2"; DOCS="$3"; OUT="$4"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT" "$WORK/vst3" "$WORK/docs" "$WORK/pkgs"

echo "== stage"
ditto "$PLUGIN" "$WORK/vst3/FM RX.vst3"
ditto "$DOCS/FM RX Manual EN.pdf" "$WORK/docs/FM RX Manual EN.pdf"
ditto "$DOCS/FM RX Manual RU.pdf" "$WORK/docs/FM RX Manual RU.pdf"
lipo -info "$WORK/vst3/FM RX.vst3/Contents/MacOS/FM RX" || true

echo "== codesign plugin"
if [[ -n "${MAC_APP_SIGN_ID:-}" ]]; then
  codesign --force --options runtime --timestamp --sign "$MAC_APP_SIGN_ID" "$WORK/vst3/FM RX.vst3"
else
  codesign --force --sign - "$WORK/vst3/FM RX.vst3"         # ad-hoc: обязательно для arm64
fi
codesign --verify --strict --verbose=2 "$WORK/vst3/FM RX.vst3"

echo "== component packages"
# Бандл не «переезжает» туда, где система нашла старую копию: всегда в /Library/Audio/Plug-Ins/VST3.
# pkgbuild --analyze не распознаёт .vst3 как бандл, поэтому описание компонента задаём явно.
cat > "$WORK/vst3.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
    <dict>
        <key>BundleHasStrictIdentifier</key><true/>
        <key>BundleIsRelocatable</key><false/>
        <key>BundleIsVersionChecked</key><false/>
        <key>BundleOverwriteAction</key><string>upgrade</string>
        <key>RootRelativeBundlePath</key><string>FM RX.vst3</string>
    </dict>
</array>
</plist>
PLIST
pkgbuild --root "$WORK/vst3" --component-plist "$WORK/vst3.plist" \
  --identifier com.nuwmun.fmrx.vst3 --version "$VERSION" \
  --install-location "/Library/Audio/Plug-Ins/VST3" "$WORK/pkgs/fmrx-vst3.pkg"
pkgbuild --root "$WORK/docs" --identifier com.nuwmun.fmrx.docs --version "$VERSION" \
  --install-location "/Library/Application Support/NUWMUN/FM RX" "$WORK/pkgs/fmrx-docs.pkg"

echo "== product"
sed "s/@VERSION@/$VERSION/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"
PRODUCT="$OUT/FM-RX-$VERSION-macOS.pkg"
SIGN_ARGS=()
[[ -n "${MAC_INSTALLER_SIGN_ID:-}" ]] && SIGN_ARGS=(--sign "$MAC_INSTALLER_SIGN_ID" --timestamp)
productbuild --distribution "$WORK/distribution.xml" --resources "$HERE/resources" \
  --package-path "$WORK/pkgs" ${SIGN_ARGS[@]+"${SIGN_ARGS[@]}"} "$PRODUCT"

if [[ -n "${MAC_INSTALLER_SIGN_ID:-}" && -n "${NOTARY_APPLE_ID:-}" ]]; then
  echo "== notarize"
  xcrun notarytool submit "$PRODUCT" --apple-id "$NOTARY_APPLE_ID" --password "$NOTARY_PASSWORD" \
    --team-id "$NOTARY_TEAM_ID" --wait
  xcrun stapler staple "$PRODUCT"
fi

pkgutil --check-signature "$PRODUCT" || echo "(package is not signed)"
echo "built: $PRODUCT"
