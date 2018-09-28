include_directories(${MONGOC_INCLUDE_DIRS}
        ${MONGOCXX_INCLUDE_DIRS}
        ${BSON_INCLUDE_DIRS}
        ${BSONCXX_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-Wno-sign-compare)

add_library(mongo_seastar
        hack.cc
        mongo-seastar.cc)

target_link_libraries(mongo_seastar PUBLIC
        ${MONGOCXX_LIBRARY}
        ${BSONCXX_LIBRARY}
        ${MONGOC_LIBRARY}
        ${BSON_LIBRARY}
        ${SSL_LIBRARY}
        ${CRYPTO_LIBRARY}
        seastar
        m
        snappy
        rt
        resolv
        z)

add_subdirectory(example)