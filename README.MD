# Optimi C Logger

## Compiler Information

This library targets modern C compilers and is intended to be built with the C11 standard.

### Supported Compilers

- GCC (recommended: 11+)
- Clang (recommended: 14+)

### Recommended Build Flags

- Standard: `-std=c11`
- Warnings: `-Wall -Wextra -Wpedantic`
- Debug build: `-g -O0`
- Release build: `-O2`

### Variadic Macro Requirements

The `OPTIMI_LOG_*` macros in this project use `##__VA_ARGS__` so calls without
extra format arguments (for example `OPTIMI_LOG_INFO(logger, "ready")`) compile
cleanly.

- This behavior is supported by GCC and Clang in GNU extension mode.
- If strict portability is required across compilers/modes, consider migrating
	macros to the C23 form using `__VA_OPT__(,) __VA_ARGS__`.

Example C23 pattern:

```c
#define OPTIMI_LOG_INFO(logger, fmt, ...) \
		optimi_logger_log((logger), OPTIMI_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, (fmt) __VA_OPT__(,) __VA_ARGS__)
```

### Example Compile Commands

#### GCC

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude -c src/optimi_logger.c -o build/optimi_logger.o
```

#### Clang

```bash
clang -std=c11 -Wall -Wextra -Wpedantic -Iinclude -c src/optimi_logger.c -o build/optimi_logger.o
```
