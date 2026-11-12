# Testing and CI

How the driver is checked: host tests against a model of the module, fuzzers that corrupt what the module sends,
compiled documentation, an install test, ESP32 and Cortex-M builds, static analysis and measured quality gates, on
every pull request.

## Host tests

The tests build the real driver against a model of the module in
[`tests/mock_fdo2.hpp`](../tests/mock_fdo2.hpp): a clock, a receive buffer, the thirteen commands, the status
word, the checksum, the broadcast stream and injected faults (silence, corrupted bytes, a late reply, a lost echo,
a wrong baud rate, every `#ERRO` code). They build with the firmware's flags, `-fno-exceptions -fno-rtti`, and
warnings as errors, on GCC and Clang.

```bash
cmake -S . -B build -DFDO2_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

| File | Covers |
| --- | --- |
| [`tests/test_crc.cpp`](../tests/test_crc.cpp), [`test_parse.cpp`](../tests/test_parse.cpp), [`test_assembler.cpp`](../tests/test_assembler.cpp), [`test_encode.cpp`](../tests/test_encode.cpp) | The protocol layer alone: CRC-16, grammar, line assembly, command encoding |
| [`tests/test_status.cpp`](../tests/test_status.cpp), [`test_conversions.cpp`](../tests/test_conversions.cpp), [`test_capabilities.cpp`](../tests/test_capabilities.cpp), [`test_validate.cpp`](../tests/test_validate.cpp) | The status word and policies, unit conversions, firmware capabilities, `Config` bounds |
| [`tests/test_lifecycle.cpp`](../tests/test_lifecycle.cpp), [`test_faults.cpp`](../tests/test_faults.cpp), [`test_errors.cpp`](../tests/test_errors.cpp) | Start-up, retries, every error path; every chip-specific `Error` is returned by some call |
| [`tests/test_flash.cpp`](../tests/test_flash.cpp), [`test_calibration.cpp`](../tests/test_calibration.cpp) | Consent, the write budget, never repeating a flash write, calibration |
| [`tests/test_modes.cpp`](../tests/test_modes.cpp) | The checksum, the baud rate, resynchronising, the broadcast stream |
| [`tests/test_safety.cpp`](../tests/test_safety.cpp) | An aborted or timed-out start-up never leaves the driver initialised without an identity; no power cut or `Deinitialize()` during a flash write; `Resync()` after a lost `#CRCE` echo; the guard before every flash write (a module reset the driver did not see, a loopback, another module); `#ERRO` on a flash write; stale getters; stream liveness |
| [`tests/test_ports.cpp`](../tests/test_ports.cpp) | Ports with and without each optional hook |
| [`tests/esp32_stub/`](../tests/esp32_stub/esp32_port_test.cpp) | The ESP32 example port compiled on the host against stubs of the ESP-IDF headers (`fdo2_esp32_port_test`): the default power pin -1 is a valid no-op, `DelayUs` yields |
| [`tests/test_model.cpp`](../tests/test_model.cpp) | The model against the driver's constants |
| [`tests/test_version.cpp`](../tests/test_version.cpp), [`test_kconfig.cpp`](../tests/test_kconfig.cpp) | Version getters, `KconfigDefaults()` |
| [`tests/compile_fail/`](../tests/compile_fail/not_a_port.cpp) | Code that must not compile, each for its `EXPECT:` diagnostic: a port without a required hook, a hook with a narrowed parameter or without `noexcept`, a `Poll()` of the wrong form, an invalid `CheckedConfig` |
| [`tests/docs/`](../tests/docs/my_port.hpp) | Every code block on this site, compiled against the driver |
| [`examples/host/`](../examples/host/README.md) | The host example, built and run; its output is compared with `expected_output.txt` |
| [`tests/check_install.cmake`](../tests/check_install.cmake) | Installs into a scratch prefix and builds [`tests/install_consumer/`](../tests/install_consumer/CMakeLists.txt) with `find_package(fdo2)` |
| [`tests/check_doxygen.cmake`](../tests/check_doxygen.cmake) | Doxygen with `_config/Doxyfile` and warnings as errors, when `doxygen` is installed |

### The model stands alone

[`tests/mock_fdo2.hpp`](../tests/mock_fdo2.hpp) is written from the datasheets, not from the driver: it includes
none of the driver's headers. Its commands, status bits, timing and CRC are its own, each with the datasheet
section, so a wrong constant in the driver cannot hide behind the same wrong constant in the model.
[`tests/test_model.cpp`](../tests/test_model.cpp) compares the two. Where the datasheets leave a behaviour open,
the model has a switch for each reading (a knob), and the tests run the driver against each.

