execute_process(
    COMMAND "${GDK_PIXBUF_CSOURCE}" --static --build-list ${PIXBUF_ARGS}
    OUTPUT_VARIABLE pixbuf_source
    RESULT_VARIABLE result)

# RESULT_VARIABLE holds the exit status as a number when the tool ran and an
# error string when it could not be launched at all, so compare as a string:
# EQUAL would treat "No such file or directory" as 0 and let the build go on
# with an empty header.
if (NOT result STREQUAL "0")
    message(FATAL_ERROR "gdk-pixbuf-csource failed with status ${result}")
endif()

file(WRITE "${OUTPUT_FILE}"
     "#ifndef MOO_PIXBUFS_H\n"
     "#define MOO_PIXBUFS_H\n\n"
     "${pixbuf_source}"
     "\n#endif /* MOO_PIXBUFS_H */\n")
