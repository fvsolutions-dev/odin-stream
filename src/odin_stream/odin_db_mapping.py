import odin_db
from odin_db import OdinDBModel, ODINDBModelType, OdinDBTypeDefinitionModel

import odin_stream_cpp


def odin_db_to_flat_list(
    group: odin_db.OdinDBParameterGroupModel,
) -> list[odin_db.OdinDBParameterModel]:
    """
    Convert OdinDBModel to a flat list of ParameterSets.
    """
    flat_list = []
    for parameter in group.parameters:
        if isinstance(parameter, odin_db.OdinDBParameterModel):
            flat_list.append(parameter)
        elif isinstance(parameter, odin_db.OdinDBParameterGroupModel):
            flat_list.extend(odin_db_to_flat_list(parameter))
    return flat_list


# A composite type is can contain one or more primitive subtypes, in a later stage they can contain other composite types
# def odin_type_dedintion_to_non_composite(
#     structure: "dict[str, OdinDBTypeDefinitionModel | ODINDBModelType]",
# ) -> "dict[str, ODINDBModelType]":
#     """
#     Convert OdinDBModel to a flat list of TypeDefinitions.
#     """
#     flat_structure = {}

#     for name, sub_type in structure.items():
#         print(name)
#         if isinstance(sub_type.structure, dict):
#             print(f"Converting member {name} of type {sub_type.name}")
#         # print(f"Converting member {name} of type {sub_type.name}")

#     return master_type


def db_type_to_steam_type(
    name: str, type: odin_db.OdinDBTypeDefinitionModel
) -> odin_stream_cpp.CompositeTypeDescriptor | odin_stream_cpp.PrimitiveTypeDescriptor:
    composite_descriptor = odin_stream_cpp.CompositeTypeDescriptor()

    if isinstance(type.structure, odin_db.ODINDBModelType):
        return odin_stream_cpp.PrimitiveTypeDescriptor.get_by_name(
            type.structure.name.lower()
        )

    elif isinstance(type.structure, dict):
        for sub_name, sub_type in type.structure.items():
            if isinstance(sub_type, odin_db.ODINDBModelType):
                descriptor = odin_stream_cpp.PrimitiveTypeDescriptor.get_by_name(
                    sub_type.name.lower()
                )
                composite_descriptor.add_member(sub_name, descriptor)

            elif isinstance(sub_type.structure, odin_db.ODINDBModelType):
                descriptor = odin_stream_cpp.PrimitiveTypeDescriptor.get_by_name(
                    sub_type.structure.name.lower()
                )
                composite_descriptor.add_member(sub_name, descriptor)

            elif isinstance(sub_type.structure, dict):
                for sub_sub_name, sub_sub_type in sub_type.structure.items():
                    if isinstance(sub_sub_type, odin_db.ODINDBModelType):
                        descriptor = (
                            odin_stream_cpp.PrimitiveTypeDescriptor.get_by_name(
                                sub_sub_type.name.lower()
                            )
                        )
                        composite_descriptor.add_member(
                            f"{sub_name}_{sub_sub_name}", descriptor
                        )

                    elif isinstance(sub_sub_type.structure, odin_db.ODINDBModelType):
                        descriptor = (
                            odin_stream_cpp.PrimitiveTypeDescriptor.get_by_name(
                                sub_sub_type.structure.name.lower()
                            )
                        )
                        composite_descriptor.add_member(
                            f"{sub_name}_{sub_sub_name}", descriptor
                        )

                    elif isinstance(sub_sub_type.structure, dict):
                        raise ValueError(
                            "maximum depth of composite types is 2, please flatten the structure"
                        )

    return composite_descriptor


def get_type_dict(
    db: odin_db.OdinDBModel,
) -> dict[
    str,
    odin_stream_cpp.CompositeTypeDescriptor | odin_stream_cpp.PrimitiveTypeDescriptor,
]:
    assert db.types is not None, "OdinDBModel types should not be None"

    type_dict = {}
    for type_name, type in db.types.items():
        type_dict[type_name] = db_type_to_steam_type(type_name, type)

    for type_name, _type in odin_db.ODINDBModelType.__members__.items():
        type_dict[type_name.lower()] = (
            odin_stream_cpp.PrimitiveTypeDescriptor.get_by_name(_type.name.lower())
        )

    return type_dict


def parameter_to_odin_param(
    parameter: odin_db.OdinDBParameterModel,
    type_dict: dict[
        str,
        odin_stream_cpp.CompositeTypeDescriptor
        | odin_stream_cpp.PrimitiveTypeDescriptor,
    ],
) -> odin_stream_cpp.ParameterDescriptor:
    element_type = type_dict.get(parameter.element_type)

    if element_type is None:
        return odin_stream_cpp.ParameterDescriptor.unknown_type(
            parameter.global_id, parameter.global_name
        )

    return odin_stream_cpp.ParameterDescriptor(
        parameter.global_id, parameter.global_name, element_type
    )


#         """
#         Convert OdinDBModel to TypeDescriptors for StreamProcessor.
#         """
#         descriptors : dict[str, odin_stream_cpp.StructDescriptor|odin_stream_cpp.PrimitiveTypeDescriptor] = {}
#         assert odin_db.types is not None, "OdinDBModel types should not be None"

#         for type_name, _type in odin_db.types.items():
#             structure = odin_stream_cpp.StructDescriptor()

#             for name, odin_type in _type.structure.items():
#                 if isinstance(odin_type, ODINDBModelType):
#                     structure.add_member(name, StreamProcessor.odin_db_type_to_odin_stream_type(odin_type))

#                 elif isinstance(odin_type, OdinDBTypeDefinitionModel):
#                     # structure.add_member(name, odin_stream_cpp.StructDescriptor())
#                     print(f"SKIPPING member {name} of type {odin_type}")

#             print(f"Registerd type {type_name} with size {structure.get_size()}")

#             descriptors[type_name] = structure
#             #

#         # Add primitive types
#         for type_name, _type in ODINDBModelType.__members__.items():
#             descriptors[type_name.lower()] = StreamProcessor.odin_db_type_to_odin_stream_type(_type)

#         parameter_descriptors = {}
#         for parameter in StreamProcessor.odin_db_to_flat_list(odin_db.root):
#             element_type = descriptors.get(parameter.element_type)

#             if element_type is None:
#                 print(f"Type {parameter.element_type} not found in descriptors, skipping parameter {parameter.name}")
#                 continue

#             parameter_descriptors[parameter.global_id] = element_type

#         # Print all parameter descriptors
#         for parameter_id, descriptor in parameter_descriptors.items():
#             print(f"Parameter ID: {parameter_id:08X} Size: {descriptor.get_size()}")

#         return odin_stream_cpp.TypeDescriptors(parameter_descriptors)
