deps_config := \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/aws_iot/Kconfig \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/bt/Kconfig \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/esp32/Kconfig \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/ethernet/Kconfig \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/fatfs/Kconfig \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/freertos/Kconfig \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/log/Kconfig \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/lwip/Kconfig \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/mbedtls/Kconfig \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/openssl/Kconfig \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/spi_flash/Kconfig \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/huangxiaoming/warehouse/hxm/esp32/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
