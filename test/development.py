from odin_stream import FixedSizeParameter,ParameterSet


parameters = [
    FixedSizeParameter(index=10005, data=b'my_data'),
    FixedSizeParameter(index=20005, data=b'my_data'),
    FixedSizeParameter(index=30005, data=b'my_data'),
    FixedSizeParameter(index=40005, data=b'my_data'),
    FixedSizeParameter(index=50005, data=b'my_data'),
]

set = ParameterSet(10)
set.add_list(parameters)

identifier = set.generate_identifier_packet()
data = set.generate_data_packet(10)

print(f"Count: {set.count}")
print(f"Count: {set.hash}")