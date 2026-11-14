# Rebuilding naneinf.dll on Windows

The Python layer talks to a compiled library: `naneinf.dll` on Windows,
`libnaneinf.so` on Linux. **All engine behaviour lives in that library**, so
copying new `.py` files into an old folder changes nothing about the game —
and if the `.py` expects a newer C API you get:

    AttributeError: function 'nf_reroll_cost' not found

The fix is to rebuild the DLL from the current sources.

## One-time setup

Install a MinGW-w64 g++ (either works):

* **w64devkit** — https://github.com/skeeto/w64devkit/releases — unzip,
  run `w64devkit.exe`, which opens a shell with `g++` on PATH.
* **MSYS2** — https://www.msys2.org — then in the MSYS2 UCRT64 shell:
  `pacman -S mingw-w64-ucrt-x86_64-gcc`

Check it works: `g++ --version`

## Build

From the `w4` directory (the one containing `capi.cpp`):

    g++ -std=c++20 -O2 -shared -static -static-libgcc -static-libstdc++ ^
        capi.cpp balatro.cpp blinds.cpp balatro_rng.cpp hand_eval.cpp clone.cpp ^
        jokers/*.cpp consumables/*.cpp -o python/naneinf.dll

(In a MinGW/MSYS2 shell use `\` for line continuation instead of `^`, or
just put it all on one line.)

The `-static*` flags matter: without them the DLL needs libstdc++ and
libgcc DLLs next to it, and Python fails to load it with a bare
`OSError: [WinError 126]`.

Then:

    cd python
    py gui.py

## Sanity check

    py -c "import naneinf; print(naneinf._MISSING or 'library is current')"

`library is current` means the DLL matches this `naneinf.py`. Anything else
lists the symbols your DLL is missing, i.e. the build didn't happen or went
to the wrong folder.

Also rerun the engine tests after a rebuild. **Use the same `-static`
flags** — without them the .exe needs `libstdc++-6.dll` and
`libgcc_s_seh-1.dll` and dies at load with no output at all (an easy one to
misread as a broken engine; it never reaches `main`):

    g++ -std=c++20 -O2 -static -static-libgcc -static-libstdc++ ^
        tests.cpp balatro.cpp blinds.cpp balatro_rng.cpp clone.cpp ^
        hand_eval.cpp jokers/*.cpp consumables/*.cpp -o run_tests.exe
    run_tests.exe

Expected: `464 passed, 0 failed`.

To check any binary's dependencies: `objdump -p naneinf.dll | findstr "DLL Name"`.
It should list only `KERNEL32.dll` and `msvcrt.dll`.

## The included DLL

`python/naneinf.dll` in this drop was cross-compiled with MinGW-w64 GCC 13
(`x86_64-w64-mingw32-g++`) from these exact sources (INCLUDING clone.cpp),
statically linked, and smoke-tested under wine: it loads via LoadLibrary,
exports all 47 `nf_*` symbols (the 5 clone-support functions included),
plays a full blind, and reproduces the Linux build's state trace on seed
QJSKRTWR to 9 decimal places, with nf_clone/nf_reshuffle_draws exercised.
run_tests.exe built the same way reports 464/464 under wine. You can use
both as-is; the rebuild instructions above are for when you change the
C++.

## Don't mix folders

Keep one working copy. Copying loose files between
`naneinf-v27-fixes\` and `naneinf_gui_and_fixes\` is what produces
version-skew errors: the `.py` comes from one build and the `.dll` from
another. Overwrite the whole `w4` tree, then rebuild the DLL once.
