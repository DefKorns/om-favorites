# Options Menu - Options Menu Favorites

[![Build](https://github.com/DefKorns/om-favorites/actions/workflows/build.yml/badge.svg)](https://github.com/DefKorns/om-favorites/actions/workflows/build.yml)

Add-on for [Options Menu](https://github.com/DefKorns/OptionsMenu).

## Build

```sh
./build.sh
```

Builds `out/om-favorites.hmod` inside the `classicmini-cross-toolchain` Docker image. The version comes from the latest `v*` tag.

## Contributing

[Report a bug](https://github.com/DefKorns/om-favorites/issues/new?template=bug_report.yml) or [suggest a feature](https://github.com/DefKorns/om-favorites/issues/new?template=feature_request.yml); pull requests are welcome too:

- **Formatting:** [`.editorconfig`](.editorconfig) sets it: UTF-8, LF line endings, a final newline, no trailing whitespace, 4 spaces for C/C++, 2 for shell scripts, tabs only in the Makefile.
- **Checks before each commit:** [pre-commit](https://pre-commit.com/) runs the same checks on the files you stage. Set it up once per clone with `pip install pre-commit` and `pre-commit install`; `cppcheck` has to be installed separately and on your `PATH`; `pre-commit run --all-files` checks everything.
- **Only change what you need:** don't reformat, re-indent or strip whitespace in lines you aren't changing. The **Style** check on pull requests looks only at the lines you add.
- Everything else follows the Options Menu's [code style](https://github.com/DefKorns/OptionsMenu#code-style).
