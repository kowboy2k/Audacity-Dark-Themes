@echo off
rem Chemin propre à ce PC : Visual Studio 18 Build Tools (outils C++ x64). Sur un autre PC,
rem remplacer "18\BuildTools" par l'édition installée (ex. 2022\Community).
set "PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\Installer"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
cl /nologo /LD /O2 /EHsc /W3 /utf-8 mod-sombre.cpp /Fe:mod-sombre.dll /link /NOLOGO || exit /b 1
del /q mod-sombre.obj mod-sombre.lib mod-sombre.exp 2>nul
