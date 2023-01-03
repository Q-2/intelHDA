# intelHDA
a barebones Intel HDA driver

This repository contains part of the code used to implement an Intel High Definition Audio driver for the UIUC ECE391 OS competition. Because this is only the audio driver, it is incomplete and contains references to functions and files not present.

If you are in ECE 391 and would like to implement an Intel HD Audio driver for your OS, please cite this in your code.

# Notes for ECE391 Students

- You will need some way of allocating physical memory location. 

- Additionally, you'll have to write some PCI interfacing functions.

- This needs the following line in your QEMU boot file/line. `-device intel-hda -device hda-duplex` The first part enables the HDA bus, and the second adds the device onto the bus. If you only have the first part, it will not work since no devices will be on the virtual bus. (Specifically, STATESTS will remain 0 and never update.)
