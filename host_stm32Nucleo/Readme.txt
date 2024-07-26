qcc730
├───App
│       led.c
│       led.h
│       qcc730_spi_host.h
│       qcc730_spi_hostdemo.c
│       spi.c
│       spi.h
│
└───Service
        fermion_cmn_api.h
        fermion_table_api.h
        qapi_spi.c
        qapi_spi.h
        qcspi_driver.c
        qcspi_driver.h
        qcspi_hal.c
        qcspi_hal.h
        ring.c
        ring.h
        transport_layer.c
        transport_layer.h



APP folder includes some application files for testing. Spi.c/h files are for raw mode. This will verify SPI RAW write/read operation without Ring buffer. Assume it will not be used in product.

Service folder includes QAPI, qcspi driver and ring buffer service. For transport_layer.c/h files, it implements simple service with interrupt mode but it doesn't work well now for multi-times as no typical use case now. Host should design its own framework based the specific case.

