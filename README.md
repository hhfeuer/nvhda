nvhda
-----

General info
------------
Linux kernel module to turn on/off Nvidia HD audio device on notebooks. Blatantly copied from Lekensteyn's bbswitch module.
 - Due to a bug regarding re-reading the header type in kernels 4.9 - 4.13, this won't work on these series. Kernels 4.4, 4.14+ are known to work
 - This module will (hopefully) once being superseded by Lukas Wunner's patches https://bugs.freedesktop.org/show_bug.cgi?id=75985#c37

Install
-------

	# make
	# sudo make install

Install using DKMS
------------------

	# sudo make -f Makefile.dkms

Uninstall
---------

	# sudo make uninstall

or

	# sudo make -f Makefile.dkms uninstall

Usage
-----

Since its logic is copied from the bbswitch module, it works like that.
### Load Module
 	# sudo modprobe nvhda

### Get status:

	# cat /proc/acpi/nvhda

### Turn audio on/off:
	# sudo tee /proc/acpi/nvhda <<<ON
	# sudo tee /proc/acpi/nvhda <<<OFF

Check dmesg for messages.

How it works
------------
See:
 - https://devtalk.nvidia.com/default/topic/1024022/linux/gtx-1060-no-audio-over-hdmi-only-hda-intel-detected-azalia/post/5211273/#5211273
 - https://bugs.freedesktop.org/show_bug.cgi?id=75985

ToDo
----

 - lots of things
