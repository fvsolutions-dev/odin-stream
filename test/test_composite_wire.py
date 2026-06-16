"""Wire-only decode of CUSTOM structs, arrays-of-structs, and primitive arrays.

Builds (with `struct`, byte-for-byte against embedded_odin_stream/stream_packet.h):
  * a 0x04 TYPE_DESCRIPTOR for `test_step_result` (4 primitive fields),
  * a 0x03 IDENTIFIER_EXT referencing it for a CUSTOM scalar and a CUSTOM array,
    plus a primitive `f32[3]` array,
  * a 0x02 DATA packet.

Feeds them to a StreamProcessor with NO OdinDB and asserts the Arrow/polars
table carries the expected per-field / per-element columns with correct types
and values — proving composite + array decode straight from the wire.
"""

import struct

import polars as pl

from odin_stream import StreamProcessor

# --- Packet type tags (stream_packet_type_t) ---
TYPE_DATA = 0x02
TYPE_IDENTIFIER_EXT = 0x03
TYPE_TYPE_DESCRIPTOR = 0x04

# --- ODIN_element_type_t values (0-indexed) ---
ELEM_UINT32 = 4
ELEM_FLOAT32 = 10
ELEM_CUSTOM = 13

IDENTIFIER = 0xBEEF  # CRC16 stand-in; the decoder trusts the header value.
DEFINITION = 0x01020304

# test_step_result struct: f32@0, u32@4, f32@8, u32@12  (16 bytes total)
STEP_TYPE_ID = 0x0042
STEP_FIELDS = [
    ("ambient_lux", ELEM_FLOAT32, 4, 0),
    ("ambient_counts", ELEM_UINT32, 4, 4),
    ("uv_index", ELEM_FLOAT32, 4, 8),
    ("uv_counts", ELEM_UINT32, 4, 12),
]
STEP_SIZE = 16

# Parameter indices.
IDX_SCALAR = 100  # custom scalar (1 struct)
IDX_ARRAY = 200   # custom array (2 structs)
IDX_PRIM = 300    # primitive f32[3]


def _packet_header(ptype: int, identifier: int) -> bytes:
    # stream_packet_header_t: type(u8), identifier(u16 LE), reserved(u8)
    return struct.pack("<BHB", ptype, identifier, 0)


def _type_descriptor_packet() -> bytes:
    # streaming_type_descriptor_packet_header_t:
    #   header(4) + definition_identifier(u32) + total_count(u16) + start_ordinal(u16)
    out = _packet_header(TYPE_TYPE_DESCRIPTOR, IDENTIFIER)
    out += struct.pack("<IHH", DEFINITION, 1, 0)
    # streaming_type_descriptor_header_t: type_id(u16), size(u16), field_count(u8), name_len(u8)
    tname = b"test_step_result"
    out += struct.pack("<HHBB", STEP_TYPE_ID, STEP_SIZE, len(STEP_FIELDS), len(tname))
    out += tname
    for fname, elem, size, offset in STEP_FIELDS:
        fb = fname.encode("utf-8")
        # streaming_type_field_header_t: element_type(u8), size(u16), offset(u16), name_len(u8)
        out += struct.pack("<BHHB", elem, size, offset, len(fb))
        out += fb
    return out


def _ext_chunk(items, total_count: int, start_ordinal: int) -> bytes:
    # streaming_identifier_ext_packet_header_t:
    #   header(4) + definition_identifier(u32) + total_count(u16) + start_ordinal(u16)
    out = _packet_header(TYPE_IDENTIFIER_EXT, IDENTIFIER)
    out += struct.pack("<IHH", DEFINITION, total_count, start_ordinal)
    for index, size, element_type, name, type_id in items:
        nb = name.encode("utf-8")
        # streaming_identifier_ext_item_header_t:
        #   index(u32), size(u16), element_type(u8), name_len(u8), type_id(u16)
        out += struct.pack("<IHBBH", index, size, element_type, len(nb), type_id)
        out += nb
    return out


def _data_packet(payload: bytes, timestamp: int, sequence: int) -> bytes:
    # streaming_data_packet_header_t: header(4) + timestamp(u32) + sequence_number(u16)
    out = _packet_header(TYPE_DATA, IDENTIFIER)
    out += struct.pack("<IH", timestamp, sequence)
    out += payload
    return out


def _step(lux, counts, uvi, uvc) -> bytes:
    return struct.pack("<fIfI", lux, counts, uvi, uvc)


