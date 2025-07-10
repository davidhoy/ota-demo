idf_component_register(SRCS 
                       "main.c"
                       #"flush_control.c"
                       "console.c"
                       #"console_config.c"
                       #"console_solenoid.c"
                       #"realtime_stats.c"
                       #"led_manager.c"
                       #"water_pressure.c"
                       "settings.c"
                       #"event_manager.c"
                       #"button_monitor.c"
                       #"solenoid_control.c"
                       #"adc_measurements.c"
                       #"demo_mode.c"
                       #"nmea_task.cpp"
                       #"pgn_handlers.cpp"
                       #"pgn_senders.cpp"
                       #"pgn130820_group_function.cpp"
                       #"realtime_stats.c"
                       #"watchdog.c"
                       "wifi_ap.c"
                       "web_server.c"
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)

# Automatically define the app version
#idf_build_set_property(COMPILE_DEFINITIONS "-DAPP_VERSION=\"0.0.5\"" APPEND)
execute_process(
    COMMAND git describe --tags --always
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
idf_build_set_property(COMPILE_DEFINITIONS "-DAPP_VERSION=\"${GIT_VERSION}\"" APPEND)


