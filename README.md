# How to use Stencils in Mozilla SpiderMonkey

If you repeatedly execute Javascript code, you can optimise performance by compiling scripts once and then reusing compilation results.
In SpiderMonkey, a Stencil is an object that represents a compiled script. You may create and execute a Stencil object on any thread. That allows the creation of a shared cache of compiled scripts.

The example code here shows how to do that. We create two essentially identical threads. Each thread picks up a Javascript code from a set of scripts, compiles, caches and executes it. However, if the script is already compiled and cached, the thread takes it from a cache and executes it.

This example is written for SpiderMonkey ESR release 115: 115.8.0esr. To run it, you will need to modify the SpiderMonkey source code. Please also note that version 115 doesn't have a public API for error reporting.

For SpiderMonkey source code modifications see [Bug 1881682](https://bugzilla.mozilla.org/show_bug.cgi?id=1881682).

To compile and run the example, modify the `makefile`.

Specify a path to the SpiderMonkey distribution:

```
SMONKEY=../firefox-115.8.0/obj-debug-x86_64-pc-linux-gnu/dist
```

and the name of the SpiderMonkey library (omitting the lib prefix):


```
SMONKEYLIB=mozjs-115
```

Build:

```
$make
```

Build and run:

```
$make test
```
