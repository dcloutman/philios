# Getting Started: Building `philios`

This page explains how to compile the `philios` interpreter and run its tests. It assumes that the tools listed in [Required Tools](required-tools.md) are installed.

## Build Procedure
1. Open a Bash terminal and go to the repository root.
   ```bash
   cd path/to/philios
   ```
2. Configure the build directory.
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   ```
3. Compile the code.
   ```bash
   cmake --build build -j4
   ```
   This step builds the `philios` binary and the unit tests. It also runs `clang-tidy` on every source file. The build must finish without warnings.
4. Run the tests.
   ```bash
   ctest --test-dir build
   ```
   Every test must pass.
5. Confirm that the binary exists.
   ```bash
   ls -l build/philios
   ```

## Rebuilding After Changes
After you edit source files, repeat steps 3 and 4. Repeat step 2 only if you change `CMakeLists.txt` or want different build options.

## Build Variants
Each variant uses its own build directory. Run steps 2 through 4 with the directory and options shown below.

| Variant | Directory | Configure command |
|---|---|---|
| Debug | `build` | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` |
| Release | `build-release` | `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release` |
| Sanitizer | `build-asan` | `cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DPHILIOS_SANITIZE=ON` |

The sanitizer variant enables AddressSanitizer and UndefinedBehaviorSanitizer. Tests must pass in all three variants before a change is complete.

## Build Options
- `PHILIOS_CLANG_TIDY` runs `clang-tidy` during the build. It is `ON` by default. Set it to `OFF` only for fast local builds. Verification requires it to be `ON`.
- `PHILIOS_SANITIZE` enables the sanitizers. It is `OFF` by default.
- `BUILD_TESTING` builds the tests. It is `ON` by default. Set it to `OFF` to build only the interpreter without Catch2.

Pass an option to the configure command with `-D`, for example `-DPHILIOS_CLANG_TIDY=OFF`.

## Running `philios`
The interpreter runs the `main.phil` file in the current directory. Go to a project directory and run the binary from there.
```bash
cd path/to/project
path/to/philios/build/philios
```
