idf_component_register(
    SRCS "openclaw_client.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_websocket_client json esp_event
    PRIV_REQUIRES esp_timer ed25519_lib mbedtls
)
