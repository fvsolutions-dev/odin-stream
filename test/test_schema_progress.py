import struct

from odin_stream import StreamProcessor

# --- Packet type tags (stream_packet_type_t) ---
TYPE_DATA = 0x02
TYPE_IDENTIFIER_EXT = 0x03
TYPE_TYPE_DESCRIPTOR = 0x04

# --- ODIN_element_type_t values (0-indexed) ---
ELEM_UINT16 = 3
ELEM_UINT32 = 4
ELEM_FLOAT32 = 10
ELEM_CUSTOM = 13

IDENTIFIER = 0x1234  # CRC16 stand-in; the decoder trusts the header value.
OTHER_IDENTIFIER = 0x9999
DEFINITION = 0x0A0B0C0D

# A CUSTOM struct: f32@0, u32@4 (8 bytes total).
STEP_TYPE_ID = 0x0077
STEP_FIELDS = [("ct", ELEM_FLOAT32, 4, 0), ("counts", ELEM_UINT32, 4, 4)]
STEP_SIZE = 8


def _packet_header(ptype: int, identifier: int) -> bytes:
    # stream_packet_header_t: type(u8), identifier(u16 LE), reserved(u8)
    return struct.pack("<BHB", ptype, identifier, 0)


def _ext_chunk(items, total_count: int, start_ordinal: int, identifier: int = IDENTIFIER) -> bytes:
    # streaming_identifier_ext_packet_header_t:
    #   header(4) + definition_identifier(u32) + total_count(u16) + start_ordinal(u16)
    out = _packet_header(TYPE_IDENTIFIER_EXT, identifier)
    out += struct.pack("<IHH", DEFINITION, total_count, start_ordinal)
    for index, size, element_type, name, type_id in items:
        name_bytes = name.encode("utf-8")
        # streaming_identifier_ext_item_header_t:
        #   index(u32), size(u16), element_type(u8), name_len(u8), type_id(u16)
        out += struct.pack("<IHBBH", index, size, element_type, len(name_bytes), type_id)
        out += name_bytes
    return out


def _type_descriptor_packet() -> bytes:
    # streaming_type_descriptor_packet_header_t:
    #   header(4) + definition_identifier(u32) + total_count(u16) + start_ordinal(u16)
    out = _packet_header(TYPE_TYPE_DESCRIPTOR, IDENTIFIER)
    out += struct.pack("<IHH", DEFINITION, 1, 0)
    # streaming_type_descriptor_header_t: type_id(u16), size(u16), field_count(u8), name_len(u8)
    tname = b"step_result"
    out += struct.pack("<HHBB", STEP_TYPE_ID, STEP_SIZE, len(STEP_FIELDS), len(tname))
    out += tname
    for fname, elem, size, offset in STEP_FIELDS:
        field_bytes = fname.encode("utf-8")
        # streaming_type_field_header_t: element_type(u8), size(u16), offset(u16), name_len(u8)
        out += struct.pack("<BHHB", elem, size, offset, len(field_bytes))
        out += field_bytes
    return out


def _data_packet(payload: bytes, timestamp: int, sequence: int, identifier: int = IDENTIFIER) -> bytes:
    # streaming_data_packet_header_t: header(4) + timestamp(u32) + sequence_number(u16)
    out = _packet_header(TYPE_DATA, identifier)
    out += struct.pack("<IH", timestamp, sequence)
    out += payload
    return out


# Three primitive parameters, in ordinal order: (index, size, element_type, name, type_id)
PRIMITIVE_ITEMS = [
    (10, 4, ELEM_FLOAT32, "data1.intensity", 0),  # ordinal 0
    (20, 2, ELEM_UINT16, "data2.raw", 0),        # ordinal 1
    (30, 4, ELEM_UINT32, "data3.mv", 0),      # ordinal 2
]


def test_paging_schema_reports_missing_ordinals_then_completes():
    proc = StreamProcessor(odin_db=None)

    # First chunk covers ordinals [0,1] of a 3-parameter schema.
    proc.process_bytes_list([_ext_chunk(PRIMITIVE_ITEMS[0:2], total_count=3, start_ordinal=0)])

    progress = proc.get_schema_progress()[IDENTIFIER]
    assert progress.identifier == IDENTIFIER
    assert progress.definition_identifier == DEFINITION
    assert progress.total_count == 3
    assert progress.learned_count == 2
    assert progress.complete is False
    assert progress.missing_ordinals == [2]
    assert progress.blocked_reason == "awaiting schema chunks"

    # Names and types are reported from the wire-learned schema, in ordinal order.
    assert [(p.ordinal, p.name, p.type) for p in progress.parameters] == [
        (0, "data1.intensity", "f32"),
        (1, "data2.raw", "u16"),
    ]

    # The trailing chunk completes the schema, so DATA becomes decodable.
    proc.process_bytes_list([_ext_chunk(PRIMITIVE_ITEMS[2:3], total_count=3, start_ordinal=2)])

    progress = proc.get_schema_progress()[IDENTIFIER]
    assert progress.complete is True
    assert progress.learned_count == 3
    assert progress.missing_ordinals == []
    assert progress.blocked_reason == ""


