idf_component_register(
    SRCS
        "main.c"
        "../../../MCUSilk/CPU_usage.c"
        "../../../MCUSilk/isr_trace.c"
    PRIV_REQUIRES spi_flash
    INCLUDE_DIRS
        "."
        "../../../MCUSilk"
        "../../../AWS_WIFI"
        REQUIRES esp_driver_gpio esp_wifi nvs_flash esp_event esp_netif mqtt json
)

# Embed the certificates into the binary
target_add_binary_data(${COMPONENT_TARGET} "../../../AWS_WIFI/root_ca.pem" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "../../../AWS_WIFI/device.crt" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "../../../AWS_WIFI/private.key" TEXT)

target_add_binary_data(${COMPONENT_TARGET} "../../../AWS_WIFI/config.json" TEXT)