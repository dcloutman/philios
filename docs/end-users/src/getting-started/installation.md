# Getting Started: Installation

Philios is in early development. Philios has no prebuilt binaries yet. To use Philios, compile the `philios` interpreter from source.

## Supported Platforms
- Philios currently supports Linux on x86-64 with GCC 13 or later.
- Other POSIX systems and compilers may work, but they are not tested.
- On Windows, build and run Philios inside the Windows Subsystem for Linux (WSL).

## Compiling `philios`
1. Install the tools listed in [Required Tools](../../../developers/src/getting-started/required-tools.md).
2. Get the source code from the [Philios repository](https://github.com/dcloutman/philios).
3. From the repository's root directory, run these commands:

   ```bash
   cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
   cmake --build build-release -j4
   ```

4. The interpreter is the file `build-release/philios`. Copy it to a directory on your `PATH`, or run it by its full path.

The developer guide, [Building `philios`](../../../developers/src/getting-started/compiling-philios.md), describes other build variants and options.

## Next Step
Write and run [Your First Program](first-program.md).
