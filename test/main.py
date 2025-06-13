from odin_stream import StreamProcessor
import odin_db
import time
ODIN_PATH = "test/OD.odin"
with open(ODIN_PATH, "rb") as f:
    odin_db_data = odin_db.OdinDBModel.model_validate_json(f.read())

processor = StreamProcessor(odin_db_data,silent_errors=True)

with open("test/dataset.hex", "r") as f:
    data = f.read()
    data = data.split("\n")
    data = [bytes.fromhex(i) for i in data]
    

start = time.time()
processor.process_bytes_list(data)


end = time.time()
result = processor.flush()
print(f"Execution time: {end - start} s")

print(processor.statistics)

