# Well-formedness check for the xml that does not go through xml-stripblanks.
# Run in script mode: cmake -DXMLLINT=... -DFILES=a;b -P cmake/ValidateXml.cmake
#
# xmllint is asked for well-formedness only. It also reports namespace errors,
# and the user tool descriptions collect a lot of them: menu.xml and context.xml
# spell elements exe:input, exe:output and exe:code, where "exe" is the command
# type from <type>exe</type> and not an xml namespace at all -- moousertools.cpp
# dispatches on the colon (line 754) and hands the element to the factory named
# before it. Declaring an xmlns to quiet the validator would say something about
# the format that is not true, so those diagnostics are dropped instead. xmllint
# exits 0 on them anyway; what it exits non-zero on, and what this is here to
# catch, is markup that is actually broken.

foreach(file ${FILES})
    execute_process(
        COMMAND "${XMLLINT}" --noout "${file}"
        RESULT_VARIABLE result
        ERROR_VARIABLE stderr
        OUTPUT_QUIET)

    # Keep only what is not a namespace complaint, and not the source line and
    # caret that xmllint prints under each one.
    set(kept "")
    string(REPLACE "\n" ";" lines "${stderr}")
    set(skip FALSE)
    foreach(line ${lines})
        if(line MATCHES "namespace error")
            set(skip TRUE)
        elseif(skip AND line MATCHES "^ *\\^ *$")
            set(skip FALSE)
        elseif(NOT skip AND NOT line STREQUAL "")
            set(kept "${kept}${line}\n")
        endif()
    endforeach()

    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${file} is not well-formed xml:\n${stderr}")
    elseif(NOT kept STREQUAL "")
        message(FATAL_ERROR "${file}:\n${kept}")
    endif()
endforeach()
