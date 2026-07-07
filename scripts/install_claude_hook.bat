@echo off
setlocal

set "SCRIPT_DIR=%~dp0"

where uv >nul 2>nul
if %ERRORLEVEL%==0 (
  uv run --script "%SCRIPT_DIR%install_claude_hook.py" %*
  exit /b %ERRORLEVEL%
)

where py >nul 2>nul
if %ERRORLEVEL%==0 (
  py "%SCRIPT_DIR%install_claude_hook.py" %*
  exit /b %ERRORLEVEL%
)

where python >nul 2>nul
if %ERRORLEVEL%==0 (
  python "%SCRIPT_DIR%install_claude_hook.py" %*
  exit /b %ERRORLEVEL%
)

echo Python 3 is required. 1>&2
exit /b 1
