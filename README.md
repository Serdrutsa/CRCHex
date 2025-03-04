# CRCHex
A CLI tool to add CRC to files in Intel HEX format.


Usage: CRCHex.exe -i input.hex [OPTIONS]

Options:
* -o <file_name>    - output file name (if not specified the CRC is just printed to stdout)
* -c <CRC type>     - 1=CRC8, 2=CRC16, 4=CRC32 (default: 4)
* -s <startAddress> - address to start CRC calculation (default: start of HEX)
* -e <endAddress>   - address to end CRC calculation (default: end of HEX)
* -a <crcAddress>   - address to put CRC (default: end of HEX)

Example:
```
CRCHex.exe -i input.hex -o output.hex -c 2 -s 0x5C00 -e 0x45BFF -a 0xFF7C
```
