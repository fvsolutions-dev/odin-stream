# ODIN-Stream
A data streaming utility designed to be added on to odin

# Installation

## Prebuilt Wheels

Prebuilt wheels are available for Windows, Linux and MacOS, they can be installed using pip:
```sh
pip install odin-stream
```

## From Source
Note: You need a valid C++ compiler and Python 3.7+ installed on your system.

Basic installation
```sh
uv pip install --reinstall -e .
```

Fast build
```sh
uv pip install --reinstall --no-build-isolation -ve .
```

Auto rebuild on run
```sh
uv pip install --reinstall --no-build-isolation -Ceditable.rebuild=true -ve .
``` 


### Python Stub files generation

They are generated automatically buy can also be generated 

```
python -m nanobind.stubgen -m nanobind_example_ext
```

### Test

```sh
pytest test
```


