echo "win release conan build"
del /q /s ../build
conan install .. -pr:a=default_release --build=missing --output-folder=../build