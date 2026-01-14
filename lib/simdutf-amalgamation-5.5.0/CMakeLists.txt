set(src
  src/simdutf/simdutf.cpp
  )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
add_library(simdutf-static STATIC ${src})
