echo "local conan build"
del /q /s build
conan install . --build=missing --output-folder=build