def test_data_before_schema_is_counted_as_buffered_then_replayed():
    proc = StreamProcessor(odin_db=None)

    # A partial schema means DATA arriving now gets held rather than dropped.
    proc.process_bytes_list([_ext_chunk(PRIMITIVE_ITEMS[0:2], total_count=3, start_ordinal=0)])
    row = struct.pack("<fHI", 1.5, 7, 4200)
    proc.process_bytes_list([_data_packet(row, timestamp=1000, sequence=1)])

    progress = proc.get_schema_progress()[IDENTIFIER]
    assert progress.buffered_data_packets == 1
    assert progress.unresolved_data_packets == 0
    assert progress.complete is False

    # Completing the schema replays the buffered DATA, emptying the backlog.
    proc.process_bytes_list([_ext_chunk(PRIMITIVE_ITEMS[2:3], total_count=3, start_ordinal=2)])

    progress = proc.get_schema_progress()[IDENTIFIER]
    assert progress.complete is True
    assert progress.buffered_data_packets == 0
    assert proc.statistics.received_data_packets == 1

    frame = proc.flush()[IDENTIFIER]
    assert frame.height == 1
    assert "data1.intensity" in frame.columns


def test_custom_parameter_awaits_its_type_descriptor():
    proc = StreamProcessor(odin_db=None)

    # A complete 0x03 schema, but its single item is a CUSTOM type whose 0x04
    # layout has not arrived — the decoder defers, and so must the report.
    custom_items = [(100, STEP_SIZE, ELEM_CUSTOM, "test.step_result", STEP_TYPE_ID)]
    proc.process_bytes_list([_ext_chunk(custom_items, total_count=1, start_ordinal=0)])

    progress = proc.get_schema_progress()[IDENTIFIER]
    assert progress.complete is False
    assert progress.learned_count == 1
    assert progress.missing_ordinals == []
    assert progress.awaiting_type_ids == [STEP_TYPE_ID]
    assert progress.blocked_reason == "awaiting type descriptor"
    assert progress.parameters[0].type == "custom"

    # The 0x04 unblocks it.
    proc.process_bytes_list([_type_descriptor_packet()])

    progress = proc.get_schema_progress()[IDENTIFIER]
    assert progress.complete is True
    assert progress.awaiting_type_ids == []
    assert progress.blocked_reason == ""


def test_data_for_unknown_identifier_is_counted_per_identifier():
    proc = StreamProcessor(odin_db=None)

    # Nothing has ever been heard about this identifier, so the DATA is dropped.
    proc.process_bytes_list([_data_packet(b"\x00" * 8, timestamp=1, sequence=1, identifier=OTHER_IDENTIFIER)])

    progress = proc.get_schema_progress()[OTHER_IDENTIFIER]
    assert progress.unresolved_data_packets == 1
    assert progress.buffered_data_packets == 0
    assert progress.total_count == 0
    assert progress.complete is False
    assert progress.blocked_reason == "awaiting schema header"

    # The global statistic keeps its existing meaning alongside the per-id record.
    assert proc.statistics.received_unresolved_data_packets == 1


def test_progress_covers_each_identifier_separately():
    proc = StreamProcessor(odin_db=None)

    proc.process_bytes_list([_ext_chunk(PRIMITIVE_ITEMS, total_count=3, start_ordinal=0)])
    proc.process_bytes_list(
        [_ext_chunk(PRIMITIVE_ITEMS[0:1], total_count=2, start_ordinal=0, identifier=OTHER_IDENTIFIER)]
    )

    progress = proc.get_schema_progress()
    assert set(progress) == {IDENTIFIER, OTHER_IDENTIFIER}
    assert progress[IDENTIFIER].complete is True
    assert progress[OTHER_IDENTIFIER].complete is False
    assert progress[OTHER_IDENTIFIER].missing_ordinals == [1]
