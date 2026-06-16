"""Wire-only decode via self-describing 0x03 IDENTIFIER_EXT packets.

Constructs a StreamProcessor with NO OdinDB and feeds synthetic chunked 0x03
packets plus a DATA packet, all hand-built with `struct` to match the wire
layout in embedded_odin_stream/stream_packet.h. Asserts the resulting Arrow
table carries the correct DOTTED column names and correctly-typed values.
"""

import struct

from odin_stream import StreamProcessor

# --- Packet type tags (stream_packet_type_t) ---
TYPE_IDENTIFIER = 0x01
TYPE_DATA = 0x02
TYPE_IDENTIFIER_EXT = 0x03

# --- ODIN_element_type_t values (0-indexed) ---
ELEM_FLOAT32 = 10
ELEM_UINT16 = 3
ELEM_INT32 = 8

IDENTIFIER = 0xABCD  # CRC16 stand-in; the decoder trusts the header value.
DEFINITION = 0x11223344


def _packet_header(ptype: int, identifier: int) -> bytes:
    # stream_packet_header_t: type(u8), identifier(u16 LE), reserved(u8)
    return struct.pack("<BHB", ptype, identifier, 0)


def _ext_chunk(items, total_count: int, start_ordinal: int) -> bytes:
    # streaming_identifier_ext_packet_header_t:
    #   header(4) + definition_identifier(u32) + total_count(u16) + start_ordinal(u16)
    out = _packet_header(TYPE_IDENTIFIER_EXT, IDENTIFIER)
    out += struct.pack("<IHH", DEFINITION, total_count, start_ordinal)
    for index, size, element_type, name in items:
        name_bytes = name.encode("utf-8")
        # streaming_identifier_ext_item_header_t:
        #   index(u32), size(u16), element_type(u8), name_len(u8), type_id(u16)
        # type_id is 0 for primitives (these items are all primitive).
        out += struct.pack("<IHBBH", index, size, element_type, len(name_bytes), 0)
        out += name_bytes
    return out


def _identifier_packet(items) -> bytes:
    # streaming_identifier_packet_header_t: header(4) + definition_identifier(u32)
    out = _packet_header(TYPE_IDENTIFIER, IDENTIFIER)
    out += struct.pack("<I", DEFINITION)
    for index, size, _element_type, _name in items:
        # streaming_identifier_item_t: index(u32), size(u16)
        out += struct.pack("<IH", index, size)
    return out


def _data_packet(payload: bytes, timestamp: int, sequence: int) -> bytes:
    # streaming_data_packet_header_t: header(4) + timestamp(u32) + sequence_number(u16)
    out = _packet_header(TYPE_DATA, IDENTIFIER)
    out += struct.pack("<IH", timestamp, sequence)
    out += payload
    return out


def test_identifier_ext_wire_only_decode():
    # Schema: three dotted-name parameters in a fixed ordinal order.
    # (index, wire size in bytes, element_type, dotted name)
    items = [
        (10, 4, ELEM_FLOAT32, "sensor.uv.intensity"),   # ordinal 0
        (20, 2, ELEM_UINT16, "sensor.vis.raw"),          # ordinal 1
        (30, 4, ELEM_INT32, "power.battery.mv"),         # ordinal 2
    ]

    # NO OdinDB: pure wire-learned decode.
    proc = StreamProcessor(odin_db=None)

    # Chunk the 0x03 schema across two packets (ordinals [0,1] then [2]).
    chunk0 = _ext_chunk(items[0:2], total_count=3, start_ordinal=0)
    chunk1 = _ext_chunk(items[2:3], total_count=3, start_ordinal=2)

    # One DATA row matching the ordinal order: f32, u16, i32.
    row = struct.pack("<fHi", 1.5, 42, -7)
    data = _data_packet(row, timestamp=1000, sequence=0)

    # Feed schema chunks first, then data.
    proc.process_bytes_list([chunk0, chunk1, data])

    stats = proc.statistics
    assert stats.received_identifier_ext_packets == 2
    assert stats.received_data_packets == 1
    assert stats.received_unresolved_data_packets == 0

    # Learned schema should expose all three dotted names.
    learned = proc.get_learned_descriptors()
    assert learned.find_by_id(10).get_name() == "sensor.uv.intensity"
    assert learned.find_by_id(20).get_name() == "sensor.vis.raw"
    assert learned.find_by_id(30).get_name() == "power.battery.mv"

    # The decoded table must have dotted column names and correctly-typed values.
    frames = proc.flush()
    assert IDENTIFIER in frames
    df = frames[IDENTIFIER]

    assert "sensor.uv.intensity" in df.columns
    assert "sensor.vis.raw" in df.columns
    assert "power.battery.mv" in df.columns

    assert df["sensor.uv.intensity"][0] == 1.5
    assert df["sensor.vis.raw"][0] == 42
    assert df["power.battery.mv"][0] == -7

    # Types: f32 -> Float32, u16 -> UInt16, i32 -> Int32.
    import polars as pl

    assert df.schema["sensor.uv.intensity"] == pl.Float32
    assert df.schema["sensor.vis.raw"] == pl.UInt16
    assert df.schema["power.battery.mv"] == pl.Int32


