# Devterm EXT Cartridge Daemon

## Building

This software requires the following libraries to be installed
 - gpiod 
 - systemd
 - notify
 - gdk_pixbuf-2.0
 - gio-2.0
 - gobject-2.0
 - glib-2.0

Then, simply call `make all`.

## Installation

After a successful build, call `make install` via `sudo` or `doas`.
Then make systemd launch the service at startup with `systemctl enable cartridged` again as superuser. 

## Configuration

### Daemon configuration

To configure `cartridged`, edit the file at `/etc/cartridged/config.ini`.
Configurable fields are:
 - `db_path`: specifies where the description files for any given cartridge number are stored.
 - `notifications`: if set to `yes`, `cartridged` will alert all users on `DISPLAY=:0` that a cartridge is inserted, or could not be detected properly.
 
 ### Cartridge DB Unit File
 
These are the configuration files for any given cartridge and are installed by default in `/etc/cartridged/cartdb/`.
It is a simple ini file describing the cartridge, and for now contain systemd services to start or stop on either insertion or removal of a cartridge.
There are plans to do device tree overlays in the future too.
The file name must be prefixed with the cartridge identifer number in hexadecimal and as encoded in its hardware as a prefix.
For example, a cartridge with number 238 has the unit file named `ee-examplecart.cart`.

Here is an example unit file as taken from the thermal printer cartridge:
```
[Cartridge]
# Name of the cartridge
Name=Printer
# Short description
Description=Thermal Printer

# Services to start are configured in [Service <yourname>] sections
# The order of definition is the order to launch.
[Service socat]
# Currently the Scope key is only effective for System-level services
Scope=System
# The actual name of the service to start or stop
Unit=printer-cartridge-socat

# As this is declared second, it starts and stops after the socat service
[Service printer]
Scope=System
Unit=printer-cartridge
```

