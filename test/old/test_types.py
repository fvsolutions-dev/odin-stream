import odin_stream_cpp 
from odin_stream_cpp import PrimitiveTypeDescriptor, PrimitiveType, StructDescriptor,TypeDescriptors
import polars as pl

with open("test/dataset.hex", "r") as f:
    data = f.read()
    data = data.split("\n")
    data = [bytes.fromhex(i) for i in data]



vec7_descriptor = StructDescriptor()
vec7_descriptor.add_member("timestamp", PrimitiveTypeDescriptor(odin_stream_cpp.UINT32))
vec7_descriptor.add_member("sequence_number", PrimitiveTypeDescriptor(odin_stream_cpp.UINT16))
vec7_descriptor.add_member("accel_x", PrimitiveTypeDescriptor(odin_stream_cpp.FLOAT32))
vec7_descriptor.add_member("accel_y", PrimitiveTypeDescriptor(odin_stream_cpp.FLOAT32))
vec7_descriptor.add_member("accel_z", PrimitiveTypeDescriptor(odin_stream_cpp.FLOAT32))
vec7_descriptor.add_member("gyro_x", PrimitiveTypeDescriptor(odin_stream_cpp.FLOAT32))
vec7_descriptor.add_member("gyro_y", PrimitiveTypeDescriptor(odin_stream_cpp.FLOAT32))
vec7_descriptor.add_member("gyro_z", PrimitiveTypeDescriptor(odin_stream_cpp.FLOAT32))
vec7_descriptor.add_member("temperature", PrimitiveTypeDescriptor(odin_stream_cpp.FLOAT32))

print(type(vec7_descriptor))
types = TypeDescriptors({0xE0C00000: vec7_descriptor,0xE0B10000: vec7_descriptor, 0xE0B20000: vec7_descriptor, 0xE0B30000: vec7_descriptor, 0xE0B40000: vec7_descriptor})


processor = odin_stream_cpp.StreamProcessor(types)
import time
start = time.time()
processor.process_bytes_list(data)



sert = (processor.get_parameter_set(6684))
# print(sert.flush_to_arrow_table())
# data = pl.from_arrow(sert.flush_to_arrow_table())
# to csv
# data.write_csv("test/dataset.csv")
print(pl.from_arrow(sert.flush_to_arrow_table()))
end = time.time()
print(f"Execution time: {end - start} s")