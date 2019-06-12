#!/bin/sh

YAML_CPP_OPTIONS='-DYAML_CPP_BUILD_TESTS=OFF -DYAML_CPP_BUILD_TOOLS=OFF -DYAML_CPP_INSTALL=OFF -DMSVC_SHARED_RT=OFF'
GTETST_OPTIONS='-DINSTALL_GTEST=OFF'
DENOISER_OPTIONS='-DDENOISER_TESTS=ON'
GLOBAL_OPTIONS='-DCMAKE_BUILD_TYPE=Release'
CMAKE_OPTIONS="$YAML_CPP_OPTIONS $GTETST_OPTIONS $DENOISER_OPTIONS $GLOBAL_OPTIONS"
MAKE_OPTIONS="-j $(grep processor /proc/cpuinfo | wc -l)"
SOURCE_DIR=/shared

set -e
mkdir build && cd build
cmake $CMAKE_OPTIONS $SOURCE_DIR
make $MAKE_OPTIONS
./artifact-denoiser -d $SOURCE_DIR --test --debug --profile