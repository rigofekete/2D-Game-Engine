REM Batch file to be ran at the OS startup in order to execute the drive subst to K:

REM echo off disables output messages from the batch files  

@echo off


REM we need to add this code so that when the OS starts up it immediatley opens this batch file as admin (otherwise it wont run)
 
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Requesting administrative privileges...
    powershell start -verb runAs '%~0'
    exit /b
)

REM create a virtual drive k that points to the specified path 

subst k: c:\dev\Engine
