#!/bin/bash
echo "linux aarch64 release conan build"
rm -rf ../build_aarch64/*
conan install .. -pr:b=default  -pr:h=aarch64_release --build=missing --output-folder=../build_aarch64