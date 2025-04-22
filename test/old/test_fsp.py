import pyarrow
import odin_stream_cpp
import polars as pl
import time

paraa = odin_stream_cpp.FixedSizeParameter(index=10005, size=4)

paraa.add_data(b"1234")
paraa.add_data(b"1234")
paraa.add_data(b"1234")
paraa.add_data(b"1234")
paraa.add_data(b"1234")
paraa.add_data(b"1234")
paraa.add_data(b"1234")
paraa.add_data(b"1234")
paraa.add_data(b"1234")

# Dump
print(paraa)

# with open("test/dataset.hex", "r") as f:
#     data = f.read()
#     # split newline
#     data = data.split("\n")
#     # Decode hex
#     data = [bytes.fromhex(i) for i in data]


# start = time.time()
#     # result = pl.from_arrow(odin_stream.test_create())
# processor = odin_stream.StreamProcessor()
# processor.process_bytes_list(data)

# end = time.time()
# print(f"Execution time: {end - start} ms")
# # print(result)


