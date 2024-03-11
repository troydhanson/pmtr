add_executable(pmtr job.c job.h net.c net.h tok.c pmtr.c pmtr.h cfg.c cfg.h)
add_executable(onconnect onconnect.c)
include_directories("include")
install(TARGETS pmtr onconnect)

# This just copies the pmtr.service into the build directory
# so that its already in the user's working directory if they
# want to go further and install it on their own
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/pmtr.service
  ${CMAKE_BINARY_DIR}/pmtr.service
  COPYONLY
)

