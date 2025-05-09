@echo off
REM windows_copy_depends.bat是脚本文件名，exe_path是exe文件的全路径，dll_folder_path是脚本当前路径下的文件夹名称
REM 如windows_copy_depends.bat E:\git_code\windows_docker_desktop\winget.exe 123
echo Usage: windows_copy_depends.bat exe_path dll_folder_path (in x64 Native Tools Command Prompt for VS 2019)

rd /s /q %2
mkdir %2

REM exe依赖dll，并将文件列表输出到txt文件中，txt文件在脚本当前目录下的指定文件夹下
dumpbin.exe /dependents %1 | findstr "\.dll$" > .\%2\log_dumpbin_findstr.txt
REM 循环读取txt文件，获取依赖文件
for /f "delims=" %%i in (.\%2\log_dumpbin_findstr.txt) do (
    where /f %%i >> .\%2\log_where.txt
    if errorlevel 1 (
        echo did not find %%i recorded in .\%2\log_copy_error.txt
        echo %%i >> .\%2\log_copy_error.txt
        )
    )

REM 将依赖dll文件拷贝到指定路径下
for /f "delims=" %%i in (.\%2\log_where.txt) do (
    ::copy %%i .\%2\
    xcopy /y %%i .\%2\
    )

REM del /q .\%2\log_dumpbin_findstr.txt
REM del /q .\%2\log_where.txt

::pause
