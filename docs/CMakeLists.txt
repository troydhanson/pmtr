add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pmtr.html
    COMMAND asciidoc -a toc2 ${CMAKE_CURRENT_SOURCE_DIR}/pmtr.txt
    COMMENT "Running asciidoc"
)

# add a target called "docs" so the maintainer can edit pmtr.txt then:
#
# cd build; cmake ..; make docs
#
# The docs target is not built by default as regenerating the docs is normally
# only done by the maintainer

add_custom_target(
    docs
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/pmtr.txt ${CMAKE_CURRENT_BINARY_DIR}/pmtr.html
)
