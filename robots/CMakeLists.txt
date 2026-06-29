# All robot implementations are always compiled into the unified binary.
# The correct robot is selected at runtime via hardware auto-detection (Stage 1)
# or via the MetaService Robot Firmware register (Stage 2).
file(GLOB_RECURSE PLATFORM_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*")
