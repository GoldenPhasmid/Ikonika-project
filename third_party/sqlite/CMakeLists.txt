# SQLite as the official amalgamation: one 9.5 MB sqlite3.c compiled as project source. No package
# manager and no linking work. Version 3.53.4.

add_library(sqlite3 STATIC sqlite3.c)

target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(sqlite3 PUBLIC
    # Full text search, for dictionary autocomplete later.
    SQLITE_ENABLE_FTS5

    # Foreign keys are off per-connection by default, which is a trap. Turn them on at build time
    # so no connection can forget.
    SQLITE_DEFAULT_FOREIGN_KEYS=1

    # Reject double-quoted string literals. SQLite accepts them as a MySQL compatibility quirk,
    # which silently turns a misspelled column name into a string constant.
    SQLITE_DQS=0

    SQLITE_OMIT_DEPRECATED
    SQLITE_THREADSAFE=1
)

# Vendored third-party C. The project's warning flags are not its problem, and it will not be
# edited here.
if (MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
else()
    target_compile_options(sqlite3 PRIVATE -w)
endif()
