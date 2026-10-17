# CMakeLists.txt for the SQLite amalgamation.

add_library(sqlite3 STATIC "${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.c")

target_include_directories(sqlite3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Minimal feature set: single-threaded, no extensions, no UTF-16, no legacy/unused APIs.
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=0
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_DQS=0
    SQLITE_MAX_EXPR_DEPTH=0
    SQLITE_LIKE_DOESNT_MATCH_BLOBS
    SQLITE_USE_ALLOCA
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_SHARED_CACHE
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_UTF16
    SQLITE_OMIT_DECLTYPE
    SQLITE_OMIT_PROGRESS_CALLBACK
    SQLITE_OMIT_TRACE
    SQLITE_OMIT_COMPLETE
    SQLITE_OMIT_GET_TABLE
    SQLITE_OMIT_TCL_VARIABLE
)

# Vendored code: silence its warnings.
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
else()
    target_compile_options(sqlite3 PRIVATE -w)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(sqlite3 PUBLIC m)
endif()
