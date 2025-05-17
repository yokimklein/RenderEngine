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

:: Compile each .obj to .vbo whilst preserving directories
for /r %source_dir% %%f in (*.obj) do (
    set "full_dir=%%~dpf"
    set "relative_dir=!full_dir:%source_dir%=!"
    set "compiled_dir=%destination_dir%!relative_dir!
	
	if not exist "!compiled_dir!" (
        mkdir "!compiled_dir!"
    )
	
	%tool_dir%\meshconvert "%%f" -vbo -nodds -y -flipu -o "!compiled_dir!%%~nf.vbo" >NUL
)

endlocal