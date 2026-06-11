# Optimi C Logger

## Compiler Information

This library targets modern C compilers and is intended to be built with the C11 standard.

### Variadic Macro Requirements

The `OPTIMI_LOG_*` macros in this project use `##__VA_ARGS__` so calls without
extra format arguments (for example `OPTIMI_LOG_INFO(logger, "ready")`) compile
cleanly.

- This behavior is supported by GCC and Clang in GNU extension mode.
- If strict portability is required across compilers/modes, consider migrating
	macros to the C23 form using `__VA_OPT__(,) __VA_ARGS__`.

## Tests

The repository includes header contract tests under `tests/`.

Run them with:

```bash
make -C tests test
```
