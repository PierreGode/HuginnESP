cmake_minimum_required(VERSION 4.0)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)
find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)

project(bare_url C)

fetch_package("github:holepunchto/libutf#b22ffb0")
fetch_package("github:holepunchto/libpunycode#e39f155")
fetch_package("github:holepunchto/libnormalize#dce9fc1")
fetch_package("github:holepunchto/libidna#5d240eb")
fetch_package("github:holepunchto/liburl#0bfdd4c")

add_bare_module(bare_url)

target_sources(
  ${bare_url}
  PRIVATE
    binding.c
)

target_link_libraries(
  ${bare_url}
  PRIVATE
    $<TARGET_OBJECTS:utf>
    $<TARGET_OBJECTS:punycode>
    $<TARGET_OBJECTS:normalize>
    $<TARGET_OBJECTS:idna>
    $<TARGET_OBJECTS:url>
  PUBLIC
    utf
    punycode
    normalize
    idna
    url
)
