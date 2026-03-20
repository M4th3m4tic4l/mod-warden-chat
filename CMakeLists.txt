if(NOT ENABLE_MODULES)
    return()
endif()

CollectSourceFiles(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE_SOURCES
)

GroupSources(${CMAKE_CURRENT_SOURCE_DIR})

add_library(mod-warden-chat ${PRIVATE_SOURCES})

target_link_libraries(mod-warden-chat
    PRIVATE
        game-interface
)

target_include_directories(mod-warden-chat
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(mod-warden-chat
    PROPERTIES
        FOLDER "modules"
)
