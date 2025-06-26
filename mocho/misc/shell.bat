@echo off


REM 	This is a comment (REM)
REM  
REM  	To run this shell.bat at startup when opening the cmd, use this as your shortcut target: 
REM  	%windir%\system32\cmd.exe /k k:\Engine\mocho\misc\shell.bat
REM	
REM  	/k means, start program with ....
REM
REM
REM
REM

REM this line calls the vs batch in order for us to use the cl compiler in our shell 
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

REM this sets the path of the vs code executable so we can open it ANYWHERE from our shell with the 'code' command 
set path=C:\Users\fabri\AppData\Local\Programs\Microsoft VS Code;%path%

REM sets the misc folder of our project to the system environment paths 
set path=k:\mocho\misc;%path%

REM starts the shell in the k: virtual drive we made with subst in the startup.bat
k:

REM opens the shell in the following directory 
cd \mocho\code

