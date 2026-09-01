"""Run nanobind's stubgen with pyarrow's Arrow C++ libraries reachable.

`odin_stream_cpp` links the Arrow C++ libraries that ship inside the pyarrow wheel.
Those live in `site-packages/pyarrow`, which is on no default library search path,
so importing the extension standalone -- which is exactly what stubgen does -- only
works if something has already pulled them into the process. Importing pyarrow does
that: the loader then resolves our `arrow.dll` / `libarrow.so.<n>` against the copy
pyarrow already mapped, so there is never a second Arrow in the process.

Windows additionally needs the directory registered explicitly: since Python 3.8 the
DLL search for extension dependencies ignores PATH and the cwd, and Windows has no
RPATH equivalent, so `os.add_dll_directory` is the only handle we have.

Usage: gen_stub.py <stubgen args...>
"""

from __future__ import annotations

import os
import pathlib
import runpy
import sys

import pyarrow

if hasattr(os, "add_dll_directory"):  # Windows only
    os.add_dll_directory(str(pathlib.Path(pyarrow.__file__).resolve().parent))

sys.argv = ["stubgen", *sys.argv[1:]]
runpy.run_module("nanobind.stubgen", run_name="__main__")
