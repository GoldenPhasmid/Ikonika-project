add_library(miniz STATIC miniz.c)

target_include_directories(miniz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# miniz writes the whole archive API unless told otherwise; the reader only needs zip
# reading, but the library has no per-function toggle worth maintaining here.
target_compile_options(miniz PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W3>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>
)
