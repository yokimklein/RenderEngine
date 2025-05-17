@echo off
setlocal enabledelayedexpansion

set destination_dir=%1
set destination_dir=%destination_dir:"=%

if exist %destination_dir%\ (
	rmdir /s /q %destination_dir%\
)
endlocal