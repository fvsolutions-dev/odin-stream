import polars as pl
from odin_db import OdinDBModel

import odin_stream_cpp

from .odin_db_mapping import (
    db_type_to_steam_type,
    get_type_dict,
    odin_db_to_flat_list,
    parameter_to_odin_param,
)


class StreamProcessor:
    processor: odin_stream_cpp.StreamProcessor

    def __init__(self, odin_db: OdinDBModel | None = None, silent_errors: bool = False):
        self.parameter_map = odin_stream_cpp.ParameterMapDescriptor()

        if odin_db is not None:
            # OdinDB supplied: bake its parameters in up front (original path).
            self.type_dict = get_type_dict(odin_db)
            for parameter in odin_db_to_flat_list(odin_db.root):
                param = parameter_to_odin_param(parameter, self.type_dict)
                self.parameter_map.add_parameter(param)
        else:
            # No OdinDB: wire-only decode. Names/types are learned at runtime from
            # self-describing 0x03 IDENTIFIER_EXT packets.
            self.type_dict = None

        self.processor = odin_stream_cpp.StreamProcessor(
            self.parameter_map, silent_errors
        )

    def get_learned_descriptors(self) -> odin_stream_cpp.ParameterMapDescriptor:
        """Schema learned purely from the wire (0x03 IDENTIFIER_EXT packets)."""
        return self.processor.get_learned_descriptors()

    def get_schema_progress(self) -> dict[int, odin_stream_cpp.SchemaProgress]:
        """Per-identifier schema completeness, and what each set is still waiting on.
        """
        return self.processor.get_schema_progress()

    def process_bytes_list(self, data: list[bytes]):
        """
        Process a list of bytes and add them to the processor.
        """
        self.processor.process_packets(data)

    def flush(self) -> dict[int, pl.DataFrame]:
        sets = {}
        for key in self.processor.get_parameter_set_identifiers():
            param_set = self.processor.get_parameter_set(key)

            # print(f"Processing parameter set {key} with {param_set} parameters")

            arrow_table = param_set.flush_to_arrow_table()
            
            # print(f"Arrow table for parameter set {arrow_table} rows")

            sets[key] = pl.from_arrow(
                arrow_table
            )
        return sets

    @property
    def statistics(self) -> odin_stream_cpp.OdinStreamStatistics:
        return self.processor.statistics
