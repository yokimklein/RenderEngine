@echo off
setlocal enabledelayedexpansion

:: Source and destination paths passed in as arguments
set assets_dir=%1
set compiled_dir=%2
set tool_dir=%3

call "%tool_dir%\build_mesh.bat" "%assets_dir%\models" "%compiled_dir%\models" %tool_dir%
call "%tool_dir%\build_tex.bat" "%assets_dir%\textures" "%compiled_dir%\textures" %tool_dir%

:: TODO: compile shaders, for now we'll just copy all hlsl files to compiled_dir
:: Moved this step to copy assets to build

endlocal