#!/bin/zsh
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
app="$script_dir/dist/CodexUsageMac.app"
contents="$app/Contents"
source_icon="$repo_root/CodexUsage/res/CodexUsage-source.png"

swift test --package-path "$script_dir"
swift build --package-path "$script_dir" -c release
bin_dir="$(swift build --package-path "$script_dir" -c release --show-bin-path)"

mkdir -p "$contents/MacOS" "$contents/Resources"
cp "$bin_dir/CodexUsageMac" "$contents/MacOS/CodexUsageMac"
cp "$repo_root/CodexUsage/res/CodexUsage.png" "$contents/Resources/CodexUsage.png"

cat > "$contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
    <key>CFBundleIdentifier</key><string>com.citopia.CodexUsageMac</string>
    <key>CFBundleName</key><string>CodexUsageMac</string>
    <key>CFBundleDisplayName</key><string>CodexUsage</string>
    <key>CFBundleExecutable</key><string>CodexUsageMac</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>1.0.20260926</string>
    <key>CFBundleVersion</key><string>1</string>
    <key>LSMinimumSystemVersion</key><string>13.0</string>
    <key>LSUIElement</key><true/>
    <key>NSHighResolutionCapable</key><true/>
    <key>CFBundleIconFile</key><string>CodexUsage.icns</string>
</dict></plist>
PLIST

iconset="$script_dir/dist/CodexUsage.iconset"
mkdir -p "$iconset"
for spec in \
    "16 icon_16x16.png" "32 icon_16x16@2x.png" \
    "32 icon_32x32.png" "64 icon_32x32@2x.png" \
    "128 icon_128x128.png" "256 icon_128x128@2x.png" \
    "256 icon_256x256.png" "512 icon_256x256@2x.png" \
    "512 icon_512x512.png" "1024 icon_512x512@2x.png"; do
    size="$(echo "$spec" | cut -d ' ' -f1)"
    name="$(echo "$spec" | cut -d ' ' -f2)"
    sips -s format png -z "$size" "$size" "$source_icon" --out "$iconset/$name" >/dev/null
done
iconutil -c icns "$iconset" -o "$contents/Resources/CodexUsage.icns"
rm -rf "$iconset"
codesign --force --deep --sign - "$app"

echo "Built $app"
echo "Move the app to /Applications before enabling Run at login."