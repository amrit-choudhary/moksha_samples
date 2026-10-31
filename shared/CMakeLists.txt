# Code shared by the samples and the samples' server: the samples' messages and the rally rules.

add_library(Samples_Shared STATIC net/samples_protocol.cpp)

target_include_directories(Samples_Shared PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(Samples_Shared PUBLIC Moksha_Shared)
