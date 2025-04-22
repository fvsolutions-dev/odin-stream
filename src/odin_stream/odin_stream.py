import odin_stream 
import polars as pl
import odin_stream_cpp
from odin_db import OdinDBModel, OdinDBParameterGroupModel,OdinDBParameterModel,OdinDBTypeDefinitionModel,ODINDBModelType

class StreamProcessor:
    processor: odin_stream_cpp.StreamProcessor

    def __init__(self, odin_db: OdinDBModel):
        self.processor = odin_stream_cpp.StreamProcessor(StreamProcessor.odin_db_to_type_descriptors(odin_db))

    def process_bytes_list(self, data: list[bytes]):
        """
        Process a list of bytes and add them to the processor.
        """
        self.processor.process_bytes_list(data)

    @staticmethod
    def odin_db_to_flat_list(odin_db: OdinDBParameterGroupModel) -> list[OdinDBParameterModel]:
        """
        Convert OdinDBModel to a flat list of ParameterSets.
        """
        flat_list = []
        for parameter in odin_db.parameters:
            if isinstance(parameter, OdinDBParameterModel):
                flat_list.append(parameter)
            elif isinstance(parameter, OdinDBParameterGroupModel):
                flat_list.extend(StreamProcessor.odin_db_to_flat_list(parameter))
        return flat_list
    
    @staticmethod
    def odin_db_type_to_odin_stream_type(odin_type: ODINDBModelType) -> odin_stream_cpp.PrimitiveTypeDescriptor:
        """
        Convert OdinDBModelType to odin_stream_cpp PrimitiveType.
        """
        if odin_type == ODINDBModelType.U8:
            return odin_stream_cpp.PrimitiveTypeDescriptor(odin_stream_cpp.UINT8)
        elif odin_type == ODINDBModelType.U16:
            return odin_stream_cpp.PrimitiveTypeDescriptor(odin_stream_cpp.UINT16)
        elif odin_type == ODINDBModelType.U32:
            return odin_stream_cpp.PrimitiveTypeDescriptor(odin_stream_cpp.UINT32)
        elif odin_type == ODINDBModelType.U64:
            return odin_stream_cpp.PrimitiveTypeDescriptor(odin_stream_cpp.UINT64)
        elif odin_type == ODINDBModelType.I8:
            return odin_stream_cpp.PrimitiveTypeDescriptor(odin_stream_cpp.INT8)
        elif odin_type == ODINDBModelType.I16:
            return odin_stream_cpp.PrimitiveTypeDescriptor(odin_stream_cpp.INT16)
        elif odin_type == ODINDBModelType.I32:
            return odin_stream_cpp.PrimitiveTypeDescriptor(odin_stream_cpp.INT32)
        elif odin_type == ODINDBModelType.I64:
            return odin_stream_cpp.PrimitiveTypeDescriptor(odin_stream_cpp.INT64)
        elif odin_type == ODINDBModelType.F32:
            return odin_stream_cpp.PrimitiveTypeDescriptor(odin_stream_cpp.FLOAT32)
        elif odin_type == ODINDBModelType.F64:
            return odin_stream_cpp.PrimitiveTypeDescriptor(odin_stream_cpp.FLOAT64)
        elif odin_type == ODINDBModelType.BOOL:
            return odin_stream_cpp.PrimitiveTypeDescriptor(odin_stream_cpp.BOOL)
        elif odin_type == ODINDBModelType.CHAR:
            return odin_stream_cpp.PrimitiveTypeDescriptor(odin_stream_cpp.CHAR)
        
        else:
            raise NotImplementedError(f"Type '{odin_type}' not implemented yet.")        

    @staticmethod
    def odin_db_to_type_descriptors(odin_db: OdinDBModel) -> odin_stream_cpp.TypeDescriptors:
        """
        Convert OdinDBModel to TypeDescriptors for StreamProcessor.
        """
        descriptors : dict[str, odin_stream_cpp.StructDescriptor|odin_stream_cpp.PrimitiveTypeDescriptor] = {}
        assert odin_db.types is not None, "OdinDBModel types should not be None"

        for type_name, _type in odin_db.types.items():
            structure = odin_stream_cpp.StructDescriptor()

            for name, odin_type in _type.structure.items():
                if isinstance(odin_type, ODINDBModelType):                    
                    structure.add_member(name, StreamProcessor.odin_db_type_to_odin_stream_type(odin_type))

                elif isinstance(odin_type, OdinDBTypeDefinitionModel):
                    # structure.add_member(name, odin_stream_cpp.StructDescriptor())
                    print(f"SKIPPING member {name} of type {odin_type}")
            
            print(f"Registerd type {type_name} with size {structure.get_size()}")

            descriptors[type_name] = structure
            # 

        # Add primitive types
        for type_name, _type in ODINDBModelType.__members__.items():
            descriptors[type_name.lower()] = StreamProcessor.odin_db_type_to_odin_stream_type(_type)
        
        parameter_descriptors = {}
        for parameter in StreamProcessor.odin_db_to_flat_list(odin_db.root):
            element_type = descriptors.get(parameter.element_type)

            if element_type is None:
                print(f"Type {parameter.element_type} not found in descriptors, skipping parameter {parameter.name}")
                continue

            parameter_descriptors[parameter.global_id] = element_type
            
        # Print all parameter descriptors
        for parameter_id, descriptor in parameter_descriptors.items():
            print(f"Parameter ID: {parameter_id:08X} Size: {descriptor.get_size()}")

        return odin_stream_cpp.TypeDescriptors(parameter_descriptors)