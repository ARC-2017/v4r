add_definitions(-D__V4R_BUILD=1)

if(NOT V4R_MODULES_PATH)
  set(V4R_MODULES_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

v4r_glob_modules(${V4R_MODULES_PATH} EXTRA ${V4R_EXTRA_MODULES_PATH})
