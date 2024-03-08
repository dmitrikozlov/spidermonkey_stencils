# How to use Stencils in Mozilla SpiderMonkey

If you repeatedly execute the same Javascript code, you can optimise performance by compiling scripts once and then reusing compilation results.
In SpiderMonkey, a Stencil is an object that represents a compiled script. You may create and execute a Stencil object on any thread. That allows the creation of a shared cache of compiled scripts.

The example code here shows how to do that. We create two essentially identical threads. Each thread picks up a Javascript code from a set of scripts, compiles, caches and executes it. However, if the script is already compiled and cached, the thread takes it from a cache and executes it.

The main branch of this repo shows an example written for a future release of SpiderMonkey, perhaps ESR 128. The branch esr115 is for ESR 115: 115.8.0esr

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
