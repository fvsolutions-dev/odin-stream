import pyarrow
import odin_stream

# Create test array
data = [1.0, 2.0, 3.0, 4.0, 5.0]
# Create a pyarrow array
array = pyarrow.array(data, type=pyarrow.float64())
result = odin_stream.create_table_with_mixed_data()

# Print the result
print(result)

# parameters = [
#     FixedSizeParameter(index=10005, data=b'myta'),
#     FixedSizeParameter(index=20005, data=b'myata'),
#     FixedSizeParameter(index=30005, data=b'my_data'),
#     FixedSizeParameter(index=40005, data=b'my_dta'),
#     FixedSizeParameter(index=50005, data=b'my_data'),
# ]

# set = ParameterSet(10)
# set.add_list(parameters)

# identifier = set.generate_identifier_packet()
# data = set.generate_data_packet(10)

# # Reconstruct the set from the identifier packet
# idw = ParameterSet.parse_identifier_packet(identifier)
# # Get list of ids
# print(f"Identifier: {idw.indices}")
# # Get list of data packets
# print(f"Identifier: {idw.parse_data_packet(data)}")

