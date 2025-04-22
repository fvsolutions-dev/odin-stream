import pyarrow
import odin_stream_cpp
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
processor = odin_stream_cpp.StreamProcessor()
processor.process_bytes_list(data)

data1 = pl.from_arrow(processor.get_parameter_set(25762).flush_to_arrow_table())
data2 = pl.from_arrow(processor.get_parameter_set(6684).flush_to_arrow_table())

# Save to compressed parquet
data1.write_parquet("test/dataset1.parquet", compression="zstd",compression_level=22)
data2.write_parquet("test/dataset2.parquet", compression="zstd",compression_level=22)
end = time.time()
print(f"Execution time: {end - start} ms")
# print(result)

print(data1)


