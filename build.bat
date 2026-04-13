@echo off
setlocal enabledelayedexpansion

rem =========================================================================
rem build.bat — Single-entry-point build orchestration for the Hellspawn project.
rem
rem The script interactively prompts for:
rem   1. Build type   — release or debug
rem   2. vcpkg usage  — yes or no
rem
rem Requirements: cmake, MSVC (Visual Studio with C++ workload),
rem               and optionally vcpkg (with VCPKG_ROOT set or vcpkg in PATH)
rem =========================================================================

rem ---------------------------------------------------------------------------
rem Constants
rem ---------------------------------------------------------------------------

set "PROJECT_ROOT=%~dp0"
rem Remove trailing backslash for cleanliness
if "%PROJECT_ROOT:~-1%"=="\" set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"

set "BUILD_DIR=%PROJECT_ROOT%\build"
set "TARGET_NAME=hellspawn"

rem ---------------------------------------------------------------------------
rem Interactive prompt: Build type (release/debug)
rem ---------------------------------------------------------------------------
rem Repeatedly asks the user until a valid answer is provided.
rem Accepts "release" or "debug" (case-insensitive).
rem ---------------------------------------------------------------------------

:prompt_build_type
set "BUILD_TYPE_INPUT="
set /p "BUILD_TYPE_INPUT=[INPUT] Enter build type (release/debug): "

if /i "!BUILD_TYPE_INPUT!"=="release" (
    set "BUILD_TYPE=Release"
    goto :build_type_done
)
if /i "!BUILD_TYPE_INPUT!"=="debug" (
    set "BUILD_TYPE=Debug"
    goto :build_type_done
)

echo [WARN]  Invalid input '!BUILD_TYPE_INPUT!'. Please enter 'release' or 'debug'.
goto :prompt_build_type

:build_type_done
echo [INFO]  Build type: %BUILD_TYPE%

rem ---------------------------------------------------------------------------
rem Interactive prompt: Use vcpkg? (yes/no)
rem ---------------------------------------------------------------------------
rem Repeatedly asks the user until a valid answer is provided.
rem Accepts "yes" or "no" (case-insensitive).
rem ---------------------------------------------------------------------------

:prompt_use_vcpkg
set "USE_VCPKG_INPUT="
set /p "USE_VCPKG_INPUT=[INPUT] Use vcpkg? (yes/no): "

if /i "!USE_VCPKG_INPUT!"=="yes" (
    set "USE_VCPKG=yes"
    goto :use_vcpkg_done
)
if /i "!USE_VCPKG_INPUT!"=="no" (
    set "USE_VCPKG=no"
    goto :use_vcpkg_done
)

echo [WARN]  Invalid input '!USE_VCPKG_INPUT!'. Please enter 'yes' or 'no'.
goto :prompt_use_vcpkg

:use_vcpkg_done
echo [INFO]  Use vcpkg: %USE_VCPKG%

rem ---------------------------------------------------------------------------
rem Dependency validation: CMake
rem ---------------------------------------------------------------------------

where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake is not installed or not in PATH. Please install CMake ^>= 3.21.
    exit /b 1
)

for /f "tokens=*" %%V in ('cmake --version 2^>^&1') do (
    set "CMAKE_VERSION=%%V"
    goto :cmake_version_done
)
:cmake_version_done
echo [OK]    CMake found: %CMAKE_VERSION%

rem ---------------------------------------------------------------------------
rem Dependency validation: MSVC compiler
rem ---------------------------------------------------------------------------

rem Try to find Visual Studio using vswhere (ships with VS 2017 15.2+)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSINSTALL_DIR="
set "VCVARSALL="

if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
        set "VSINSTALL_DIR=%%i"
    )
)

if defined VSINSTALL_DIR (
    set "VCVARSALL=!VSINSTALL_DIR!\VC\Auxiliary\Build\vcvarsall.bat"
    if exist "!VCVARSALL!" (
        echo [OK]    MSVC found via vswhere: !VSINSTALL_DIR!
        goto :msvc_found
    )
)

