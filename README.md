# Printer_3D
WiFi SD card interface to Ender 3D printer using webDav network file sharing.

The implementation works by controlling the SD card via the simple SPI interface from the ESp-12 side -- Unfortunately the new Ender FW now controlls the SD card with the full speed 4 dataline SPI interface which is not compatible with the SPI mode and once swtched into one mode the SD card can only switched into a different mode by completely resetting it and starting from fresh.  
This makes the device somewhat useless as it has to be removed and re-inserted in the Ender-3 for such a restart to take effect.  There is a possibility that the cad detect switch inside the E-3 can be opend/closed to force a reinit on that side, but that seems somewhat impractical haveing to bring out those wires -- Have not checked yet if the E3 actually uses the CD pins on the SD holder or not.

