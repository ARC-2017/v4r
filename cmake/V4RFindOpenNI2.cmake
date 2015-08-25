if(WITH_OPENNI2)
  find_package(OpenNI2)
  if(${OpenNI2_FOUND})
    set(OPENNI2_LIBRARIES "${OPENNI2_LIBRARY}")
    set(OPENNI2_INCLUDE_DIRS "${OPENNI2_INCLUDE_DIR}")
    set(HAVE_OPENNI2 TRUE)
  endif()
endif()
