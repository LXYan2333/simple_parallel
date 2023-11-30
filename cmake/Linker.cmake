macro(simple_parallel_configure_linker project_name)
    include(CheckCXXCompilerFlag)

    set(USER_LINKER_OPTION
        "lld"
        CACHE STRING "Linker to be used")
    set(USER_LINKER_OPTION_VALUES "lld" "gold" "bfd" "mold")
    set_property(CACHE USER_LINKER_OPTION PROPERTY STRINGS ${USER_LINKER_OPTION_VALUES})
    list(
        FIND
        USER_LINKER_OPTION_VALUES
        ${USER_LINKER_OPTION}
        USER_LINKER_OPTION_INDEX)

    if(${USER_LINKER_OPTION_INDEX} EQUAL -1)
        message(
        STATUS
            "Using custom linker: '${USER_LINKER_OPTION}', explicitly supported entries are ${USER_LINKER_OPTION_VALUES}")
    endif()

    if(NOT simple_parallel_ENABLE_USER_LINKER)
        return()
    endif()

    set(LINKER_FLAG "-fuse-ld=${USER_LINKER_OPTION}")

    check_cxx_compiler_flag(${LINKER_FLAG} CXX_SUPPORTS_USER_LINKER)
    if(CXX_SUPPORTS_USER_LINKER)
        add_link_options("-fuse-ld=${USER_LINKER_OPTION}")
        if(${USER_LINKER_OPTION} STREQUAL "lld")
            # https://clang.llvm.org/docs/ThinLTO.html
            # TBB 的问题，https://github.com/madler/zlib/issues/856
            add_link_options("-Wl,--thinlto-cache-dir=cache/ThinLTO/" "-Wl,--undefined-version")
        endif()
        message(STATUS "Using custom linker: '${USER_LINKER_OPTION}'")
    endif()
endmacro()
