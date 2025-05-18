import odin_stream_cpp 
import polars as pl
import odin_stream_cpp
from odin_db import OdinDBModel
from .odin_db_mapping import odin_db_to_flat_list, get_type_dict, db_type_to_steam_type,parameter_to_odin_param

class StreamProcessor:
    processor: odin_stream_cpp.StreamProcessor

    def __init__(self, odin_db: OdinDBModel,silent_errors: bool = False):
        self.type_dict = get_type_dict(odin_db)

        self.parameter_map = odin_stream_cpp.ParameterMapDescriptor()

        for parameter in odin_db_to_flat_list(odin_db.root):
            param = parameter_to_odin_param(parameter, self.type_dict)
            self.parameter_map.add_parameter(param)

        self.processor = odin_stream_cpp.StreamProcessor(self.parameter_map, silent_errors)

    def process_bytes_list(self, data: list[bytes]):
        """
        Process a list of bytes and add them to the processor.
        """
        self.processor.process_packets(data)

    def flush(self) -> dict[int, pl.DataFrame]:
        sets = {}
        for key in self.processor.get_parameter_set_identifiers():
            sets[key] = pl.from_arrow(self.processor.get_parameter_set(key).flush_to_arrow_table())
        return sets   
    
    @property
    def statistics(self) -> odin_stream_cpp.OdinStreamStatistics:
        return self.processor.statistics
    
    