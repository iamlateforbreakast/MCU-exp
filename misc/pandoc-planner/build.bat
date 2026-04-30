@echo off
REM Usage: build.bat my-project.md
REM Output: my-project.html

if "%~1"=="" (
    echo Usage: build.bat your-plan.md
    exit /b 1
)

set INPUT=%~1
set OUTPUT=%~n1.html

pandoc "%INPUT%" ^
  --template plan-template.html ^
  --lua-filter plan-filter.lua ^
  --output "%OUTPUT%" ^
  --standalone

if %errorlevel%==0 (
    echo Done! Output: %OUTPUT%
    start "" "%OUTPUT%"
) else (
    echo Pandoc failed. Make sure pandoc is installed and in your PATH.
)
