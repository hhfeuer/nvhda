nvhda
-----

General info
------------
Linux kernel module to turn on/off Nvidia HD audio device on notebooks
Blatantly copied from Lekensteyn's bbswitch module

Install
-------

	# make
	# sudo make install
	# sudo modprobe nvhda

Usage
-----

Since its logic is copied from the bbswitch module, it works like that:

### Get status:

	# cat /proc/acpi/nvhda

### Turn audio on/off:
	# sudo tee /proc/acpi/nvhda <<<ON
	# sudo tee /proc/acpi/nvhda <<<OFF

Check dmesg for messages.
