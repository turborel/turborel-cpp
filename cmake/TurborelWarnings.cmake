# Compiler warnings for the project's own targets (RULES.md R-LINT-1), and
# the sanitizer and coverage instrumentation (RULES.md D9, D10). Third-party
# targets get none of these: their warnings are not ours to fix.
include_guard(GLOBAL)

# Warnings shared by GCC and Clang.
set(TURBOREL_CPP_GNU_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
    -Wfloat-equal
    -Wextra-semi)

# Warnings only GCC has.
set(TURBOREL_CPP_GCC_WARNINGS -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op
                              -Wuseless-cast)

# Clang checks doc comments against the declarations they document (R-DOC-1).
set(TURBOREL_CPP_CLANG_WARNINGS -Wdocumentation -Wdocumentation-unknown-command)

# MSVC's level 4, standard conformance, and source read as UTF-8.
set(TURBOREL_CPP_MSVC_WARNINGS /W4 /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor)

# Gives `target` the project's warnings, for its compiler.
function(turborel_cpp_warnings target)
  target_compile_options(
    ${target}
    PRIVATE "$<$<CXX_COMPILER_ID:GNU>:${TURBOREL_CPP_GNU_WARNINGS};${TURBOREL_CPP_GCC_WARNINGS}>"
            "$<$<CXX_COMPILER_ID:Clang,AppleClang>:${TURBOREL_CPP_GNU_WARNINGS};${TURBOREL_CPP_CLANG_WARNINGS}>"
            "$<$<CXX_COMPILER_ID:MSVC>:${TURBOREL_CPP_MSVC_WARNINGS}>")
endfunction()

# Gives `target` the sanitizers or coverage instrumentation asked for. A
# sanitized or measured library is linked with the same flags, so they are
# public.
function(turborel_cpp_instrument target)
  if(TURBOREL_CPP_SANITIZE)
    if(MSVC)
      # MSVC has AddressSanitizer alone.
      target_compile_options(${target} PUBLIC /fsanitize=address)
    else()
      set(flags -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
      target_compile_options(${target} PUBLIC ${flags})
      target_link_options(${target} PUBLIC ${flags})
    endif()
  endif()
  if(TURBOREL_CPP_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      message(FATAL_ERROR "[DESIGN] RULES.md D10: coverage is measured with Clang, not ${CMAKE_CXX_COMPILER_ID}")
    endif()
    set(flags -fprofile-instr-generate -fcoverage-mapping)
    target_compile_options(${target} PUBLIC ${flags})
    target_link_options(${target} PUBLIC ${flags})
  endif()
endfunction()
