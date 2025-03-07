#!/bin/bash

rm -rf cmake-build-d
mkdir cmake-build-d
cd cmake-build-d

cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -- -j4
