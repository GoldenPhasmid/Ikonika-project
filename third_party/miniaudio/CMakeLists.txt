# miniaudio as a single header. Only raw device playback is wanted, so the file loading, encoding,
# generation and high level engine layers are all switched off. That cuts a large amount of compile
# time out of a 4 MB header.
#
# The data converter stays on: the models produce 22050 Hz and 44100 Hz mono, and the device may not
# accept a given rate directly, in which case miniaudio converts on the way to the backend.

add_library(miniaudio STATIC miniaudio.c)

target_include_directories(miniaudio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(miniaudio PUBLIC
    MA_NO_DECODING
    MA_NO_ENCODING
    MA_NO_GENERATION
    MA_NO_RESOURCE_MANAGER
    MA_NO_NODE_GRAPH
    MA_NO_ENGINE
)

if (IOS)
    set_source_files_properties(miniaudio.c PROPERTIES COMPILE_OPTIONS "-x;objective-c")
    target_compile_definitions(miniaudio PUBLIC MA_NO_RUNTIME_LINKING)
    target_link_libraries(miniaudio PUBLIC
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AVFoundation")
endif()

# Vendored third-party code. The project's warning flags are not its problem.
if (MSVC)
    target_compile_options(miniaudio PRIVATE /W0)
else()
    target_compile_options(miniaudio PRIVATE -w)
endif()
