cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Keep format warnings visible, but do not fail the build on them.
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=format" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=format-security" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=maybe-uninitialized" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=uninitialized" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=return-type" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=tautological-compare" APPEND)
# Do not fail the build on warnings from bundled components.
idf_build_set_property(COMPILE_OPTIONS "-Wno-error" APPEND)

add_compile_options(-fdiagnostics-color=always -w)

project(cavemens_best_friend)
