<p align="center"><img src="https://telodendria.org/user/themes/bancino/images/logo/Cytoplasm.png"></p>
<h1 align="center">Cytoplasm (<code>libcytoplasm</code>)</h1>

Cytoplasm is a general-purpose C library for creating high-level (particularly networked and multi-threaded) C applications. It allows applications to take advantage of the speed, flexibility, and simplicity of the C programming language, while providing helpful code to allow applications to perform various complex tasks with minimal effort. Cytoplasm provides high-level data structures, a basic logging facility, an HTTP client and server, and more. It also reports memory leaks, which can aid in debugging, particularly on systems that don't have advanced tools like `valgrind`.

Cytoplasm aims not to only do one thing well, but to do many things good enough. This is in contrast to other libraries, which only do one thing and thus require the developer to pull in many different libraries for a broad range of functionality. The primary target of Cytoplasm is simple yet higher level C applications that have to perform relatively complex tasks, but don't want to depend on a large number of dependencies.

Cytoplasm is extremely opinionated on the way programs using it are written. It strives to create a comprehensive and tightly-integrated programming environment, while also maintaining C programming correctness. It doesn't do any macro magic or make C look like anything other than C. It is written entirely in C99, and depends only on a POSIX environment. This differentiates it from other general-purpose libraries that often require more modern compilers and non-standard language and environment features. Cytoplasm is intended to be extremely portable and simple, while still providing some of the functionality expected in higher-level programming languages in a platform-agnostic manner. In the case of TLS, Cytoplasm wraps low-level TLS libraries to offer a single, unified interface to TLS so that programs do not have to care about the underlying implementation.

Cytoplasm is probably not suitable for embedded programming. It makes liberal use of the heap, and while data structures are designed to conserve memory where possible and practical, minimal memory usage is not really a design goal for Cytoplasm, although Cytoplasm takes care not to use any more memory than it absolutely needs. Cytoplasm also wraps a few standard libraries with additional logic and checking. While this ensures better runtime safety, this inevitably adds a little overhead, which may be unsuitable for time- or space-critical tasks.

