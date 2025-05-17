@echo off
setlocal enabledelayedexpansion

:: Source and destination paths passed in as arguments
set source_dir=%1
set destination_dir=%2
set tool_dir=%3
:: Remove speech marks from directories
set source_dir=%source_dir:"=%
set destination_dir=%destination_dir:"=%
set tool_dir=%tool_dir:"=%

:: TODO: convert different texture types to different DDS formats
for /r "%source_dir%" %%f in (*.tga) do (
    set "full_dir=%%~dpf"
    set "relative_dir=!full_dir:%source_dir%=!"
    set "compiled_dir=%destination_dir%!relative_dir!
	
	:: Remove trailing backslash from compiled_dir which texconv will not work with
	if "!compiled_dir:~-1!"=="\" set "compiled_dir=!compiled_dir:~0,-1!"
	
    if not exist "!compiled_dir!" (
        mkdir "!compiled_dir!"
    )
	
	"%tool_dir%\texconv" "%%f" -o "!compiled_dir!" -hflip -vflip -y >NUL
)

:: Move existing DDS files to compiled assets
for /r "%source_dir%" %%f in (*.dds) do (
    set "file=%%f"
    set "relative_path=!file:%source_dir%=!"
    set "dest_folder=%destination_dir%!relative_path!\..\"
	
	:: Create folder if it doesn't exist
    if not exist "!dest_folder!" (
        mkdir "!dest_folder!"
    )

    :: Copy the file to the destination, >NUL suppresses output to build log
    copy "%%f" "!dest_folder!" >NUL
)
endlocal