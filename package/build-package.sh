#!/bin/bash
# Builds Quill and its .hpkg on Haiku. Run from anywhere; needs haiku_devel.
set -e
cd "$(dirname "$0")/.."
ROOT=$(pwd)
VERSION=$(grep '^version' package/PackageInfo | awk '{print $2}')
ARCH=$(getarch)

echo "Building Quill..."
make

PKG=$ROOT/package/pkg
rm -rf "$PKG"
mkdir -p "$PKG/apps" "$PKG/data/deskbar/menu/Applications" \
	"$PKG/documentation/packages/quill"

cp objects.*/Quill "$PKG/apps/Quill"
ln -s ../../../../apps/Quill "$PKG/data/deskbar/menu/Applications/Quill"
cp README.md "$PKG/documentation/packages/quill/README.md"
cp LICENSE "$PKG/documentation/packages/quill/LICENSE"
sed "s/^architecture.*/architecture\t$ARCH/" package/PackageInfo \
	> "$PKG/.PackageInfo"

OUT=$ROOT/package/quill-$VERSION-$ARCH.hpkg
rm -f "$OUT"
(cd "$PKG" && package create -q "$OUT")
rm -rf "$PKG"
echo "Created $OUT"
echo "Install with: pkgman install $OUT"
