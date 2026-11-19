
# OpenAuto

## Description
OpenAuto is an AndroidAuto(tm) headunit emulator based on aasdk library and Qt libraries. Main goal is to run this application on the RaspberryPI 3 board computer smoothly.

[See demo video](https://www.youtube.com/watch?v=k9tKRqIkQs8)

## Build Guide
#### Local build instructions for Raspberry Pi
#### Building all components of OpenDash should be in the following order:
1. aasdk
2. qt-gstreamer
3. OpenAuto
4. Dash

Having <a href="https://github.com/robert5974/aasdk">aasdk</a> built and installed first, is a prequisite for this task. Please complete the necessary aasdk steps before proceeding here.

Before building OpenAuto, build **Qt-GStreamer** with the following instructions:

**1. Install the pre-requisite packages:**
```
sudo apt install qt5-default qml-module-qtquick2 qtdeclarative5-dev qtmultimedia5-dev qml-module-qtquick* qml-module-qtmultimedia qml-module-qt-labs-settings qml-module-qt-labs-folderlistmodel libqt5xmlpatterns5-dev qml-module-qtbluetooth libqt5charts5 qml-module-qtcharts build-essential openssl libglib2.0-dev libboost-dev libudev-dev libgstreamer1.0-dev gstreamer1.0-plugins-base-apps gstreamer1.0-plugins-bad gstreamer1.0-libav gstreamer1.0-alsa libgstreamer-plugins-base1.0-dev gstreamer1.0-pulseaudio libfaad-dev libfftw3-dev librtlsdr-dev libusb-1.0-0-dev mesa-common-dev libglu1-mesa-dev zlib1g-dev portaudio19-dev libsndfile1-dev libsamplerate0-dev bluez bluez-obexd pulseaudio-module-bluetooth  qml-module-qtbluetooth qml-module-org-kde-bluezqt qtbase5-private-dev libcanberra-dev libgconf2-dev libpulse-dev libmp3lame-dev libsoapysdr-dev libmpg123-dev
```
**2. Clone the qt-gstreamer repo:**
```
git clone https://github.com/robert5974/qt-gstreamer
```
**3. Change to the cloned directory:**
```
cd qt-gstreamer
```
**4. Create the _build_ directory then change to it:**
```
mkdir build
cd build
```
**5. Run the following _cmake_ command:**
```
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH) \
-DCMAKE_INSTALL_INCLUDEDIR=include -DQT_VERSION=5 -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-std=c++11
```
**6. Compile qt-gstreamer:**
```
make -j4
```
**7. Install and scan for recently installed directory:**
```
sudo make install
sudo ldconfig
```

### Continue with building OpenAuto:

**1. Install the pre-requisite packages:**
```
sudo apt-get update
sudo apt-get -y install cmake build-essential git

sudo apt-get install libboost-all-dev libusb-1.0.0-dev libssl-dev cmake libprotobuf-dev protobuf-c-compiler protobuf-compiler libqt5multimedia5 libqt5multimedia5-plugins libqt5multimediawidgets5 qtmultimedia5-dev libqt5bluetooth5 libqt5bluetooth5-bin qtconnectivity5-dev pulseaudio librtaudio-dev
```
**2. Clone the OpenAuto repo:**
```
git clone https://github.com/robert5974/openauto
```
**3. Change to the cloned directory:**
```
cd openauto
```
**5. Run the following _cmake_ command:**
```
cmake -DRPI_BUILD=TRUE -DGST_BUILD=TRUE .
```
**6. Compile OpenAuto:**
```
make
```
**7. Install:**
```
sudo make install
```

**The executable binary can then be found at ~/openauto/bin/autoapp**

### Supported functionalities
 - 480p, 720p and 1080p with 30 or 60 FPS
 - RaspberryPI 3 hardware acceleration support to decode video stream (up to 1080p@60!)
 - Audio playback from all audio channels (Media, System and Speech)
 - Audio input for voice commands
 - Touchscreen and buttons input
 - Bluetooth
 - Automatic launch after device hotplug
 - Automatic detection of connected Android devices
 - Wireless (WiFi) mode via head unit server (must be enabled in hidden developer settings)
 - User-friendly settings

### Supported platforms

 - Linux
 - RaspberryPI 3
 - Windows

### License
GNU GPLv3

Copyrights (c) 2018 f1x.studio (Michal Szwaj)

*AndroidAuto is registered trademark of Google Inc.*

### Used software
 - [aasdk](https://github.com/robert5974/aasdk)
 - [Boost libraries](http://www.boost.org/)
 - [Qt libraries](https://www.qt.io/)
 - [CMake](https://cmake.org/)
 - [RtAudio](https://www.music.mcgill.ca/~gary/rtaudio/playback.html)
 - Broadcom ilclient from RaspberryPI 3 firmware
 - OpenMAX IL API

### Remarks
**This software is not certified by Google Inc. It is created for R&D purposes and may not work as expected by the original authors. Do not use while driving. You use this software at your own risk.**