def test_composite_and_array_wire_only_decode():
    # IDENTIFIER_EXT items, in ordinal order:
    #   ordinal 0: custom scalar  (size = 1 * 16)
    #   ordinal 1: custom array   (size = 2 * 16)
    #   ordinal 2: primitive f32[3] (size = 3 * 4)
    items = [
        (IDX_SCALAR, STEP_SIZE, ELEM_CUSTOM, "step", STEP_TYPE_ID),
        (IDX_ARRAY, 2 * STEP_SIZE, ELEM_CUSTOM, "steps", STEP_TYPE_ID),
        (IDX_PRIM, 3 * 4, ELEM_FLOAT32, "arr", 0),
    ]

    proc = StreamProcessor(odin_db=None)

    type_desc = _type_descriptor_packet()
    ext = _ext_chunk(items, total_count=len(items), start_ordinal=0)

    # One DATA row: scalar step, then array[0], array[1], then f32[3].
    row = b""
    row += _step(1.5, 10, 2.5, 20)            # step
    row += _step(3.5, 30, 4.5, 40)            # steps[0]
    row += _step(5.5, 50, 6.5, 60)            # steps[1]
    row += struct.pack("<fff", 7.0, 8.0, 9.0)  # arr[0..2]
    data = _data_packet(row, timestamp=1234, sequence=0)

    # Feed type descriptor + schema first, then data.
    proc.process_bytes_list([type_desc, ext, data])

    stats = proc.statistics
    assert stats.received_type_descriptor_packets == 1
    assert stats.received_identifier_ext_packets == 1
    assert stats.received_data_packets == 1
    assert stats.received_unresolved_data_packets == 0

    frames = proc.flush()
    assert IDENTIFIER in frames
    df = frames[IDENTIFIER]

    cols = set(df.columns)

    # --- Custom scalar -> name.field columns ---
    for f in ("ambient_lux", "ambient_counts", "uv_index", "uv_counts"):
        assert f"step.{f}" in cols
    assert df["step.ambient_lux"][0] == 1.5
    assert df["step.ambient_counts"][0] == 10
    assert df["step.uv_index"][0] == 2.5
    assert df["step.uv_counts"][0] == 20

    # --- Custom array -> name[i].field columns ---
    assert "steps[0].ambient_lux" in cols
    assert "steps[1].uv_index" in cols
    assert df["steps[0].ambient_lux"][0] == 3.5
    assert df["steps[0].ambient_counts"][0] == 30
    assert df["steps[1].ambient_lux"][0] == 5.5
    assert df["steps[1].uv_index"][0] == 6.5
    assert df["steps[1].uv_counts"][0] == 60

    # --- Primitive array -> name[i] columns ---
    assert {"arr[0]", "arr[1]", "arr[2]"}.issubset(cols)
    assert df["arr[0]"][0] == 7.0
    assert df["arr[1]"][0] == 8.0
    assert df["arr[2]"][0] == 9.0

    # --- Types ---
    assert df.schema["step.ambient_lux"] == pl.Float32
    assert df.schema["step.ambient_counts"] == pl.UInt32
    assert df.schema["steps[1].uv_index"] == pl.Float32
    assert df.schema["arr[2]"] == pl.Float32


def test_custom_type_descriptor_arriving_after_data_is_deferred():
    """A CUSTOM parameter whose 0x04 TYPE_DESCRIPTOR arrives AFTER the schema and
    DATA must be deferred (not frozen as an unknown/null column), then built with
    real per-field columns once the 0x04 lands."""
    items = [
        (IDX_SCALAR, STEP_SIZE, ELEM_CUSTOM, "step", STEP_TYPE_ID),
    ]
    proc = StreamProcessor(odin_db=None)

    ext = _ext_chunk(items, total_count=len(items), start_ordinal=0)
    row = _step(1.0, 2, 3.0, 4)
    data = _data_packet(row, timestamp=5, sequence=0)
    type_desc = _type_descriptor_packet()

    # Schema + DATA before the type descriptor: must defer, not lose the row.
    proc.process_bytes_list([ext, data, type_desc])

    stats = proc.statistics
    assert stats.received_unresolved_data_packets == 0
    assert stats.received_data_packets == 1

    df = proc.flush()[IDENTIFIER]
    assert df["step.ambient_lux"][0] == 1.0
    assert df["step.uv_counts"][0] == 4
