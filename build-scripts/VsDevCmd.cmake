#[=======================================================================[

VsDevCmd.cmake
--------------

Bootstraps the Visual Studio toolchain environment when CMake is not already
running inside it (i.e. VSCMD_VER is not set in the environment).

Runs VsDevCmd.bat and captures the environment variables it exports.
Injects them into the current CMake process scope and populates
_MSVC_DEVENV for writing into CMakeUserPresets.json at a later step.

This is a no-op when CMake is launched from:
  - The Visual Studio IDE (File > Open > Folder)
  - A Visual Studio Developer Command Prompt / PowerShell
  - A CI environment that pre-configures the VS toolchain

#]=======================================================================]

if("$ENV{VSCMD_VER}" STREQUAL "")
    # Not inside a VS developer environment. Locate VsDevCmd.bat via vswhere.

    if ("$ENV{DevEnvDir}" STREQUAL "")
        # vswhere.exe ships with VS 2017+ at a well-known fixed path.
        # Prefer it over downloading to avoid network dependencies at configure time.
        set(_vswhere_installed "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer/vswhere.exe")
        if(EXISTS "${_vswhere_installed}")
            set(_vswhere_exe "${_vswhere_installed}")
            set(_vswhere_found TRUE)
            message(STATUS "Found vswhere.exe: ${_vswhere_installed}")
        else()
            # Fall back to downloading vswhere.exe.
            message(STATUS "vswhere.exe not found at standard location, downloading...")
            set(_vswhere_download "${CMAKE_BINARY_DIR}/vswhere.exe")
            file(DOWNLOAD
                https://github.com/microsoft/vswhere/releases/download/3.0.3/vswhere.exe
                "${_vswhere_download}"
                TLS_VERIFY ON
                EXPECTED_HASH SHA1=8569081535767af53811f47c0e6abeabd695f8f4
                STATUS _vswhere_status
            )
            list(GET _vswhere_status 0 _vswhere_status_code)
            if("0" EQUAL _vswhere_status_code)
                set(_vswhere_exe "${_vswhere_download}")
                set(_vswhere_found TRUE)
            else()
                list(GET _vswhere_status 1 _vswhere_error)
                message(WARNING "Failed to download vswhere.exe: ${_vswhere_error}")
                set(_vswhere_found FALSE)
            endif()
        endif()

        if(_vswhere_found)
            execute_process(
                COMMAND "${_vswhere_exe}" -all -latest -property productPath
                OUTPUT_VARIABLE DevEnvDir
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if(DevEnvDir STREQUAL "")
                message(WARNING
                    "vswhere.exe could not find a Visual Studio installation.\n"
                    "Ensure Visual Studio 2022 with the 'Desktop development with C++' workload is installed.\n"
                    "If VS is installed, try running cmake from a VS Developer Command Prompt instead."
                )
            else()
                cmake_path(GET DevEnvDir PARENT_PATH DevEnvDir)
                set(ENV{DevEnvDir} ${DevEnvDir})
            endif()
        else()
            # Last resort: assume the standard VS 2022 Community path.
            message(WARNING
                "Could not locate vswhere.exe. Assuming VS 2022 Community Edition.\n"
                "If your VS installation is elsewhere, set the DevEnvDir environment variable\n"
                "to your Visual Studio IDE directory (the folder containing devenv.exe)."
            )
            set(ENV{DevEnvDir} "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/")
        endif()
    endif()

    # Run VsDevCmd.bat and capture the environment variables it sets.
    set(DevEnvDir $ENV{DevEnvDir})
    cmake_path(APPEND DevEnvDir ../Tools/VsDevCmd.bat OUTPUT_VARIABLE VSDEVCMD_BAT)
    cmake_path(NATIVE_PATH VSDEVCMD_BAT NORMALIZE VSDEVCMD_BAT)
    cmake_path(NATIVE_PATH DevEnvDir NORMALIZE DevEnvDir)
    set(ENV{DevEnvDir} ${DevEnvDir})
    set(ENV{VSDEVCMD_BAT} \"${VSDEVCMD_BAT}\")

    if(NOT EXISTS "${VSDEVCMD_BAT}")
        message(FATAL_ERROR
            "VsDevCmd.bat not found at: ${VSDEVCMD_BAT}\n"
            "Ensure Visual Studio 2022 is installed with the 'Desktop development with C++' workload.\n"
            "Alternatively, open cmake from a VS 2022 Developer Command Prompt."
        )
    endif()

    # Use short DOS path names to avoid issues with spaces in the VS install path.
    # See https://gitlab.kitware.com/cmake/cmake/-/issues/16321
    execute_process(COMMAND cmd /c for %A in (%VSDEVCMD_BAT%) do @echo %~sA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE VSDEVCMD_BAT)

    message(STATUS "Running VsDevCmd.bat to bootstrap VS environment...")
    execute_process(COMMAND cmd /c ${VSDEVCMD_BAT} -no_logo -arch=amd64 && set
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE _ENV)

    # Capture only the environment variables that the VS toolchain adds.
    # Derived from: comm -1 -3 <(sort before.txt) <(sort after.txt) | grep -o '^[^=]+'
    set(_replace
        ExtensionSdkDir=
        Framework40Version=
        FrameworkDIR64=
        FrameworkDir=
        FrameworkVersion64=
        FrameworkVersion=
        INCLUDE=
        LIB=
        LIBPATH=
        NETFXSDKDir=
        Path=
        UCRTVersion=
        UniversalCRTSdkDir=
        VCIDEInstallDir=
        VCINSTALLDIR=
        VCToolsInstallDir=
        VCToolsRedistDir=
        VCToolsVersion=
        VS170COMNTOOLS=
        VSCMD_ARG_HOST_ARCH=
        VSCMD_ARG_TGT_ARCH=
        VSCMD_ARG_app_plat=
        VSCMD_VER=
        VSINSTALLDIR=
        VisualStudioVersion=
        WindowsLibPath=
        WindowsSDKLibVersion=
        WindowsSDKVersion=
        WindowsSDK_ExecutablePath_x64=
        WindowsSDK_ExecutablePath_x86=
        WindowsSdkBinPath=
        WindowsSdkDir=
        WindowsSdkVerBinPath=
    )
    string(REGEX REPLACE ";" "\\\\;" _ENV "${_ENV}")
    string(REGEX MATCHALL "[^\n]+\n" _ENV "${_ENV}")
    foreach(_env IN LISTS _ENV)
        string(REGEX MATCH ^[^=]+    _key   "${_env}")
        string(REGEX MATCH =[^\n]+\n _value "${_env}")
        # Skip lines that are not key=value pairs (e.g. spurious cmd output).
        if(NOT _value MATCHES ^=)
            continue()
        endif()
        string(SUBSTRING "${_value}" 1 -1 _value) # Remove leading =
        string(STRIP "${_value}" _value)           # Remove trailing \r
        list(FIND _replace ${_key}= _idx)
        if(-1 EQUAL _idx)
            continue()
        endif()
        list(REMOVE_AT _replace ${_idx})
        set(ENV{${_key}} "${_value}")
        string(REPLACE \\ \\\\ _value "${_value}")
        set(_json_entry "\"${_key}\": \"${_value}\"")
        if("${_MSVC_DEVENV}" STREQUAL "")
            string(APPEND _MSVC_DEVENV "${_json_entry}")
            continue()
        endif()
        string(APPEND _MSVC_DEVENV ",\n        ${_json_entry}")
    endforeach() # _ENV

    message(STATUS "VS environment bootstrapped successfully.")
endif() # VSCMD_VER