rem Fallback: check if cl.exe is already on PATH (e.g., Developer Command Prompt)
where cl.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK]    MSVC compiler ^(cl.exe^) already available in PATH.
    set "VCVARSALL="
    goto :msvc_found
)

rem Fallback: search common VS installation paths
for %%Y in (2022 2019 2017) do (
    for %%E in (Enterprise Professional Community BuildTools) do (
        set "CANDIDATE=%ProgramFiles%\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvarsall.bat"
        if exist "!CANDIDATE!" (
            set "VCVARSALL=!CANDIDATE!"
            echo [OK]    MSVC found at: !CANDIDATE!
            goto :msvc_found
        )
        set "CANDIDATE=%ProgramFiles(x86)%\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvarsall.bat"
        if exist "!CANDIDATE!" (
            set "VCVARSALL=!CANDIDATE!"
            echo [OK]    MSVC found at: !CANDIDATE!
            goto :msvc_found
        )
    )
)

echo [ERROR] MSVC compiler not found. Install Visual Studio with the "Desktop development with C++" workload.
exit /b 1

:msvc_found

rem Initialize MSVC environment if vcvarsall was located and cl.exe is not yet on PATH
if defined VCVARSALL (
    where cl.exe >nul 2>&1
    if !errorlevel! neq 0 (
        echo [INFO]  Initializing MSVC environment via vcvarsall.bat ^(x64^)...
        call "!VCVARSALL!" x64 >nul 2>&1
        if !errorlevel! neq 0 (
            echo [ERROR] Failed to initialize MSVC environment.
            exit /b 1
        )
        rem Verify cl.exe is now available
        where cl.exe >nul 2>&1
        if !errorlevel! neq 0 (
            echo [ERROR] cl.exe still not found after vcvarsall.bat. MSVC setup may be broken.
            exit /b 1
        )
        echo [OK]    MSVC environment initialized.
    )
)

rem ---------------------------------------------------------------------------
rem Dependency validation: vcpkg (only when the user opted in)
rem ---------------------------------------------------------------------------

set "VCPKG_TOOLCHAIN="

if /i "%USE_VCPKG%"=="no" (
    echo [INFO]  Skipping vcpkg — dependencies must be available via system packages.
    goto :vcpkg_resolved
)

rem 1. Explicit VCPKG_ROOT environment variable
if defined VCPKG_ROOT (
    if exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
        set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
        echo [OK]    vcpkg root ^(from VCPKG_ROOT^): %VCPKG_ROOT%
        goto :vcpkg_found
    )
)

rem 2. vcpkg binary in PATH — derive root from its location
where vcpkg.exe >nul 2>&1
if %errorlevel% equ 0 (
    for /f "tokens=*" %%P in ('where vcpkg.exe') do (
        set "VCPKG_BIN_PATH=%%~dpP"
        rem Remove trailing backslash
        if "!VCPKG_BIN_PATH:~-1!"=="\" set "VCPKG_BIN_PATH=!VCPKG_BIN_PATH:~0,-1!"
        if exist "!VCPKG_BIN_PATH!\scripts\buildsystems\vcpkg.cmake" (
            set "VCPKG_ROOT=!VCPKG_BIN_PATH!"
            set "VCPKG_TOOLCHAIN=!VCPKG_BIN_PATH!\scripts\buildsystems\vcpkg.cmake"
            echo [OK]    vcpkg root ^(from PATH^): !VCPKG_ROOT!
            goto :vcpkg_found
        )
    )
)

rem 3. Common installation directories
for %%D in (
    "C:\vcpkg"
    "C:\src\vcpkg"
    "C:\tools\vcpkg"
    "%USERPROFILE%\vcpkg"
    "%USERPROFILE%\source\vcpkg"
) do (
    if exist "%%~D\scripts\buildsystems\vcpkg.cmake" (
        set "VCPKG_ROOT=%%~D"
        set "VCPKG_TOOLCHAIN=%%~D\scripts\buildsystems\vcpkg.cmake"
        echo [OK]    vcpkg root ^(common path^): %%~D
        goto :vcpkg_found
    )
)

