# ONNX Runtime C API, header only.
#
# The DLL itself is not fetched here: sherpa-onnx 1.13.8 already ships onnxruntime.dll and fetch-native.ps1
# copies it into third_party\sherpa-onnx. The two must agree on version, because the C API is versioned
# by ORT_API_VERSION. The header is fetched pinned to the same onnxruntime release as the wheel.
#
# There is no import library and nothing to link. The translator loads onnxruntime.dll by name at run
# time and takes the API through function pointers from OrtGetApiBase, so this target only supplies the
# include directory.

add_library(onnxruntime-c-api INTERFACE)
target_include_directories(onnxruntime-c-api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
