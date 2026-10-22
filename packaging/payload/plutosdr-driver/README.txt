PlutoSDR Windows USB driver

This directory contains the Analog Devices PlutoSDR-M2K Windows driver files.
The catalogs are signed by Analog Devices International UC. Run
install-driver.cmd and approve the Windows administrator prompt.

After installation, disconnect and reconnect the PlutoSDR. For a network
connection, SDR++ also accepts a complete libiio URI such as:

    ip:192.168.1.10

The bundled diagnostic command can test the same address without starting
SDR++:

    ..\sdrpp\iio_info.exe -u ip:192.168.1.10

The unofficial PlutoSDR must expose an IIO network context and its firmware
must provide the ad9361-phy and cf-ad9361-lpc devices.
