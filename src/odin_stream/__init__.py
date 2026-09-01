import os
import pathlib

# Import pyarrow before the extension module, and keep it first in this file.
#
# odin_stream_cpp links the Arrow C++ libraries that ship inside the pyarrow wheel
# rather than vendoring its own copy, so those libraries have to be reachable before
# the extension loads. Importing pyarrow maps them into the process, after which the
# loader resolves our arrow.dll / libarrow.so.<n> against that single copy.
#
# On Windows this import is not sufficient on its own: since Python 3.8 the DLL search
# for extension dependencies ignores PATH and the cwd, and there is no RPATH to fall
# back on (the ELF/Mach-O builds get one pointing at $ORIGIN/pyarrow, see CMakeLists.txt).
# add_dll_directory is the only handle available, so register pyarrow's directory
# explicitly rather than relying on the import having happened to get there first.
import pyarrow

if hasattr(os, "add_dll_directory"):  # Windows only
    os.add_dll_directory(str(pathlib.Path(pyarrow.__file__).resolve().parent))

from .odin_stream import StreamProcessor  # noqa: E402  (must follow the pyarrow setup above)

__all__ = [
    "StreamProcessor",
    "pyarrow",
]
