# ZBeam - Advanced Flashlight Firmware

ZBeam is a high-performance flashlight firmware built on the Zephyr RTOS. It features a sophisticated UI, thermal regulation, battery monitoring, and support for multiple microcontrollers including ESP32-C3, STM32, and more.

## Features

- **Advanced UI**: Anduril-inspired interface with ramping (smooth/stepped), strobes, and configuration menus.
- **Multi-Architecture**: Support for ESP32, STM32, nRF52, and CH32V series.
- **Thermal Regulation**: PID-based thermal management to prevent overheating.
- **Battery Check**: Voltage readout via blink codes.
- **Configurability**: extensive Kconfig options for customization.

## Supported Hardware

The project explicitly supports the following boards:
- `esp32c3_supermini`
- `esp32c3_devkitm`

Support for other families (STM32F4/L4, nRF52, CH32V) is enabled via Kconfig but may require specific board overlay configuration.

## Build Instructions

This project is a valid Zephyr application.

### Prerequisites

- Zephyr SDK and West tool.

### Building

To build for the ESP32-C3 SuperMini:

```bash
west build -b esp32c3_supermini
```

To build for another board:

```bash
west build -b <your_board_name>
```

### Flashing

```bash
west flash
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for details on how to contribute.

## License

This project is licensed under the GPLHz License. See [LICENSE](LICENSE) for details.
