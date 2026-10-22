# Physical-radio frequency control

TinyDoppler's **电台控频…** settings enable a separate Hamlib `rigctld` endpoint.
Only the Doppler-corrected receive frequency (`F <Hz>`) is sent, on each valid
tracking tick. There are no PTT, mode, split, start/stop or VFO commands. SDR#
and SDR++ retain their existing tuning paths. Leave this option disabled when
using an SDR alone.

Install Hamlib for your OS, select the radio model and serial port in rigctld,
and match its baud rate to the radio. Confirm the model number with `rigctl -l`
on the installed Hamlib version. Examples (replace the port and baud rate):

```sh
# IC-705 (model 3085); IC-9700 uses model 3081.
rigctld -m 3085 -r /dev/ttyUSB0 -s 19200 -T 127.0.0.1 -t 4534
# macOS serial devices normally use /dev/cu.*; Windows uses COMn.
```

In TinyDoppler enable radio control, address `127.0.0.1`, port `4534`. Keep
4532 for an existing SDR++ Rigctl Server. The radio's intended receive VFO
must already be selected; do not run rigctld in explicit VFO mode (`-o`).
TinyDoppler does not switch VFOs. Input mode and the radio's 12 kHz USB IF
audio output are separate from CAT control; choose the appropriate audio
input in the receiver launcher.

Invalid tracking data closes the radio connection, and reconnection waits for
a fresh frequency. Disabled is the default. A TCP protocol regression test
covers connection, exact command bytes, disable and reconnect; it does not
replace an IC-705/IC-9700 hardware test.

Protocol reference: [Hamlib rigctld manual](https://hamlib.sourceforge.net/html/rigctld.1.html).
