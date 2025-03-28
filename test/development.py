from odin_stream import FixedSizeParameter,ParameterSet


parameters = [
    FixedSizeParameter(index=10005, data=b'myta'),
    FixedSizeParameter(index=20005, data=b'myata'),
    FixedSizeParameter(index=30005, data=b'my_data'),
    FixedSizeParameter(index=40005, data=b'my_dta'),
    FixedSizeParameter(index=50005, data=b'my_data'),
]

set = ParameterSet(10)
set.add_list(parameters)

identifier = set.generate_identifier_packet()
data = set.generate_data_packet(10)

# Reconstruct the set from the identifier packet
idw = ParameterSet.parse_identifier_packet(identifier)
# Get list of ids
print(f"Identifier: {idw.indices}")
# Get list of data packets
print(f"Identifier: {idw.parse_data_packet(data)}")