Originally a part of Telodendria ([Website](https://telodendria.org), [Repo](/Telodendria/Telodendria)), a Matrix homeserver written in C, Cytoplasm was split off into its own project due to the desire of some Telodendria developers to use Telodendria's code in other projects. Cytoplasm is still an official Telodendria project, but it is designed specifically to be distributed and used totally independent of Telodendria.

The name "Cytoplasm" was chosen for a few reasons. It plays off the precedent set up by the Matrix organization in naming projects after the parts of a neuron. It also speaks to the function of Cytoplasm.  The cytoplasm of a cell is the supporting material. It is what gives the cell its shape, and it facilitates the movement of materials to the other cell parts. Likewise, Cytoplasm aims to provide a support mechanism for C applications that have to perform complex tasks beyond what the C standard library provides.

Cytoplasm also starts with a C, which I think is a nice touch for C libraries. It's also fun to say and unique enough that searching for "libcytoplasm" should bring you to this project and not some other one.

## Requirements

Cytoplasm aims to have zero software dependencies beyond what is mandated by POSIX. You only need a standard C99 compiler, and the standard `math` and `pthread` libraries to build Cytoplasm. TLS support can optionally be enabled with the configuration script. The supported TLS implementations are as follows:

- OpenSSL
- LibreSSL

If TLS support is not enabled, all APIs that use it should fall back to non-TLS behavior in a sensible manner. For example, if TLS support is not enabled, then the HTTP client API will simply return an error if a TLS connection is requested.

## Building

If your operating system or software distribution provides a pre-built package of Cytoplasm, you should prefer to use that instead of building it from source.

Cytoplasm uses the standard C library build procedure. Just run these commands:

```
./configure
make
```

This will produce the following out/ directory:

```
    out/
        lib/
            libcytoplasm.so - The Cytoplasm shared library.
            libcytoplasm.a - The Cytoplasm static archive.
        bin/ - A few useful tools build with Cytoplasm.
        man/ - All Cytoplasm API documentation.
```

You can also run `make install` as `root` to install Cytoplasm to the system. This will install the libraries, tools, and `man` pages.

The `configure` script has a number of optional flags, which are as follows:

- `--with-(openssl|libressl)`: Select the TLS implementation to use. OpenSSL is selected by default.
- `--disable-tls`: Disable TLS altogether.
- `--prefix=<path>`: Set the install prefix to set by default in the `Makefile`. This defaults to `/usr/local`, which should be appropriate for most Unix-like systems.
- `--(enable|disable)-debug`: Control whether or not to enable debug mode. This sets the optimization level to 0 and builds with debug symbols. Useful for running with a debugger.

Cytoplasm can be customized with the following options:

- `--lib-name=<name>`: The output name of the library. This defaults to `Cytoplasm` and should in most cases not be changed.

The following recipes are available in the generated `Makefile`:

- `all`: This is the default target. It builds everything.
- `Cytoplasm`: Build the `libCytoplasm.(so|a)` binaries. If you specified an alternative `--lib-name`, then this target will be named after that.
- `docs`: Generate the header documentation as `man` pages.
- `tools`: Build the supplemental tools which may be useful for development.
- `clean`: Remove the build and output directories. Cytoplasm builds are out-of-tree, which greatly simplifies this recipe compared to in-tree builds.

If you're developing Cytoplasm, these recipes may also be helpful:

- `format`: Format the source code using `indent`. This may require a BSD `indent` because last time I tried GNU `indent`, it didn't like the flags in `indent.pro`. Your mileage may vary.
- `license`: Update the license headers in all source code files with the contents of the `LICENSE.txt`.

To install Telodendria to your system, the following recipes are available:

- `install`: This installs Cytoplasm under the prefix set with `./configure --prefix=<dir>` or with `make PREFIX=<dir>`. By default, the `make` `PREFIX` is set to whatever was set with `configure --prefix`.
- `uninstall`: Uninstall Cytoplasm from the same prefix as specified above.

After a build, you can find the object files in `build/` and the output binaries in `out/lib/`.

## Usage

Cytoplasm provides the typical .so and .a files, which can be used to link programs with it in the usual way. Somewhat *unusually* for C libraries, however, it provides its own `main()` function, so programs written with Cytoplasm provide `Main()` instead, which is called by Cytoplasm. Cytoplasm works this way because it needs to perform some setup logic before user code runs and some teardown logic after user code returns.

Here is the canonical Hello World written with Cytoplasm:

```c
#include <Cytoplasm/Log.h>

int Main(void)
{
    Log(LOG_INFO, "Hello World!");
    return 0;
}
```

If this file is `Hello.c`, then you can compile it by doing this:

	$ cc -o hello Hello.c -lCytoplasm

The full form of `Main()` expected by the stub is as follows:

```c
    int Main(Array *args, HashMap *env);
```

The first argument is a Cytoplasm array of the command line arguments, and the second is a Cytoplasm hash map of environment variables. Most linkers will let programs omit the `env` argument, or both arguments if you don't need either. The return value of `Main()` is returned to the operating system, as would be expected.

Note that both arguments to Main may be treated like any other Cytoplasm array or hash map. However, do not invoke `ArrayFree()` or `HashMapFree()` on the passed pointers, because memory is cleaned up after `Main()` returns.

## License

All of the code and documentation for Cytoplasm is licensed under the same license as Telodendria itself. Please refer to [Telodendria &rightarrow; License](/Telodendria/Telodendria#license) for details.

The Cytoplasm logo was designed by [Tobskep](https://tobskep.com) and is licensed under the [Creative Commons Attribution-ShareAlike 4.0](https://creativecommons.org/licenses/by-sa/4.0/) license.
