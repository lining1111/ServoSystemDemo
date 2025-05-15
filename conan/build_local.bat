echo "win x64 n build"
del /q /s build
conan install . --build=missing --output-folder=build