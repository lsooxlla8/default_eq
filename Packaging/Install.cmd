@echo off
setlocal

set "PACKAGE_DIRECTORY=%~dp0"
set "VST3_SOURCE=%PACKAGE_DIRECTORY%default_eq.vst3"
set "VST3_DIRECTORY=%LOCALAPPDATA%\Programs\Common\VST3"
set "STANDALONE_SOURCE=%PACKAGE_DIRECTORY%default_eq.exe"
set "STANDALONE_DIRECTORY=%LOCALAPPDATA%\Programs\default_eq"

if exist "%VST3_SOURCE%" (
    if not exist "%VST3_DIRECTORY%" mkdir "%VST3_DIRECTORY%"
    if exist "%VST3_DIRECTORY%\default_eq.vst3" rmdir /s /q "%VST3_DIRECTORY%\default_eq.vst3"
    xcopy "%VST3_SOURCE%" "%VST3_DIRECTORY%\default_eq.vst3\" /e /i /h /y >nul
    if errorlevel 1 goto :error
    echo Installed: %VST3_DIRECTORY%\default_eq.vst3
)

if exist "%STANDALONE_SOURCE%" (
    if not exist "%STANDALONE_DIRECTORY%" mkdir "%STANDALONE_DIRECTORY%"
    copy /y "%STANDALONE_SOURCE%" "%STANDALONE_DIRECTORY%\default_eq.exe" >nul
    if errorlevel 1 goto :error
    echo Installed: %STANDALONE_DIRECTORY%\default_eq.exe
)

echo.
echo default_eq installation complete. Restart your DAW before scanning plug-ins.
pause
exit /b 0

:error
echo.
echo Installation failed.
pause
exit /b 1
