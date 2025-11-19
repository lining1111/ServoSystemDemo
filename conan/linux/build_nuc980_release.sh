#!/bin/bash
echo "linux nuc980 release conan build"
rm -rf ../build/*
conan install .. -pr:b=default  -pr:h=nuc980_release --build=missing --output-folder=../build