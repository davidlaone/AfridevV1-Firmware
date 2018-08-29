Release Version 3.5

Application Release Version 3.5
Bootloader Release Version 1.1
Release Date: July 25th, 2018

Bootloader Changes
There is no change to the Bootloader in this release.  As this release targeted sensors in the field, the Bootloader 
remains the version that the sensors were shipped with, which is version 1.1.

Application Changes
This version is a substantial re-write of the Outpour 2.1 firmware release.  The following items were targeted as the 
goal of the re-write:
 - Replace the existing water sensing algorithm with one developed by IPS
 - Change the message formats to be compatible with the IOT platform
 - Reduce existing code size to fit the above changes

OTA messages are command messages sent by IOT platform to the Sensor.  Due to code size limitations, not all over-the-air 
(OTA) messages supported by the Cascade product could be supported in the Afridev (version 1) product.  

