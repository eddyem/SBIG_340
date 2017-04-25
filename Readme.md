SBIG all-sky 340c driving utilite
===============================

Run `make` to compile three binary files:

* `sbig340_standalone` --- standalone utility (no daemon/client)
* `sbig340_daemon` --- pure grabbing daemon (driven by socket)
* `sbig340_client` --- pure client (take images from daemon and save locally)

When connected to daemon you can send commands "heater=1" or "heater=0":
first command will turn heater on for 10 minutes, second will turn it off.
Receiving these commands daemon won't send image, immediately disconnect.
