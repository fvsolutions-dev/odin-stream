from math import e
from odin_stream import StreamProcessor
import odin_db
import time
ODIN_PATH = "test/OD.odin"
with open(ODIN_PATH, "rb") as f:
    odin_db = odin_db.OdinDBModel.model_validate_json(f.read())

processor = StreamProcessor(odin_db)


with open("test/dataset.hex", "r") as f:
    data = f.read()
    data = data.split("\n")
    data = [bytes.fromhex(i) for i in data]

print(f"Loaded {len(data)} packets")
start = time.time()
result = processor.process_bytes_list(data)
end = time.time()
print(result)


print(f"Execution time: {end - start} s")
