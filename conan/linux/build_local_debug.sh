#!/bin/bash
echo "linux x64 debug conan build"
rm -rf ../build/*
conan install .. -pr:a=default_debug --build=missing --output-folder=../build