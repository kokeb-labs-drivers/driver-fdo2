# Runs the host example and compares its console output with examples/host/expected_output.txt.
#   -DEXE=<fdo2_host_example> -DEXPECTED=<expected_output.txt>
execute_process(COMMAND ${EXE} OUTPUT_VARIABLE actual RESULT_VARIABLE code)
file(READ ${EXPECTED} expected)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "the host example exited with ${code}:\n${actual}")
endif()
if(NOT actual STREQUAL expected)
  message(FATAL_ERROR "the host example output differs from ${EXPECTED}\n--- actual ---\n${actual}")
endif()
