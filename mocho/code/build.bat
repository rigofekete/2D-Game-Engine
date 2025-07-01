@echo off

REM create a directory relative to the current location (specified by the ..\ )

mkdir ..\..\build

REM pushes the current directory to the stack and puts us in ..\..\build
REM while we run the compiler line, so that the compiled files can be stored in the created build folder 

pushd ..\..\build

REM compile the cpp with the linked lib to our build folder // For documentation on cl commands used read the cmdnotes.txt

cl /FC /Zi ..\mocho\code\win32_mocho.cpp /link user32.lib Gdi32.lib

REM pop the previous directory so we can go to the location we were before the pushd

popd



 