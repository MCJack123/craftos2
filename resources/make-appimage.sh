#!/bin/sh

set -eu

ARCH=$(uname -m)

echo "Installing package dependencies..."
echo "---------------------------------------------------------------"
pacman -Syu --noconfirm \
    base-devel    \
    libdecor \
    sdl2    \
    sdl2_mixer  \
    libwebp  \
    ncurses  \
    poco  \
    lua53  \
    png++  \
    libharu

echo "Installing debloated packages..."
echo "---------------------------------------------------------------"
get-debloated-pkgs --add-common --prefer-nano

# Comment this out if you need an AUR package
make-aur-package craftos-pc-data
make-aur-package craftos-pc 

# /----------------/

ARCH=$(uname -m)
VERSION=$(pacman -Q craftos-pc | awk '{print $2; exit}') # example command to get version of application here
export ARCH VERSION
export OUTPATH=./dist
export ADD_HOOKS="self-updater.hook"
export UPINFO="gh-releases-zsync|${GITHUB_REPOSITORY%/*}|${GITHUB_REPOSITORY#*/}|latest|*$ARCH.AppImage.zsync"
export ICON=/usr/share/icons/hicolor/96x96/apps/craftos.png
export DESKTOP=/usr/share/applications/CraftOS-PC.desktop

# Deploy dependencies
quick-sharun \
  /usr/bin/craftos  \
  /usr/share/craftos  \
  /usr/lib/libcraftos2-lua.so

# Additional changes can be done in between here

# Turn AppDir into AppImage
quick-sharun --make-appimage

# Test the app for 12 seconds, if the test fails due to the app
# having issues running in the CI use --simple-test instead
quick-sharun --test ./dist/*.AppImage
