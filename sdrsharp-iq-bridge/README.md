# 阿斯图系列本地 I/Q 与多普勒插件 for SDR#

SDR# Studio v1920 compatible .NET Framework 4.6 x86 plugin. It registers an
`IIQProcessor` at `ProcessorType.DecimatedAndFilteredIQ` and publishes the
filtered complex float I/Q stream through the named shared-memory ring
`Local\ASRTU_IQ_BRIDGE_V1`.

The bridge is local-only. It does not install an audio driver, open a network
port, or modify the I/Q samples in SDR#.

SDR# may deliver filtered I/Q at rates such as 56.25 kS/s. The plugin performs
continuous complex linear resampling and always publishes 48 kS/s for the
decoder. Header offset 8 contains the 48 kS/s bridge rate; offset 32 records
the current SDR# input rate.
