# sherpa-onnx 1.13.8, as the prebuilt Windows C API DLL plus ONNX Runtime.
#
# The DLLs and the speech model are NOT in git. Run tools\fetch-native.ps1 first. Only the header is
# committed, which is enough to read but not to build.
#
# Prebuilt from the official Python wheel rather than a C++ archive, because no prebuilt C++ archive is
# published for Windows. The wheel carries exactly these libraries. Using the C API means MinGW links
# the DLL directly with no import library and no ABI concern, the same arrangement as eSpeak.
#
# MSVC cannot link a DLL directly and needs a .lib import library instead. The wheel ships an
# MSVC-format one alongside the DLL, which fetch-native.ps1 also copies in.

set(SHERPA_ONNX_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(SHERPA_ONNX_C_API "${SHERPA_ONNX_DIR}/sherpa-onnx-c-api.dll")
set(SHERPA_ONNX_C_API_IMPLIB "${SHERPA_ONNX_DIR}/sherpa-onnx-c-api.lib")
set(SHERPA_ONNX_RUNTIME "${SHERPA_ONNX_DIR}/onnxruntime.dll")

if (NOT EXISTS "${SHERPA_ONNX_C_API}")
    message(FATAL_ERROR
        "sherpa-onnx libraries are missing. Run tools\\fetch-native.ps1 to download them.")
endif()

if (MSVC AND NOT EXISTS "${SHERPA_ONNX_C_API_IMPLIB}")
    message(FATAL_ERROR
        "sherpa-onnx-c-api.lib is missing. Run tools\\fetch-native.ps1 to download it.")
endif()

add_library(sherpa-onnx SHARED IMPORTED GLOBAL)

# MinGW links the DLL directly and has no import library at all; MSVC cannot link a DLL and
# needs the .lib fetched above.
if (MSVC)
    set(SHERPA_ONNX_IMPLIB "${SHERPA_ONNX_C_API_IMPLIB}")
else()
    set(SHERPA_ONNX_IMPLIB "${SHERPA_ONNX_C_API}")
endif()

set_target_properties(sherpa-onnx PROPERTIES
    IMPORTED_LOCATION "${SHERPA_ONNX_C_API}"
    IMPORTED_IMPLIB "${SHERPA_ONNX_IMPLIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${SHERPA_ONNX_DIR}/include"
)

# Consumed by the app and the tests, which have to place the libraries and the models where the
# executable can find them at run time.
#
# Two models live under models/, both loaded at once and switched between at run time. The directory
# names are the contract between the fetch script, CMake and the app.
set(SHERPA_ONNX_DLLS "${SHERPA_ONNX_C_API}" "${SHERPA_ONNX_RUNTIME}" CACHE INTERNAL "sherpa dlls")
set(SHERPA_ONNX_MODELS_DIR "${SHERPA_ONNX_DIR}/models" CACHE INTERNAL "speech models")
