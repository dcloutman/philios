# Getting Started: Building `philios`

This page explains how to compile the `philios` interpreter and run its tests.

## Build Procedure
1. Open a Bash terminal and go to the repository root.
2. Configure the build directory.
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   ```
3. Compile the code.
   ```bash
   cmake --build build -j4
   ```
4. Run the tests.
   ```bash
   ctest --test-dir build -j8
   ```
5. Confirm that the binary exists.
   ```bash
   ls -l build/philios
   ```

## Build Variants
| Variant | Directory | Configure command |
|---|---|---|
| Debug | `build` | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` |
| Release | `build-release` | `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release` |
| Sanitizer | `build-asan` | `cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DPHILIOS_SANITIZE=ON` |

## Running `philios`
The interpreter runs `main.phil` in the current directory:

```bash
cd path/to/project
/path/to/philios/build/philios
```

## CLI golden tests and stdout checks
CLI acceptance tests are defined in `tests/cli/cli_cases.cmake` and executed through `tests/cli/run_case.cmake`.

Plan 0002 added exact stdout checks and stdout destination modes for `philios_cli_stdout_test`:
- `EXPECT_STDOUT_HEX=<hex>`: require exact stdout bytes (capture mode only).
- `STDOUT_MODE=capture|dev_full|closed|broken_pipe`: control where stdout goes when running the case.

Task 6 also added `MODE copy_folder` plus `FIXTURE_DIR` in the driver for fixture directories such as `tests/unit/sample-programs/hello-world/`.
