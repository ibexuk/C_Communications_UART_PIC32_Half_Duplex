
######################################################################
##### Open sourced by IBEX, an electronic product design company #####
#####    based in South East England.  http://www.ibexuk.com     #####
######################################################################


This is a general use PIC32 comms UART handler.

A half duplex bus is assumed (receive something then transmit a response, so RS485 bus friendly, but its easy to adapt the code to make full duplex instead (tx and rx independently).

It automatically deals with variable packet length by including a length value in the packet.  It also automatically adds simple checksumming of the packet.
