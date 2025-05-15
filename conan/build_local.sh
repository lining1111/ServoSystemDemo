#!/bin/bash
echo "linux x64 conan build"
rm -rf build/*
conan install . --build=missing --output-folder=build