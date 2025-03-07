idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS ".")

REQUIRES esp_https_ota
REQUIRES app_update
REQUIRES mbedtls