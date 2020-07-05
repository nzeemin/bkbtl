@echo off
set ASTYLEEXE=c:\bin\astyle.exe
set ASTYLEOPT=-n -Q --options=..\astyle-cpp-options
%ASTYLEEXE% %ASTYLEOPT% emubase\*.h emubase\*.cpp Util\*.h Util\*.cpp
%ASTYLEEXE% %ASTYLEOPT% *.h *.cpp --exclude=Version.h --exclude=stdafx.h --exclude=stdafx.cpp
