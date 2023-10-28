# comms
 Comms module for high speed communication in realtime between two mcu's.


## Packet
Each packet contains a header and a data part.

### Header

#### Length

#### CRC
> The checksum is only calculated for the data part of the Packet.



### Data
The data part is dynamic and based on the current request type. This ensures that the lowest amount of bytes is
exchanged between the master and the slave.