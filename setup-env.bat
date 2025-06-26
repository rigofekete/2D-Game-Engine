@echo off

subst k: c:\dev\Engine
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

REM this sets the path of the vs code executable so we can open it ANYWHERE from our shell with the 'code' command 
set path=C:\Users\fabri\AppData\Local\Programs\Microsoft VS Code;%path%

REM sets the misc folder of our project to the system environment paths 
set path=k:\mocho\misc;%path%

REM starts the shell in the k: virtual drive we made

k:

REM opens the shell in the following directory 
cd \mocho\code

REM IMPORTANT NOTE: Not in use right now because I am using the actual setting.json to set up the terminal colors
REM  
REM command to set terminal colors, with two hexadecimal digits - the first for background, second for foreground
REM more info in the Dev Notes folder 
REM color 01