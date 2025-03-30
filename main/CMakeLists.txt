# Embed the server root certificate into the final binary
idf_build_get_property(project_dir PROJECT_DIR)
idf_component_register(SRCS "simple_ota_example.c"
                    INCLUDE_DIRS "."
                    REQUIRES 
                            epd_driver 
                            driver 
                            esp_event 
                            esp_http_client 
                            esp_https_ota 
                            esp_http_server 
                            freertos 
                            log 
                            nvs_flash 
                            esp_system 
                            esp_wifi 
                            esp_netif 
                            mbedtls
                            app_update 

                    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)
