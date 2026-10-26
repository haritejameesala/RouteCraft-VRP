@echo off
setlocal
cd /d "%~dp0"
where javac >nul 2>nul
if errorlevel 1 (
  echo JDK 17 or newer is required. Install a JDK and add javac to PATH.
  pause
  exit /b 1
)
set "CXX="
set "CXX_DIR="
if exist "C:\msys64\ucrt64\bin\g++.exe" (
  set "CXX=C:\msys64\ucrt64\bin\g++.exe"
  set "CXX_DIR=C:\msys64\ucrt64\bin"
)
if not defined CXX (
  for /f "delims=" %%G in ('where g++ 2^>nul') do if not defined CXX (
    set "CXX=%%G"
    set "CXX_DIR=%%~dpG"
  )
)
if not defined CXX (
  echo A 64-bit C++17 compiler such as MinGW g++ is required.
  pause
  exit /b 1
)
if not defined JAVA_HOME (
  echo JAVA_HOME must point to a JDK 17 or newer installation.
  pause
  exit /b 1
)
set "PATH=%CXX_DIR%;C:\msys64\usr\bin;%PATH%"
set "CXX_TARGET="
for /f "delims=" %%T in ('"%CXX%" -dumpmachine') do set "CXX_TARGET=%%T"
echo %CXX_TARGET% | findstr /i "x86_64" >nul
if errorlevel 1 (
  echo A 64-bit C++ compiler is required to match this JDK.
  pause
  exit /b 1
)
if not exist build mkdir build
"%CXX%" -std=c++17 -O2 -Wall -Wextra -Wpedantic -shared ^
  -I include -I "%JAVA_HOME%\include" -I "%JAVA_HOME%\include\win32" ^
  source\RouteObjects.cpp source\RouteOptimizer.cpp source\RouteOptimizerJni.cpp ^
  -o build\RouteCraftDsa.dll
if errorlevel 1 (
  echo.
  echo C++ DSA library compilation failed. Review the errors above.
  pause
  exit /b 1
)
javac -encoding UTF-8 -d build RouteCraftVRP.java RouteOptimizerBridge.java
if errorlevel 1 (
  echo.
  echo Java interface compilation failed. Review the errors above.
  pause
  exit /b 1
)
java -Djava.library.path="%CD%\build" -cp build RouteCraftVRP
if errorlevel 1 pause
