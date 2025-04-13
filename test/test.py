import pyarrow
import odin_stream
import polars as pl
import time

with open("test/dataset.hex", "r") as f:
    data = f.read()
    # split newline
    data = data.split("\n")
    # Decode hex
    data = [bytes.fromhex(i) for i in data]


start = time.time()
    # result = pl.from_arrow(odin_stream.test_create())
processor = odin_stream.StreamProcessor()
processor.process_bytes_list(data)

print(pl.from_arrow(processor.get_parameter_set(25762).flush_to_arrow_table()))
print(pl.from_arrow(processor.get_parameter_set(6684).flush_to_arrow_table()))
end = time.time()
print(f"Execution time: {end - start} ms")
# print(result)



