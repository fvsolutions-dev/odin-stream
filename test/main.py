from odin_stream import StreamProcessor
import odin_db

ODIN_PATH = "test/OD.odin"
with open(ODIN_PATH, "rb") as f:
    odin_db = odin_db.OdinDBModel.model_validate_json(f.read())

processor = StreamProcessor(odin_db)


# with open("test/dataset.hex", "r") as f:
#     data = f.read()
#     data = data.split("\n")
#     data = [bytes.fromhex(i) for i in data]


# processor.process_bytes_list(data)

