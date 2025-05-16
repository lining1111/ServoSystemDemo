#!/bin/bash
echo "linux aarch64 release conan build"
rm -rf ../build/*
conan install .. -pr:b=default_debug  -pr:h=aarch64_release --build=missing --output-folder=../build