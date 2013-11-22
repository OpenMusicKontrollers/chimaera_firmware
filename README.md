# Chimaera

## a polymagnetophonic theremin

The general idea was to develop an expressive music controller. The general characteristics that had to be fulfilled were:

- playable with fingers (those are the body parts that we have a very fine grained control on)
- design should qualify for do-it-yourself production
- entirely based on and released as open source hardware and software
- adhere to the KISS principle (were our favorite wording is: *keep it simple and smart*
- highly sensitive, e.g. has to react to subtle changes in input (a prerequisite for an expressive play with vibratos, tremolos, ...)
- high update rates (2-5kHz)
- driver-less communication and integration into any setup or operating system
- possibility for network performances inherently included

The outcome was a device that is best described as a polyphonic theremin, based on magnetic distance sensing to permanent magnets atttached to fingers relative to an array of high accuracy linear hall effect sensors. An open source prototyping board with an ARM Cortex M4 MCU acts as the central computing unit, reading out the sensors, computing multi touch recognition and sending output via Ethernet.

The device therefore features two continuous dimensions. The X-dimension tells about the fingers position in the sensor array and most of the time will correspond to pitch. The P-dimension tells about finger proximity to the sensor array and may correspond to amplitude, filter cutoff frequency, modulation, etc.

<http://open-music-kontrollers.ch/chimaera/about/>
