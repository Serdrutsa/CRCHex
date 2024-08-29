# CRCHex - A tool to add CRC to files in Intel HEX format
<br>
Usage: CRCHex.exe -i=<input.hex> [options]<br>
	Options:<br>
	-c=<CRC_type>      - 0=CRC16+Len, 1=Checksum32, 2=Checksum16+Len (default: 0)<br>
	-s=<start_address> - address to start CRC calculation (default: start of HEX)<br>
	-e=<end_address>   - address to end CRC calculation (default: end of HEX)<br>
	-a=<CRC_address>   - address to put CRC at (default: end of HEX)<br>
	-p=1               - print info only (no file change)<br>

Example: CRCHex.exe -i=fw.hex -c=1 -s=0x5C00 -e=0x45BFF -a=0xFF7C<br>