echo [ERROR] vcpkg not found. Set the VCPKG_ROOT environment variable or install vcpkg.
exit /b 1

:vcpkg_found

if not exist "%VCPKG_TOOLCHAIN%" (
    echo [ERROR] vcpkg toolchain file not found at: %VCPKG_TOOLCHAIN%
    exit /b 1
)

echo [OK]    vcpkg toolchain: %VCPKG_TOOLCHAIN%

:vcpkg_resolved

rem ---------------------------------------------------------------------------
rem Determine parallel job count
rem ---------------------------------------------------------------------------

set "PARALLEL_JOBS=%NUMBER_OF_PROCESSORS%"
if not defined PARALLEL_JOBS set "PARALLEL_JOBS=4"

echo [INFO]  Parallel jobs: %PARALLEL_JOBS%

rem ---------------------------------------------------------------------------
rem Create build directory
rem ---------------------------------------------------------------------------

if not exist "%BUILD_DIR%" (
    echo [INFO]  Creating build directory: %BUILD_DIR%
    mkdir "%BUILD_DIR%"
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to create build directory: %BUILD_DIR%
        exit /b 1
    )
)

rem ---------------------------------------------------------------------------
rem CMake configure
rem ---------------------------------------------------------------------------
rem When vcpkg is enabled the toolchain file is passed so that manifest
rem dependencies are installed automatically during this step.
rem ---------------------------------------------------------------------------

echo [INFO]  Configuring CMake...

rem Build the base cmake configure command.  The vcpkg toolchain argument is
rem appended only when the user chose to use vcpkg.
rem
rem The path is wrapped in quotes so that spaces (e.g. "C:\Program Files\...")
rem are preserved as a single token when %VCPKG_ARG% is expanded on the cmake
rem command line.  The unquoted `set` form is intentional — it allows the
rem embedded quotes around the path value to become part of the variable
rem content, producing:  -DCMAKE_TOOLCHAIN_FILE="C:\Program Files\...\vcpkg.cmake"
set "VCPKG_ARG="
if defined VCPKG_TOOLCHAIN (
    if not "!VCPKG_TOOLCHAIN!"=="" (
        set VCPKG_ARG=-DCMAKE_TOOLCHAIN_FILE="!VCPKG_TOOLCHAIN!"
    )
)

cmake ^
    -S "%PROJECT_ROOT%" ^
    -B "%BUILD_DIR%" ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    %VCPKG_ARG% ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if %errorlevel% neq 0 (
    rem Retry without specifying a generator — let CMake pick the best available
    echo [WARN]  VS 2022 generator failed. Retrying with default generator...
    cmake ^
        -S "%PROJECT_ROOT%" ^
        -B "%BUILD_DIR%" ^
        -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
        %VCPKG_ARG% ^
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    if !errorlevel! neq 0 (
        echo [ERROR] CMake configuration failed.
        exit /b 1
    )
)

echo [OK]    CMake configuration complete.

rem ---------------------------------------------------------------------------
rem Build
rem ---------------------------------------------------------------------------

echo [INFO]  Building %TARGET_NAME% ^(%BUILD_TYPE%^) with %PARALLEL_JOBS% parallel jobs...

cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --target %TARGET_NAME% -j %PARALLEL_JOBS%
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    exit /b 1
)

rem ---------------------------------------------------------------------------
rem Success
rem ---------------------------------------------------------------------------

rem Multi-config generators (Visual Studio) place binaries under a config subdir
if exist "%BUILD_DIR%\%BUILD_TYPE%\%TARGET_NAME%.exe" (
    echo [OK]    Build succeeded: %BUILD_DIR%\%BUILD_TYPE%\%TARGET_NAME%.exe
) else if exist "%BUILD_DIR%\%TARGET_NAME%.exe" (
    echo [OK]    Build succeeded: %BUILD_DIR%\%TARGET_NAME%.exe
) else (
    echo [OK]    Build succeeded. Binary located in: %BUILD_DIR%
)

exit /b 0
