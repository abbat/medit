execute_process(
    COMMAND "${GDK_PIXBUF_CSOURCE}" --static --build-list ${PIXBUF_ARGS}
    OUTPUT_VARIABLE pixbuf_source
    RESULT_VARIABLE result)

if (NOT result EQUAL 0)
    message(FATAL_ERROR "gdk-pixbuf-csource failed with status ${result}")
endif()

file(WRITE "${OUTPUT_FILE}"
     "#ifndef MOO_PIXBUFS_H\n"
     "#define MOO_PIXBUFS_H\n\n"
     "${pixbuf_source}"
     "\n#endif /* MOO_PIXBUFS_H */\n")