def test_plain_identifier_before_ext_is_deferred_then_resolved():
    """A plain 0x01 IDENTIFIER (and its DATA) that arrive before the 0x03
    chunks must be DEFERRED — not frozen with by-index columns — then
    (re)built with dotted names once the self-describing schema completes."""
    items = [
        (10, 4, ELEM_FLOAT32, "a.b.c"),
        (20, 2, ELEM_UINT16, "d.e"),
    ]

    proc = StreamProcessor(odin_db=None)

    ident = _identifier_packet(items)
    row = struct.pack("<fH", 2.5, 9)
    data = _data_packet(row, timestamp=1, sequence=0)
    chunk = _ext_chunk(items, total_count=2, start_ordinal=0)

    # Plain IDENTIFIER first (schema not yet known -> deferred), then DATA
    # (buffered), then the completing 0x03 schema chunk.
    proc.process_bytes_list([ident, data, chunk])

    stats = proc.statistics
    # The early DATA must not be counted as permanently unresolved.
    assert stats.received_unresolved_data_packets == 0
    assert stats.received_data_packets == 1

    frames = proc.flush()
    df = frames[IDENTIFIER]
    # Dotted names, not by-index columns — proving the deferral worked.
    assert df["a.b.c"][0] == 2.5
    assert df["d.e"][0] == 9


def test_live_ordering_multiple_flushes_accumulates_rows():
    """The LIVE hermes/BLE path, which the single-batch tests above don't cover:
    DATA arrives across MANY process_bytes_list() calls with flush() between them
    (the WorkSchedulerNode's per-second cadence), while the 0x03 schema pages in
    chunk-by-chunk over several of those calls. Regression for the case where the
    early DATA is buffered/deferred until the wire schema completes, then replayed
    and drained across subsequent flushes."""
    items = [
        (10, 4, ELEM_FLOAT32, "grp.a"),
        (20, 2, ELEM_UINT16, "grp.b"),
        (30, 4, ELEM_INT32, "grp.c"),
        (40, 4, ELEM_FLOAT32, "grp.d"),
    ]
    total = len(items)
    proc = StreamProcessor(odin_db=None)

    # Page the schema two items per chunk → two chunks for a full cycle.
    chunks = [
        _ext_chunk(items[0:2], total_count=total, start_ordinal=0),
        _ext_chunk(items[2:4], total_count=total, start_ordinal=2),
    ]

    def row(seq: int) -> bytes:
        # matches sizes 4,2,4,4 → f32, u16, i32, f32
        return struct.pack("<fHif", 1.0 + seq, seq, -seq, 2.0 + seq)

    rows = 0
    ci = 0
    for sec in range(8):
        batch = [_data_packet(row(sec), timestamp=1000 + sec, sequence=sec)]
        if sec % 2 == 1 and ci < len(chunks):
            batch.append(chunks[ci])           # one schema chunk every other call
            ci += 1
        if sec % 4 == 0:
            batch.append(_identifier_packet(items))  # plain IDENTIFIER periodically
        proc.process_bytes_list(batch)
        frames = proc.flush()                  # flush each cycle, like the scheduler
        for df in frames.values():
            if df is not None and df.height:
                rows += df.height

    # Rows must actually flow once the schema completes (the bug symptom was 0).
    assert rows > 0, "no rows emitted across multiple flush cycles"
    # Only the leading DATA (before any schema was known) may be unresolved.
    assert proc.statistics.received_unresolved_data_packets <= 1
    # Columns carry the wire-learned dotted names, not by-index fallbacks.
    learned = proc.get_learned_descriptors()
    assert learned.find_by_id(10).get_name() == "grp.a"
    assert learned.find_by_id(40).get_name() == "grp.d"