The model also keeps a count the driver cannot see: the writes to the module's flash that ran without a `#VERS` or
`#MOXY` of the driver answered since the module's last reset, seen or unseen (`UnseenReset()`), and whether the
supply was cut during a flash cycle. The tests and the fuzzer assert that both stay zero.

## Fuzz tests

Everything the driver reads comes from the module over the line, so a noisy cable or a broken module controls it.
Two libFuzzer targets exist:

- [`tests/fuzz/fuzz_fdo2.cpp`](../tests/fuzz/fuzz_fdo2.cpp) chooses a configuration, then up to 64 driver calls, and
  on every read what the line does (pass it, drop it, corrupt it, delay it), or a port whose `Read()` returns the
  fuzz bytes themselves. After every call it checks the driver's promises and aborts, naming the line, when one
  breaks: every returned `Error` is a defined value, the module's flash is never written without consent, its write
  count never passes the budget, every command written is one of the thirteen, a blocking call returns within a
  bounded time and a successful measurement is inside the configured windows. The invariants are numbered F1 to
  F14 in the file's header: besides those above, a flash write is only executed by a module the driver has
  identified since its last reset, seen or unseen (F11), nothing is sent while uninitialised (F12) or while the
  state is unknown (F13), and the supply is never cut during a flash cycle (F14).
  Not all of the properties one might want are fuzzed: the design's wider list (for example an independent
  oracle for every decoded value) is covered by the example-based tests, not by the fuzzer.
- [`tests/fuzz/fuzz_fdo2_parse.cpp`](../tests/fuzz/fuzz_fdo2_parse.cpp) feeds the parser, the line assembler and the
  encoder arbitrary bytes and checks their invariants P1 to P7, among them that `EncodeCommand()` and `FindCommand()`
  never read outside their tables, whatever the `Command` value or the letter count.

```bash
# libFuzzer needs Clang (on macOS, Homebrew's LLVM)
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DFDO2_BUILD_TESTS=ON -DFDO2_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_fdo2 fuzz_fdo2_parse
./build-fuzz/tests/fuzz/fuzz_fdo2 -max_total_time=120 tests/fuzz/corpus/fuzz_fdo2
```

The same files also build without libFuzzer as replay programs, and CTest replays every input in
[`tests/fuzz/corpus/`](../tests/fuzz/corpus) on GCC and Clang: the seeds, and every input that ever broke a
promise (name it `regress-*`), so a fixed bug stays fixed. CI fuzzes for 120 s per target on every pull request.

## Quality gates

The `Host quality` job measures the library code, `inc/` and `src/` only (tests and examples are left out), and
fails when a value is over the limits of this repository's assurance level, `high` (set in
[`.kokeb/handbook.yml`](../.kokeb/handbook.yml), defined by the handbook's ADR-0020):

