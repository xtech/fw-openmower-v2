
set(COMPONENT_ADD_INCLUDEDIRS csrc)
file(GLOB COMPONENT_SRCS u8g2-master/csrc/*.c)
file(GLOB COMPONENT_HDRS u8g2-master/csrc/*.h)
file(GLOB COMPONENT_CPPSRCS u8g2-master/cppsrc/*.cpp)
file(GLOB COMPONENT_CPPHDRS u8g2-master/cppsrc/*.h)

add_library(u8g2 ${COMPONENT_SRCS} ${COMPONENT_CPPSRCS})
target_include_directories(u8g2 PUBLIC u8g2-master/cppsrc u8g2-master/csrc)