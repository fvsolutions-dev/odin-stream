# BURST link protocol 
Binary Utility for Reliable Stream Transfer (BURST) is a cross-platform library that provides a robust link layer protocol for transmitting binary data over streams (e.g. UART). It ensures data integrity by applying a 16-bit checksum and utilizes COBS (Consistent Overhead Byte Stuffing) encoding to format packets efficiently.


## Features
* Multi-language Support: Seamless integration with Python, C, and C++ projects.
* Reliable Transmission: 16-bit checksum ensures error detection.
* Efficient Encoding: Uses COBS encoding to convert data into a stream-friendly format.

## Design
The following diagram illustrates the data flow within the BURST protocol:

![design](docs/design.svg "design")

# Installation

## Prebuilt Wheels

Prebuilt wheels are available for Windows, Linux and MacOS, they can be installed using pip:
```sh
pip install burst-link-protocol
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
pip install --no-build-isolation -Ceditable.rebuild=true -ve .
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


