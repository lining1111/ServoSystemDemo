#!/bin/bash
echo "linux x64 release conan build"
rm -rf ../build/*
conan install .. -pr:a=default_release --build=missing --output-folder=../build