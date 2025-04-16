{
    "name": "ODIN",
    "description": "TODO",
    "creation_timestamp": 1744743305.4324188,
    "configuration_hash": 140874797129511446346926059739866948087,
    "root": {
        "type": "parameter_group",
        "name": "root",
        "global_name": "root",
        "description": "No description",
        "parameters": [
            {
                "type": "parameter_group",
                "name": "raw_sensor_data",
                "global_name": "root.raw_sensor_data",
                "description": "This contains the last conversion of the sensors, this data is can be asyncronously generated",
                "parameters": [
                    {
                        "type": "parameter",
                        "name": "BMI323_1",
                        "description": "This contains the last conversion of the bosch BMI323 sensor",
                        "global_id": 3769696256,
                        "global_name": "root.raw_sensor_data.BMI323_1",
                        "size": 34,
                        "element_type": "timestampedVec7"
                    },
                    {
                        "type": "parameter",
                        "name": "BMI323_2",
                        "description": "This contains the last conversion of the bosch BMI323 sensor",
                        "global_id": 3769761792,
                        "global_name": "root.raw_sensor_data.BMI323_2",
                        "size": 34,
                        "element_type": "timestampedVec7"
                    },
                    {
                        "type": "parameter",
                        "name": "BMI323_3",
                        "description": "This contains the last conversion of the bosch BMI323 sensor",
                        "global_id": 3769827328,
                        "global_name": "root.raw_sensor_data.BMI323_3",
                        "size": 34,
                        "element_type": "timestampedVec7"
                    },
                    {
                        "type": "parameter",
                        "name": "BMI323_4",
                        "description": "This contains the last conversion of the bosch BMI323 sensor",
                        "global_id": 3769892864,
                        "global_name": "root.raw_sensor_data.BMI323_4",
                        "size": 34,
                        "element_type": "timestampedVec7"
                    },
                    {
                        "type": "parameter",
                        "name": "ADIS16607_1",
                        "description": "This contains the last conversion of the ADIS16607 sensor",
                        "global_id": 3768647680,
                        "global_name": "root.raw_sensor_data.ADIS16607_1",
                        "size": 34,
                        "element_type": "timestampedVec7"
                    },
                    {
                        "type": "parameter",
                        "name": "ADIS16607_2",
                        "description": "This contains the last conversion of the ADIS16607 sensor",
                        "global_id": 3768713216,
                        "global_name": "root.raw_sensor_data.ADIS16607_2",
                        "size": 34,
                        "element_type": "timestampedVec7"
                    },
                    {
                        "type": "parameter",
                        "name": "LIS3MDL_1",
                        "description": "This contains the last conversion of the LIS3MDL sensor",
                        "global_id": 3770679296,
                        "global_name": "root.raw_sensor_data.LIS3MDL_1",
                        "size": 18,
                        "element_type": "timestampedVec3"
                    },
                    {
                        "type": "parameter",
                        "name": "ADXL1001_1",
                        "description": "This contains the last conversion of the LM10001 sensor",
                        "global_id": 3768582144,
                        "global_name": "root.raw_sensor_data.ADXL1001_1",
                        "size": 10,
                        "element_type": "timestampedVec1"
                    }
                ]
            },
            {
                "type": "parameter_group",
                "name": "filtered_sensor_data",
                "global_name": "root.filtered_sensor_data",
                "description": "This contains the last conversion of the sensors after filtering, this data should be generated at 600Hz",
                "parameters": [
                    {
                        "type": "parameter",
                        "name": "BMI323_1",
                        "description": "This contains the last conversion of the bosch BMI323 sensor",
                        "global_id": 3786473472,
                        "global_name": "root.filtered_sensor_data.BMI323_1",
                        "size": 34,
                        "element_type": "timestampedVec7"
                    },
                    {
                        "type": "parameter",
                        "name": "BMI323_2",
                        "description": "This contains the last conversion of the bosch BMI323 sensor",
                        "global_id": 3786539008,
                        "global_name": "root.filtered_sensor_data.BMI323_2",
                        "size": 34,
                        "element_type": "timestampedVec7"
                    },
                    {
                        "type": "parameter",
                        "name": "BMI323_3",
                        "description": "This contains the last conversion of the bosch BMI323 sensor",
                        "global_id": 3786604544,
                        "global_name": "root.filtered_sensor_data.BMI323_3",
                        "size": 34,
                        "element_type": "timestampedVec7"
                    },
                    {
                        "type": "parameter",
                        "name": "BMI323_4",
                        "description": "This contains the last conversion of the bosch BMI323 sensor",
                        "global_id": 3786670080,
                        "global_name": "root.filtered_sensor_data.BMI323_4",
                        "size": 34,
                        "element_type": "timestampedVec7"
                    },
                    {
                        "type": "parameter",
                        "name": "ADIS16607_1",
                        "description": "This contains the last conversion of the ADIS16607 sensor",
                        "global_id": 3785424896,
                        "global_name": "root.filtered_sensor_data.ADIS16607_1",
                        "size": 34,
                        "element_type": "timestampedVec7"
                    },
                    {
                        "type": "parameter",
                        "name": "ADIS16607_2",
                        "description": "This contains the last conversion of the ADIS16607 sensor",
                        "global_id": 3785490432,
                        "global_name": "root.filtered_sensor_data.ADIS16607_2",
                        "size": 34,
                        "element_type": "timestampedVec7"
                    },
                    {
                        "type": "parameter",
                        "name": "LIS3MDL_1",
                        "description": "This contains the last conversion of the LIS3MDL sensor",
                        "global_id": 3787456512,
                        "global_name": "root.filtered_sensor_data.LIS3MDL_1",
                        "size": 18,
                        "element_type": "timestampedVec3"
                    },
                    {
                        "type": "parameter",
                        "name": "ADXL1001_1",
                        "description": "This contains the last conversion of the LM10001 sensor",
                        "global_id": 3785359360,
                        "global_name": "root.filtered_sensor_data.ADXL1001_1",
                        "size": 10,
                        "element_type": "timestampedVec1"
                    }
                ]
            },
            {
                "type": "parameter_group",
                "name": "imu_output_data",
                "global_name": "root.imu_output_data",
                "description": "The output data of the IMU",
                "parameters": [
                    {
                        "type": "parameter",
                        "name": "IMU",
                        "description": "This contains the last conversion algorithm output",
                        "global_id": 3791716352,
                        "global_name": "root.imu_output_data.IMU",
                        "size": 34,
                        "element_type": "timestampedVec7"
                    }
                ]
            },
            {
                "type": "parameter_group",
                "name": "system_info",
                "global_name": "root.system_info",
                "description": "This contains the system information",
                "parameters": [
                    {
                        "type": "parameter",
                        "name": "uptime",
                        "description": "The uptime of the system in ms",
                        "global_id": 3808493568,
                        "global_name": "root.system_info.uptime",
                        "size": 4,
                        "element_type": "u32"
                    },
                    {
                        "type": "parameter",
                        "name": "boot_counter",
                        "description": "The number of times the system has been booted, this parameter is stored persistently",
                        "global_id": 3808559104,
                        "global_name": "root.system_info.boot_counter",
                        "size": 4,
                        "element_type": "u32"
                    }
                ]
            },
            {
                "type": "parameter_group",
                "name": "config",
                "global_name": "root.config",
                "description": "Persistent configuration of the system",
                "parameters": [
                    {
                        "type": "parameter_group",
                        "name": "sensor_accuisition_config",
                        "global_name": "root.config.sensor_accuisition_config",
                        "description": "Configuraiton of the sensor accuisition",
                        "parameters": [
                            {
                                "type": "parameter",
                                "name": "simulate_sensors",
                                "description": "If true the sensor accuustion will be simulated",
                                "global_id": 3519086848,
                                "global_name": "root.config.sensor_accuisition_config.simulate_sensors",
                                "size": 1,
                                "element_type": "bool"
                            },
                            {
                                "type": "parameter_group",
                                "name": "filter",
                                "global_name": "root.config.sensor_accuisition_config.filter",
                                "description": "This contains the last conversion of the sensors",
                                "parameters": [
                                    {
                                        "type": "parameter",
                                        "name": "BMI_GYRO",
                                        "description": "This contains the configuration of the pre filter for the BMI gyro",
                                        "global_id": 3519090689,
                                        "global_name": "root.config.sensor_accuisition_config.filter.BMI_GYRO",
                                        "size": 36,
                                        "element_type": "second_order_IIR_filter_config"
                                    },
                                    {
                                        "type": "parameter",
                                        "name": "BMI_ACCEL",
                                        "description": "This contains the configuration of the pre filter for the BMI accel",
                                        "global_id": 3519090690,
                                        "global_name": "root.config.sensor_accuisition_config.filter.BMI_ACCEL",
                                        "size": 36,
                                        "element_type": "second_order_IIR_filter_config"
                                    },
                                    {
                                        "type": "parameter",
                                        "name": "ADXL_ACCEL",
                                        "description": "This contains the configuration of the pre filter for the ADXL accel",
                                        "global_id": 3519090691,
                                        "global_name": "root.config.sensor_accuisition_config.filter.ADXL_ACCEL",
                                        "size": 36,
                                        "element_type": "second_order_IIR_filter_config"
                                    },
                                    {
                                        "type": "parameter",
                                        "name": "LIS_MAG",
                                        "description": "This contains the configuration of the pre filter for the LIS mag",
                                        "global_id": 3519090692,
                                        "global_name": "root.config.sensor_accuisition_config.filter.LIS_MAG",
                                        "size": 36,
                                        "element_type": "second_order_IIR_filter_config"
                                    }
                                ]
                            }
                        ]
                    },
                    {
                        "type": "parameter_group",
                        "name": "algorithm_config",
                        "global_name": "root.config.algorithm_config",
                        "description": "This contains the last conversion of the sensors",
                        "parameters": [
                            {
                                "type": "parameter",
                                "name": "apply_collins_offset",
                                "description": "If True the collins position offset will be applied to compensate for sensor alignment",
                                "global_id": 3519152384,
                                "global_name": "root.config.algorithm_config.apply_collins_offset",
                                "size": 1,
                                "element_type": "bool"
                            }
                        ]
                    },
                    {
                        "type": "parameter_group",
                        "name": "calibration_config",
                        "global_name": "root.config.calibration_config",
                        "description": "Calibration configuration of the system",
                        "parameters": [
                            {
                                "type": "parameter",
                                "name": "BMI323_1",
                                "description": "Sensor calibration",
                                "global_id": 3519611136,
                                "global_name": "root.config.calibration_config.BMI323_1",
                                "size": -1,
                                "element_type": "three_axis_sensor_calibration"
                            },
                            {
                                "type": "parameter",
                                "name": "BMI323_2",
                                "description": "Sensor calibration",
                                "global_id": 3519611392,
                                "global_name": "root.config.calibration_config.BMI323_2",
                                "size": -1,
                                "element_type": "three_axis_sensor_calibration"
                            },
                            {
                                "type": "parameter",
                                "name": "BMI323_3",
                                "description": "Sensor calibration",
                                "global_id": 3519611648,
                                "global_name": "root.config.calibration_config.BMI323_3",
                                "size": -1,
                                "element_type": "three_axis_sensor_calibration"
                            },
                            {
                                "type": "parameter",
                                "name": "BMI323_4",
                                "description": "Sensor calibration",
                                "global_id": 3519611904,
                                "global_name": "root.config.calibration_config.BMI323_4",
                                "size": -1,
                                "element_type": "three_axis_sensor_calibration"
                            },
                            {
                                "type": "parameter",
                                "name": "ADIS16607_1",
                                "description": "Sensor calibration",
                                "global_id": 3519652096,
                                "global_name": "root.config.calibration_config.ADIS16607_1",
                                "size": -1,
                                "element_type": "three_axis_sensor_calibration"
                            },
                            {
                                "type": "parameter",
                                "name": "ADIS16607_2",
                                "description": "Sensor calibration",
                                "global_id": 3519652352,
                                "global_name": "root.config.calibration_config.ADIS16607_2",
                                "size": -1,
                                "element_type": "three_axis_sensor_calibration"
                            },
                            {
                                "type": "parameter",
                                "name": "LIS3MDL_1",
                                "description": "Sensor calibration",
                                "global_id": 3519660032,
                                "global_name": "root.config.calibration_config.LIS3MDL_1",
                                "size": -1,
                                "element_type": "three_axis_sensor_calibration"
                            },
                            {
                                "type": "parameter",
                                "name": "ADXL1001_1",
                                "description": "Sensor calibration",
                                "global_id": 3519651840,
                                "global_name": "root.config.calibration_config.ADXL1001_1",
                                "size": -1,
                                "element_type": "three_axis_sensor_calibration"
                            }
                        ]
                    }
                ]
            },
            {
                "type": "parameter_group",
                "name": "runtime_information",
                "global_name": "root.runtime_information",
                "description": "Runtime information of the system",
                "parameters": [
                    {
                        "type": "parameter",
                        "name": "async_simulation_task",
                        "description": "The task that simulates the sensor accuisition at high frequency",
                        "global_id": 3271622656,
                        "global_name": "root.runtime_information.async_simulation_task",
                        "size": 56,
                        "element_type": "high_resolution_timer"
                    },
                    {
                        "type": "parameter",
                        "name": "sync_simulation_task",
                        "description": "The task that simulates the sensor accuisition at 600Hz",
                        "global_id": 3271688192,
                        "global_name": "root.runtime_information.sync_simulation_task",
                        "size": 56,
                        "element_type": "high_resolution_timer"
                    },
                    {
                        "type": "parameter",
                        "name": "sync_stream_packet_generation",
                        "description": "The generation of the sync stream packet",
                        "global_id": 3271819264,
                        "global_name": "root.runtime_information.sync_stream_packet_generation",
                        "size": 56,
                        "element_type": "high_resolution_timer"
                    },
                    {
                        "type": "parameter",
                        "name": "async_stream_packet_generation",
                        "description": "The generation of the sync stream packet",
                        "global_id": 3271753728,
                        "global_name": "root.runtime_information.async_stream_packet_generation",
                        "size": 56,
                        "element_type": "high_resolution_timer"
                    },
                    {
                        "type": "parameter",
                        "name": "sync_rate_tracker",
                        "description": "The rate tracker for the sync stream packet",
                        "global_id": 3271884800,
                        "global_name": "root.runtime_information.sync_rate_tracker",
                        "size": 20,
                        "element_type": "rate_tracker"
                    },
                    {
                        "type": "parameter",
                        "name": "async_rate_tracker",
                        "description": "The rate tracker for the async stream packet",
                        "global_id": 3271950336,
                        "global_name": "root.runtime_information.async_rate_tracker",
                        "size": 20,
                        "element_type": "rate_tracker"
                    },
                    {
                        "type": "parameter",
                        "name": "total_rate_tracker",
                        "description": "The rate tracker for the total stream packet",
                        "global_id": 3272015872,
                        "global_name": "root.runtime_information.total_rate_tracker",
                        "size": 20,
                        "element_type": "rate_tracker"
                    },
                    {
                        "type": "parameter",
                        "name": "burst_unencoded_output_tracker",
                        "description": "The rate tracker for the burst unencoded output",
                        "global_id": 3272081408,
                        "global_name": "root.runtime_information.burst_unencoded_output_tracker",
                        "size": 20,
                        "element_type": "rate_tracker"
                    },
                    {
                        "type": "parameter",
                        "name": "burst_encoded_output_tracker",
                        "description": "The rate tracker for the burst encoded output",
                        "global_id": 3272146944,
                        "global_name": "root.runtime_information.burst_encoded_output_tracker",
                        "size": 20,
                        "element_type": "rate_tracker"
                    },
                    {
                        "type": "parameter",
                        "name": "burst_encoder_statistics",
                        "description": "Burst encoder statistics",
                        "global_id": 3272605696,
                        "global_name": "root.runtime_information.burst_encoder_statistics",
                        "size": -1,
                        "element_type": "burst_encoder_statistics"
                    },
                    {
                        "type": "parameter",
                        "name": "burst_decoder_statistics",
                        "description": "Burst decoder statistics",
                        "global_id": 3272671232,
                        "global_name": "root.runtime_information.burst_decoder_statistics",
                        "size": -1,
                        "element_type": "burst_decoder_statistics"
                    }
                ]
            }
        ]
    }
}