#!/bin/sh

mkdir build
cd build
cmake ..
make -j $(grep processor /proc/cpuinfo | wc -l)
cd ..
./build/artifact-denoiser --test
