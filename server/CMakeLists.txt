# Samples_Server: the server the net test sample connects to (scores, high score, quiz).

add_subdirectory(third_party/sqlite3)

# The score database on its own, so the tests can link it.
add_library(Samples_ScoreDB STATIC db/score_db.cpp)
target_include_directories(Samples_ScoreDB PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(Samples_ScoreDB PUBLIC sqlite3 Moksha_Options)

moksha_add_server(Samples_Server
    SOURCES
        main.cpp
        net_test_server.cpp
    RES_LIST resources/res_list.json)
target_link_libraries(Samples_Server PRIVATE Samples_ScoreDB Samples_Shared)
