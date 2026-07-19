# Contributing to eterOS

We welcome contributions to eterOS! Please read these guidelines before submitting a pull request.

## Code Style

- Use standard C conventions.
- Prefer 4 spaces for indentation (no tabs).
- Limit line length to 120 characters where practical.
- Use explicit types (e.g., `uint32_t`, `int64_t`) from `<stdint.h>` instead of ambiguous basic types when size matters.
- Always use bounded string functions (`strlcpy`, `strlcat`, `snprintf`) instead of their unbounded counterparts (`strcpy`, `strcat`, `sprintf`).

## Pull Request Process

1. **Ensure Tests Pass**: Run `make test` or execute `./tests/run_tests.sh` and `./tests/run_integration.sh`. Do not submit code that breaks existing tests.
2. **Pre-commit Checks**: Ensure you have run all necessary pre-commit verification steps, including checking for compiler warnings.
3. **Commit Messages**: Write clear, concise commit messages summarizing the "what" and "why" of the change.

## Architecture

Please familiarize yourself with the [Filesystem Architecture](kernel/fs/README.md) and the [Memory Management](kernel/mm/README.md) documents before modifying core subsystems.
