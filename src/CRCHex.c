// CRCHex.c : Defines the entry point for the console application.
//
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define	MAX_ROM_SIZE	(1024L*1024L)		// 1 MB

//------------------------------------------------------------------------
// Convert the byte to hex and write it to the file 
void PutHexByte(FILE *fp, int c) 
{ 
	char hex[17]="0123456789ABCDEF"; 
	c=c&0xFF;
	fputc(hex[c>>4],fp); 
	fputc(hex[c&0xF],fp); 
}
//------------------------------------------------------------------------
// Write a line of data to the file 
void WriteHexLine(FILE *fp, int size, unsigned short addr, int type, unsigned char *data) 
{
	int i; 
	unsigned char csum, sumOfData = 0; 
	fputc(':',fp); 
	PutHexByte(fp, size);
	PutHexByte(fp, addr>>8); 
	PutHexByte(fp, addr&0xFF); 
	PutHexByte(fp, type);
	for (i=0; i<size; i++)
	{
		PutHexByte(fp, data[i]);
		sumOfData += data[i];
	}
	csum = -(size + (addr>>8) + (addr&0xFF) + type + sumOfData); 
	PutHexByte(fp, csum);
	fputc('\n',fp);
}
//------------------------------------------------------------------------
// Calculates CRC-16
uint16_t CalcCRC16(uint8_t *buf, uint32_t len, uint32_t crc_addr)
{
    int		i;
    uint16_t    c, crc=0;
    uint32_t	addr=0;
	
  while (addr<len)
	{
		if (addr!=crc_addr && addr!=(crc_addr+1) && addr!=(crc_addr+2) && addr!=(crc_addr+3))
		{
			c = ((*buf++) ^ crc) & 0xFF;
			i = 8;
			do {
				if ( c & 1 ) {
					c >>= 1;
					c ^= 0xA001;
				}
				else c >>= 1;
			} while( --i );
			crc = (crc>>8) ^ c;
		}
		addr++;
    }
    return crc;
}
//------------------------------------------------------------------------
// Calculates Checksum-32
uint32_t CalcChecksum32(uint8_t *buf, uint32_t len, uint32_t crc_addr)
{
	uint8_t	compl=0;
	uint32_t	check_sum=0;
	uint32_t	addr=0;

	while (addr<len)
	{
		if (addr!=crc_addr && addr!=(crc_addr+2))
		{
			check_sum+=*(uint16_t*)&buf[addr];
			check_sum+=compl;
			compl++;
		}
		addr+=2;
	}
	return check_sum;
}
//------------------------------------------------------------------------
// Calculates Checksum-16
uint16_t CalcChecksum16(uint8_t *buf, uint32_t len, uint32_t crc_addr)
{
  uint16_t	check_sum=0;
	uint32_t	addr=0;
	
  while (addr<len)
	{
		if (addr!=crc_addr && addr!=(crc_addr+1))
		{
			check_sum+=*buf++;
		}
		addr++;
    }
    return check_sum;
}
//------------------------------------------------------------------------
// application entry point
int _tmain(int argc, _TCHAR* argv[])
{
	int		nLength;
	int		nStartAddr=-1, nEndAddr=-1, nCRCType=0, nCRCAddr=-1;
	int		nInfo=0;
	char	sInFname[256];
	
	printf("Add CRC to Hex tool v.1.3.0.0 (c) 2014 ADD-TECHNOLOGY / by Serghei Druta\n");
	if (argc<2) {
		printf("  Usage: CRCHex.exe -i=<input.hex> [options]\n");
		printf("    Options:\n");
		printf("    -c=<CRC_type>      - 0=CRC16+Len, 1=Checksum32, 2=Checksum16+Len (default: 0)\n");
		printf("    -s=<start_address> - address to start CRC calculation (default: start of HEX)\n");
		printf("    -e=<end_address>   - address to end CRC calculation (default: end of HEX)\n");
		printf("    -a=<CRC_address>   - address to put CRC (default: end of HEX)\n");
		printf("    -p=1               - print info only (no file change)\n");
		printf("  Example: CRCHex.exe -i=fw.hex -c=1 -s=0x5C00 -e=0x45BFF -a=0xFF7C\n");
		printf("\n");
		return 1;
	}

	sInFname[0]=0;

	// Loop through command line arguments
	for (int i=1; i<argc; i++)
	{
		nLength = strlen(argv[i]);
		
		if ( (nLength > 3) && (*(argv[i]) == '-') && (*(argv[i]+2) == '=') )
		{
			switch( *(argv[i]+1) )
			{
			case 'i':	// input file name
			case 'I':
				strcpy(sInFname, argv[i]+3);
				break;
			case 's':	// start address
			case 'S':
				nStartAddr=strtol((argv[i]+3), NULL, 0);
				break;
			case 'e':	// end address
			case 'E':
				nEndAddr=strtol((argv[i]+3), NULL, 0);
				break;
			case 'c':	// CRC type
			case 'C':
				nCRCType=strtol((argv[i]+3), NULL, 0);
				break;
			case 'a':	// CRC address
			case 'A':
				nCRCAddr=strtol((argv[i]+3), NULL, 0);
				break;
			case 'p':	// info only
			case 'P':
				nInfo=strtol((argv[i]+3), NULL, 0);
				break;
			}
		}
	}

	if (!strlen(sInFname)) {
		printf("  Error: input file not specified!\n");
		return 1;
	}

	uint8_t	*bufROM=new uint8_t[MAX_ROM_SIZE];
	if (bufROM==NULL) {
		printf("  Error: memory allocation error!\n");
		return 2;
	}
	memset(bufROM,0xFF,MAX_ROM_SIZE);

	FILE	*ifp;
	ifp=fopen(sInFname,"rt");
	if (ifp == NULL) {
		printf("  Error: input file (%s) open error!\n", sInFname);
		return 3;
	}

	uint16_t	nSegment=0;
	char	str[256], s[16];
	uint16_t	crc16=0;
	uint16_t	chksum16=0;
	uint32_t	chksum32=0;
	uint32_t	hexLastAddr=0, chkAddr;
	int		line=0;

	// Process HEX file and make ROM
	printf("  Processing file %s...\n", sInFname);
	while (fgets(str,256,ifp))
	{
		line++;
		nLength=strlen(str);
		if (nLength<11) continue;
		if (str[0]!=':') continue;
		// length
		strncpy(s,&str[1],2); s[2]=0;
		nLength=strtol(s,NULL,16);
		// checksum
		uint8_t crc=0;
		for (int i=0; i<5+nLength; i++) {
			strncpy(s,&str[1+i*2],2); s[2]=0;
			crc+=(uint8_t)strtol(s,NULL,16);
		}
		if (crc) {	// checksum error
			printf("  Error: input file (%s) CRC error in line %d!\n", sInFname, line);
			fclose(ifp);
			return 4;
		}
		// address
		strncpy(s,&str[3],4); s[4]=0;
		uint16_t nAddr=(uint16_t)strtol(s,NULL,16);
		// type
		strncpy(s,&str[7],2); s[2]=0;
		uint8_t nType=(uint8_t)strtol(s,NULL,16);
		// data
		if (nType==0) {
			int nAddress=(nSegment<<16)+nAddr;
			if (nStartAddr==-1) {
				nStartAddr=nAddress;
			}
			for (int i=0;i<nLength;i++) {
				strncpy(s,&str[9+i*2],2); s[2]=0;
				chkAddr=nAddress+i-nStartAddr;
				if (chkAddr < MAX_ROM_SIZE) {
					bufROM[nAddress+i-nStartAddr]=(uint8_t)strtol(s,NULL,16);
					if (nCRCAddr!=-1) {
						if ((nCRCAddr==nAddress+i)||(nCRCAddr+1==nAddress+i)||(nCRCAddr+2==nAddress+i)||(nCRCAddr+3==nAddress+i)) {
							printf("  Error: CRC address in use!\n");
							fclose(ifp);
							return 5;
						}
					}
				}
			}
			if ((nAddress+nLength-nStartAddr) < MAX_ROM_SIZE)
				hexLastAddr=nAddress+nLength;
		}
		else if (nType==4) {
			strncpy(s,&str[9],4); s[4]=0;
			nSegment=(uint16_t)strtol(s,NULL,16);
		}
		else if (nType==1) {
			if (nEndAddr==-1) {
				nEndAddr=hexLastAddr-1;
			}
			break;
		}
	}
	if (ifp) fclose(ifp);

	uint32_t	crcAddr=nCRCAddr;
	if (crcAddr==-1) crcAddr=hexLastAddr;

	printf("  Info: start_address=%lXh, end_address=%lXh, crc_address=%lXh\n", nStartAddr,nEndAddr,crcAddr);

	// Calculate CRC
	if (nCRCType==0) {
		crc16=CalcCRC16(bufROM,nEndAddr-nStartAddr+1,crcAddr);
		printf("        CRC16=%X\n",crc16);
	}
	else if (nCRCType==1) {
		chksum32=CalcChecksum32(bufROM,nEndAddr-nStartAddr+1,crcAddr);
		printf("        Checksum32=%lX\n",chksum32);
	}
	else if (nCRCType==2) {
		chksum16=CalcChecksum16(bufROM,nEndAddr-nStartAddr+1,crcAddr);
		printf("        Checksum16=%X\n",chksum16);
	}
	else {
		printf("  Error: unsupported CRC type!\n");
		return 6;
	}

	if (nInfo) {
		printf("Done.\n");
		return 0;
	}

	// Make a source file copy
	if (!CopyFile(sInFname,"old.hex",FALSE)) {
		printf("  Error: failed to create temporary file!\n");
		return 7;
	}

	ifp=fopen("old.hex","rt");
	if (ifp == NULL) {
		printf("  Error: temporary file open error!\n");
		return 3;
	}

	FILE	*ofp;
	ofp=fopen(sInFname,"wt");
	if (ofp == NULL) {
		printf("  Error: output file create error!\n");
		return 3;
	}

	// Write CRC to output file
	printf("  Generating output file %s...\n", sInFname);
	int		bDone=0;
	nSegment=0;
	while (fgets(str,256,ifp))
	{
		nLength=strlen(str);
		if (nLength<11) continue;
		if (str[0]!=':') continue;
		// length
		strncpy(s,&str[1],2); s[2]=0;
		nLength=strtol(s,NULL,16);
		// address
		strncpy(s,&str[3],4); s[4]=0;
		uint16_t nAddr=(uint16_t)strtol(s,NULL,16);
		int nAddress=(nSegment<<16)+nAddr;
		// type
		strncpy(s,&str[7],2); s[2]=0;
		uint8_t nType=(uint8_t)strtol(s,NULL,16);
		// data
		if (nType==4) {
			strncpy(s,&str[9],4); s[4]=0;
			nSegment=(uint16_t)strtol(s,NULL,16);
		}
		else if (nType==1) {
			if ((nCRCAddr==-1)&&(!bDone)) {
				uint8_t	crcbuf[8];
				if (nCRCType==0) {
					memcpy(crcbuf,&crc16,2);
					memcpy(crcbuf+2,&hexLastAddr,4);
					WriteHexLine(ofp,6,hexLastAddr&0xFFFF,0,crcbuf);
				}
				else if (nCRCType==1) {
					memcpy(crcbuf,&chksum32,4);
					WriteHexLine(ofp,4,hexLastAddr&0xFFFF,0,crcbuf);
				}
				else if (nCRCType==2) {
					memcpy(crcbuf,&chksum16,2);
					memcpy(crcbuf+2,&hexLastAddr,4);
					WriteHexLine(ofp,6,hexLastAddr&0xFFFF,0,crcbuf);
				}
				bDone=1;
			}
		}
		// check CRC address
		if (nCRCAddr!=-1) {
			if ((nAddress > nCRCAddr)&&(!bDone)) {
				uint8_t	crcbuf[8];
				if (nCRCType==0) {
					memcpy(crcbuf,&crc16,2);
					memcpy(crcbuf+2,&nCRCAddr,4);
					WriteHexLine(ofp,6,nCRCAddr&0xFFFF,0,crcbuf);
				}
				else if (nCRCType==1) {
					memcpy(crcbuf,&chksum32,4);
					WriteHexLine(ofp,4,nCRCAddr&0xFFFF,0,crcbuf);
				}
				else if (nCRCType==2) {
					memcpy(crcbuf,&chksum16,2);
					memcpy(crcbuf+2,&nCRCAddr,4);
					WriteHexLine(ofp,6,nCRCAddr&0xFFFF,0,crcbuf);
				}
				bDone=1;
			}
		}
		fputs(str,ofp);
	}
	if (ifp) fclose(ifp);
	if (ofp) fclose(ofp);
	printf("Done.\n");
	return 0;
}