| Gate | Tool and how it is measured | Limit at `high` |
| --- | --- | --- |
| Complexity | lizard 1.24.1, modified cyclomatic complexity (`-m`: a `switch` counts once) per function | CCN at most 15 |
| Function length | lizard, lines of code per function | at most 60 |
| Parameters | lizard, per function | at most 6 |
| Line coverage | GCC 14 `--coverage -O0`, every CTest, gcovr 8.7 `--merge-lines` (a template's lines count once across its instances), `--exclude-unreachable-branches --exclude-throw-branches` | at least 85 % |
| Branch coverage | same run | at least 75 % |
| MC/DC | GCC `-fcondition-coverage`, same run | measured, not gated at `high` (100 % at `safety`) |
| Sanitizers | Clang, `-fsanitize=address,undefined -fno-sanitize-recover=all`, every CTest | no report |
| Fuzzing | libFuzzer with ASan and UBSan, 120 s per target | no broken promise |

Measured on the host build with these gates (GCC 14, `inc/` and `src/`, 30 CTest tests passing):

| File | Lines | Branches | Conditions (MC/DC input) |
| --- | --- | --- | --- |
| `inc/fdo2.hpp` | 100 % (29/29) | 75 % (9/12) | 75 % (9/12) |
| `inc/fdo2_protocol.hpp` | 97 % (357/368) | 95 % (332/348) | 95 % (295/308) |
| `inc/fdo2_types.hpp` | 94 % (142/150) | 92 % (125/135) | 92 % (105/114) |
| `src/fdo2.ipp` | 94 % (1081/1144) | 88 % (656/739) | 88 % (598/676) |
| Total | 95 % (1614/1696) | 90 % (1122/1234) | 90 % (1007/1110) |

The condition column is gcovr's count of condition outcomes seen by GCC's `-fcondition-coverage`; it is
measured, not gated at `high`.

The reports are in the run's `host-quality-reports` artifact and its step summary. To run the gates locally:

```bash
# Coverage (GCC 14 or later), then the complexity limits
cmake -S . -B build-cov -DCMAKE_CXX_COMPILER=g++-14 -DFDO2_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="--coverage -O0 -fcondition-coverage"
cmake --build build-cov && ctest --test-dir build-cov
gcovr --root . --filter inc/ --filter src/ --merge-lines --exclude-unreachable-branches --txt build-cov
lizard -m -C 15 -L 60 -a 6 inc src

# Sanitizers (Clang)
cmake -S . -B build-san -DCMAKE_CXX_COMPILER=clang++ -DFDO2_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g"
cmake --build build-san && ctest --test-dir build-san --output-on-failure
```

The fuzz gate runs locally as shown in [Fuzz tests](#fuzz-tests).

clang-tidy 20 ([`_config/.clang-tidy`](../_config/.clang-tidy): bugprone, CERT, clang-analyzer,
cppcoreguidelines, modernize, performance, portability and readability, each disabled check with its reason) and
cppcheck (strict) run in the `C++ lint` and `Repository checks` jobs and fail on any finding.

The `MISRA` job ([`ci-misra.yml`](../.github/workflows/ci-misra.yml)) runs the shared `ru-misra` workflow of
ci-tools at level `high`: it traces a host build and the Cortex-M build (`fdo2_optimised_build`) of every
translation unit, checks them against MISRA C++:2023 and the handbook's C++20 profile (the CodeQL coding-standards
and profile packs, plus the validated clang-tidy and cppcheck checks), and reports each finding against the
handbook's compliance matrix and deviation records. A finding is fixed in the code; a deliberate exception names
its deviation on the line before it (`// codeql::misra_deviation_next_line(MD-CPP-009)` above the `.ipp` include
of `inc/fdo2.hpp`). Until the repository is public the job runs with `open_source_prerelease: true`, the
declaration that the code is released under the licence in `LICENSE` and is to be published.

## Documentation snippets

Every C++ block on this site is included from [`tests/docs/`](../tests/docs/my_port.hpp), which the host build
compiles (target `fdo2_doc_snippets`) with the driver instantiated over the documented port. A change to
the API that breaks a page breaks the build first. The README's example is a generated copy of the same file;
`kokeb-docs check` fails when it drifts.

## CI workflows

Everything runs from [`ci.yml`](../.github/workflows/ci.yml), which calls one workflow file per check and ends in
the `CI gate` job, the single required check (`CI gate / gate`).

| Job | What it checks |
| --- | --- |
| Host tests | GCC, then Clang (one job), build and run every test above, then build and run `examples/host` on its own |
| Host quality | The gates above: complexity, coverage, sanitizers, fuzzing |
| Cross-compile | `arm-none-eabi-g++` for Cortex-M0+, M4F and M7F with the strict flags, warnings as errors (nothing linked) |
| C++ lint | clang-format and clang-tidy over the sources |
| MISRA | MISRA C++:2023 and the C++20 profile on traced host and Cortex-M builds, at level `high` (see [Quality gates](#quality-gates)) |
| Docs | Strict site build (`_config/mkdocs.yml`), figure and generated-region drift, Doxygen (`_config/Doxyfile`), offline link check, Lighthouse |
| Repository checks | One job: DCO on pull requests, the org standard, Markdown and YAML lint, the [Kokeb Labs handbook](https://github.com/kokeb-labs/handbook) rules for `.kokeb/handbook.yml` (by rule ID), template drift (warns only) and cppcheck over the driver sources; a second job runs the handbook's source checks (include guards, bus requirements, raw access) |
| CI gate | Passes only if every job above passed: the one required check |

The **ESP32 examples** job builds every app in `examples/esp32/app_config.yml`, for each listed ESP-IDF version and
build type, with the repository root as the driver's component, and lints the example sources. The gate needs it
too.

The workflows call the shared, versioned workflows of
[kokeb-labs/ci-tools](https://github.com/kokeb-labs/ci-tools) at `@v1`.

## On hardware

Not verified on hardware: no module has been connected to this driver. An MCU example ([Examples](examples.md)) is
the bench test: it logs the oxygen partial pressure and the temperature once a second. Put the module in air and
check that the partial pressure is about 210 hPa at sea level; put it in nitrogen and check that it falls towards
zero. Record the board and the result in the pull request's bench-status section.
