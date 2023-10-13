sclumpy
=======

The **sclumpy** utility aims to create texture WAD files for GoldSrc games. It
can parse Lumpy scripts the same way as the original Qlumpy utility.
Additionally, it can create GoldSrc transparent sprays with a convenience
option.

The program relies on C++17 and should be portable across most POSIX systems.
It succeeded in making sprays for Sven Co-op.

Building
--------

First, check the `Makefile`. Adapt its content if you’re not using GCC or if
you want different optimization settings.

Then, run `make`. That’s it!

**Reminder:** On some `make` implementations, the `-j` option parallelizes the
process. With C++ compile times, this makes a big difference.

Running
-------

The file `sclumpy.tr` contains a [troff](https://troff.org/) manual page
detailing how to use the utility. On GNU/Linux, you may read it with the `man
-l sclumpy.tr` command.

If all you want to use it for is making sprays, this is all you need:
```bash
sclumpy -s {image.bmp
```

Things left to do
-----------------

* Make sprays transparent even if the input file doesn’t start with `{`.

* Implement LBM file loading.

* Implement lump types other than `mipmap`.

* Better document how script parsing differs from the original Qlumpy.
