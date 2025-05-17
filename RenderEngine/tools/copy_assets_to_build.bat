@echo off
setlocal enabledelayedexpansion

:: Define source and destination directories
set compiled_dir=%1
set build_dir=%2

:: Copy compiled assets folder to build
xcopy /s /e /y %compiled_dir%\ %build_dir%\ >NUL

endlocal