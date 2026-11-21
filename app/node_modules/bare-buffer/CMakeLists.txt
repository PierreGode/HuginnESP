cmake_minimum_required(VERSION 4.0)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)
find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)

project(bare_buffer C)

fetch_package("github:holepunchto/libutf#b22ffb0")
fetch_package("github:holepunchto/libbase64#0941b4b")
fetch_package("github:holepunchto/libhex#7c3e8b1")

add_bare_module(bare_buffer)

target_sources(
  ${bare_buffer}
  PRIVATE
    binding.c
)

target_link_libraries(
  ${bare_buffer}
  PRIVATE
    $<TARGET_OBJECTS:utf>
    $<TARGET_OBJECTS:base64>
    $<TARGET_OBJECTS:hex>
  PUBLIC
    utf
    base64
    hex
)
