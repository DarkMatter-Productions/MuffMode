@echo off
rem Build (build.bat) then deploy (deploy.bat) in a single step.
rem   - Any arguments are passed through to build.bat, e.g.  build-deploy.bat x64 v143
rem   - Set MUFFMODE_BUILD_CONFIG=Release beforehand for a release build.
rem Deploy only runs if the build succeeds.
setlocal EnableExtensions

set "ROOT=%~dp0"

echo ============================================================
echo   BUILD ^& DEPLOY
echo ============================================================
echo.

call "%ROOT%build.bat" %*
set "BUILD_EXIT=%ERRORLEVEL%"
if not "%BUILD_EXIT%"=="0" (
    echo.
    echo ============================================================
    echo   BUILD FAILED ^(exit %BUILD_EXIT%^) - deploy skipped.
    echo   Nothing was copied to the server.
    echo ============================================================
    echo.
    pause
    exit /b %BUILD_EXIT%
)

echo.
echo [OK] Build complete - starting deploy...
echo.

call "%ROOT%deploy.bat"
set "DEPLOY_EXIT=%ERRORLEVEL%"
if not "%DEPLOY_EXIT%"=="0" (
    rem deploy.bat has already printed its own failure banner and paused.
    exit /b %DEPLOY_EXIT%
)

exit /b 0
