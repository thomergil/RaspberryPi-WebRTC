# webrtc-test

using signalr as the webrtc signaling server

# Build the WebRTC lib (`libwebrtc.a`)
## Environment
* RaspberryPi OS 64bit
* [clang 12+](https://github.com/llvm/llvm-project/releases)
* `boringssl` replace `openssl`

## Preparations
* Install the Chromium depot tools.
    ``` bash
    sudo apt install git
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
    export PATH=/home/ubuntu/depot_tools:$PATH
    ```
* Replace ninja in the `depot_tools`.
    ``` bash
    git clone https://github.com/martine/ninja.git;
    cd ninja;
    ./configure.py --bootstrap;
    mv /home/ubuntu/depot_tools/ninja /home/ubuntu/depot_tools/ninja_org;
    cp /home/ubuntu/ninja/ninja /home/ubuntu/depot_tools/ninja;
    ```
* Replace ninja in the `depot_tools`.
    ``` bash
    git clone https://gn.googlesource.com/gn;
    cd gn;
    sed -i -e "s/-Wl,--icf=all//" build/gen.py;
    python build/gen.py;
    ninja -C out;
    sudo mv /home/ubuntu/webrtc-checkout/src/buildtools/linux64/gn /home/ubuntu/webrtc-checkout/src/buildtools/linux64/gn_org;
    cp /home/ubuntu/gn/out/gn /home/ubuntu/webrtc-checkout/src/buildtools/linux64/gn;
    sudo mv /home/ubuntu/depot_tools/gn /home/ubuntu/depot_tools/gn_org;
    cp /home/ubuntu/gn/out/gn /home/ubuntu/depot_tools/gn;
    ```

## Compile `libwebrtc.a`

1. Clone WebRTC source code and sync
    ```bash
    mkdir webrtc-checkout
    cd ./webrtc-checkout
    fetch --nohooks webrtc
    # git checkout branch-heads/4147 # for specific version
    gclient sync -D
    ```
2. Build
    ``` bash
    gn gen out/DefaultT --args='target_os="linux" \ 
    target_cpu="arm64" \
    rtc_include_tests=false \
    rtc_use_h264=false \
    use_rtti=true \
    is_component_build=false \
    is_debug=true \
    rtc_build_examples=false \
    use_custom_libcxx=false \
    rtc_use_pipewire=false \
    clang_base_path="/usr" \
    treat_warnings_as_errors=false \
    clang_use_chrome_plugins=false'
    ninja -C out/Default
    ```

# Build the [SignalR-Client-Cpp](https://github.com/aspnet/SignalR-Client-Cpp) lib (`microsoft-signalr.so`)
## Preparations
* `sudo apt-get install build-essential curl git cmake ninja-build golang libpcre3-dev zlib1g-dev`
* Install boringssl
    ```bash
    sudo apt-get install libunwind-dev zip unzip tar
    git clone https://github.com/google/boringssl.git
    cd boringssl
    mkdir build
    cd build
    cmake -GNinja .. -DBUILD_SHARED_LIBS=1
    ninja
    sudo ninja install
    sudo cp -r ../install/include/* /usr/local/include/
    sudo cp ../install/lib/*.a /usr/local/lib/
    sudo cp ../install/lib/*.so /usr/local/lib/
    ```
*  Install [cpprestsdk](https://github.com/Microsoft/cpprestsdk/wiki/How-to-build-for-Linux)
    ```bash
    sudo apt-get install libboost-atomic-dev libboost-thread-dev libboost-system-dev libboost-date-time-dev libboost-regex-dev libboost-filesystem-dev libboost-random-dev libboost-chrono-dev libboost-serialization-dev libwebsocketpp-dev
    git clone https://github.com/Microsoft/cpprestsdk.git casablanca
    cd casablanca
    mkdir build.debug
    cd build.debug
    ```
    because we use boringssl rather than openssl, so need modify `/home/pi/casablanca/Release/cmake/cpprest_find_openssl.cmake:53` from
    ```cmake
    find_package(OpenSSL 1.0.0 REQUIRED)
    ```
    to
    ```cmake
    set(OPENSSL_INCLUDE_DIR "/usr/local/include/openssl" CACHE INTERNAL "")
    set(OPENSSL_LIBRARIES "/usr/local/lib/libssl.a" "/usr/local/lib/libcrypto.a" CACHE INTERNAL "")
    ```
    and replace `-std=c++11` into `-std=c++14` in `Release/CMakeLists.txt`, then
    ``` bash
    cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug
    ```
    then build
    ``` bash
    ninja
    sudo ninja install
    ```

## Compile `microsoft-signalr.so`
1. Clone the source code of SignalR-Client-Cpp
    ```bash
    git clone https://github.com/aspnet/SignalR-Client-Cpp.git
    cd ./SignalR-Client-Cpp/
    git submodule update --init
    ```
2. Remove old vcpkg, clone latest version and install packages.
    ```bash
    #cd ./submodules/
    #rm -rf ./vcpkg/
    #git clone https://github.com/microsoft/vcpkg
    #export VCPKG_DEFAULT_TRIPLET=arm64-linux
    #export VCPKG_FORCE_SYSTEM_BINARIES=1
    #./vcpkg/bootstrap-vcpkg.sh -useSystemBinaries
    #./submodules/vcpkg/vcpkg install cpprestsdk[websockets] boost-system boost-chrono boost-thread
    ```
3. In `SignalR-Client-Cpp/CMakeLists.txt:60`, replace from
    ```cmake
    find_package(OpenSSL 1.0.0 REQUIRED)
    ```
    into
    ```cmake
    set(OPENSSL_INCLUDE_DIR "/usr/local/include/openssl" CACHE INTERNAL "")
    set(OPENSSL_LIBRARIES "/usr/local/lib/libssl.a" "/usr/local/lib/libcrypto.a" CACHE INTERNAL "")
    ```
    and in `line 17` replace from `-std=c++11` into `-std=c++14` as well.
4. Build
    ``` bash
    cd ..
    mkdir build-release
    cd ./build-release/
    cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_CPPRESTSDK=true
    cmake --build . --config Release
    sudo make install
    ```
# Reference
* [Development | WebRTC](https://webrtc.github.io/webrtc-org/native-code/development/)
* [Version | WebRTC](https://chromiumdash.appspot.com/branches)