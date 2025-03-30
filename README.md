# ODIN-Stream
A data streaming utility designed as an extension to the odin object dictionary.

## How it works

This library is designed to stream out data. It separates the data into two packets, an identifier packet and a data packet. The identifier packet contains the metadata of the data packets, and the data packets contain the actual data. These packets can be send at different rates, and the identifier packet can be sent less frequently than the data packets to save bandwidth.


## Packet definition
![alt text](docs/design.svg)

**Common header**
The header contains the following content:
* `type`: The type of the identifier packet, must be 1
* `id`: The id of packet content, this is must be unique for each packet definition, and is used to identify the packet content in the data stream.


### Identifier packet `type=0x01`
This packet contains the required metadata to process the data packets. They are sent less frequently than the data packets to save bandwidth.

In addition to the common header, the header contains the following content:
* `od_identifier`: The identifier of the odin packet, this marks which Od definition is used.

There remaining data in the packet contains a fixed list of identifiers, each of which is a tuple of the following:
* `id`: Ths id of the entry as referenced in odin
* `size`: The size of the entry in bytes as it will be encoded in the data stream


### Data packet `type=0x02`
This packet contains the data which is streamed out.

In addition to the common header, the header contains the following content:
* `timestamp`: The timestamp of the packet, this is used to identify the time when the packet was sent.
* `sequence`: The sequence number of the packet, this is used to identify the order of the packets.

There remaining data in the packet contains the data in a specific order, which is defined in the identifier packet.

### Event packet `type=0x0B`
This packet contains custom event data which can be streamed out. Details of the events need to br predefined on both sides of the stream.


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


