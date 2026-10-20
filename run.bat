@echo off
setlocal
cd /d "%~dp0"
where javac >nul 2>nul
if errorlevel 1 (
  echo JDK 17 or newer is required. Install a JDK and add javac to PATH.
  pause
  exit /b 1
)
javac -encoding UTF-8 RouteCraftVRP.java
if errorlevel 1 (
  echo.
  echo Compilation failed. Review the errors above.
  pause
  exit /b 1
)
java RouteCraftVRP
if errorlevel 1 pause
