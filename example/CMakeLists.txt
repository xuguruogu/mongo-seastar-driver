add_executable(mongo_seastar_hello
        mongo_seastar_hello.cc)

target_link_libraries(mongo_seastar_hello
        mongo_seastar)

add_executable(mongo_seastar_pool
        mongo_seastar_pool.cc)

target_link_libraries(mongo_seastar_pool
        mongo_seastar)

add_executable(mongo_seastar_create
        mongo_seastar_create.cc)

target_link_libraries(mongo_seastar_create
        mongo_seastar)

add_executable(mongo_seastar_query_projection
        mongo_seastar_query_projection.cc)

target_link_libraries(mongo_seastar_query_projection
        mongo_seastar)