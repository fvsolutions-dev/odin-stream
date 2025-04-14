import pyarrow
import odin_stream 
from odin_stream import PrimitiveTypeDescriptor, PrimitiveType, StructDescriptor,TypeDescriptors

with open("test/dataset.hex", "r") as f:
    data = f.read()
    # split newline
    data = data.split("\n")
    # Decode hex
    data = [bytes.fromhex(i) for i in data]



vec7_descriptor = StructDescriptor()

vec7_descriptor.add_member("timestamp", PrimitiveTypeDescriptor(odin_stream.UINT32))
vec7_descriptor.add_member("sequence_number", PrimitiveTypeDescriptor(odin_stream.UINT16))
vec7_descriptor.add_member("accel_x", PrimitiveTypeDescriptor(odin_stream.FLOAT32))
vec7_descriptor.add_member("accel_y", PrimitiveTypeDescriptor(odin_stream.FLOAT32))
vec7_descriptor.add_member("accel_z", PrimitiveTypeDescriptor(odin_stream.FLOAT32))
vec7_descriptor.add_member("gyro_x", PrimitiveTypeDescriptor(odin_stream.FLOAT32))
vec7_descriptor.add_member("gyro_y", PrimitiveTypeDescriptor(odin_stream.FLOAT32))
vec7_descriptor.add_member("gyro_z", PrimitiveTypeDescriptor(odin_stream.FLOAT32))
vec7_descriptor.add_member("temperature", PrimitiveTypeDescriptor(odin_stream.FLOAT32))

print(type(vec7_descriptor))
types = TypeDescriptors({10005: vec7_descriptor,510: vec7_descriptor})


processor = odin_stream.StreamProcessor(types)
processor.process_bytes_list(data)

print(processor)