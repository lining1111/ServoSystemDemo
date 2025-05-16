echo "win debug conan build"
del /q /s ../build
conan install .. -pr:a=default_debug --build=missing --output-folder=../build