@echo off
for /f %%i in ('git rev-list HEAD --count') do set REVISION=%%i
echo The revision is %REVISION%
echo. > Version.h
echo #define SVN_REVISION %REVISION% >> Version.h
echo. >> Version.h
echo #define BKBTL_VERSION_STRING "1.0.%REVISION%" >> Version.h
