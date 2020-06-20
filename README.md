# Kobalt Royalties processor

A simple processor for royalties CSVs made in C++11:

* Separates statements by writer(s).
* Inserts and calculates the values for a 'Final Distributed Amount' column and outputs its total.
* Highly configurable.
* Default support for [Kobalt](https://www.kobaltmusic.com/)-style CSV royalties.

## Usage

Run the .exe in the latest release (Windows only) and follow instructions on the command line. Custom configuration is read from `config.cfg` in the working directory.

Alternatively, you can compile the program yourself (the single file src/royalties-processor.cpp). If not on Windows, remove `#include <windows.h>` and all references to `QueryPerformanceCounter` and `LARGE_INTEGER`. These are only used to print elapsed time, otherwise the code is highly portable.

## Configuration format

```ini
VAR_NAME=VALUE
VAR_2=VALUE_2
```

Comments using `#` are allowed, spaces are not. Running the program once with custom configuration enabled will generate a default config file.

## Contribution

Please note bugs or feature suggestions in the [issue tracker](https://github.com/louis-vs/royalties-processor/issues). Alternatively, fork the repo and implement the change/fix yourself. All suggestions and contributions are welcome.

## Acknowledgements

Commissioned by Van Steene Music Publishing Ltd.
