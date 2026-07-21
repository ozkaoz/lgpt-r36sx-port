#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
bridge=(root/'source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp').read_text()
daemon=(root/'device/r36s_u2523_usb_audio_io.c').read_text()
setup=(root/'device/otg_u241_setup_once.sh').read_text()
assert 'ensure_monitor_fifo_node' in bridge
assert 'mkfifo(kCaptureMonitorFifo, 0666)' in bridge
assert bridge.index('ensure_monitor_fifo_open();') < bridge.index('write_runtime_file_atomic(kCaptureMonitor, state)')
assert 'U2.51.7 MONITOR FIFO HANDSHAKE' in bridge
assert 'ensure_monitor_fifo_node' in daemon
assert 'U2517_MONITOR_FIFO_STARTUP_READY' in daemon
assert 'ensure_monitor_fifo' in setup
assert 'MONITOR_FIFO=/tmp/r36sx_usb_capture_monitor_fifo' in setup
assert 'R36SX_USB_AUDIO_DAEMON_ABI=7' in bridge
assert 'R36SX_USB_AUDIO_DAEMON_ABI=7' in daemon
print('TEST_U2517_MONITOR_FIFO_HANDSHAKE_OK')
