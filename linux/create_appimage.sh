#!/bin/bash
cd linux

wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage

chmod +x linuxdeploy*.AppImage

export APPIMAGE_EXTRACT_AND_RUN=1
./linuxdeploy-x86_64.AppImage --appdir AppDir -l /usr/lib/x86_64-linux-gnu/libssl.so.1.1 -e ../bin/Sync-my-L2P -i sync-my-L2P.png -d Sync-my-L2P.desktop --plugin qt --output appimage
