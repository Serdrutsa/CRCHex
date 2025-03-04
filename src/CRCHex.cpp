#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <getopt.h>

//------------------------------------------------------------------------------
// Constants and macros
#define APP_VERSION     "2.0.0"

//------------------------------------------------------------------------------
// Static functions declarations
static uint8_t  calculateCRC8 (const std::vector<uint8_t> data, uint32_t startAddr, uint32_t endAddr, std::pair<uint32_t, size_t> skip);
static uint16_t calculateCRC16(const std::vector<uint8_t> data, uint32_t startAddr, uint32_t endAddr, std::pair<uint32_t, size_t> skip);
static uint32_t calculateCRC32(const std::vector<uint8_t> data, uint32_t startAddr, uint32_t endAddr, std::pair<uint32_t, size_t> skip);

static void print_help(const char* progname);

//------------------------------------------------------------------------------
// HexProcessor and Builder classes
class HexProcessor
{
public:
    class Builder;

    bool process();

private:
    HexProcessor(const std::string &inputFile, const std::string &outputFile, int crcT, uint32_t startAddr, uint32_t endAddr, uint32_t crcAddr) :
        inputFileName(inputFile), outputFileName(outputFile), crcType(crcT),
        startAddress(startAddr), endAddress(endAddr), crcAddress(crcAddr) {}

    std::string inputFileName;
    std::string outputFileName;
    int crcType;
    uint32_t startAddress;
    uint32_t endAddress;
    uint32_t crcAddress;
    std::vector<uint8_t> romBuffer;

    void putHexByte(std::ofstream &out, uint8_t value);
    void writeHexLine(std::ofstream &out, uint16_t addr, int type, const std::vector<uint8_t> &data);
};

class HexProcessor::Builder
{
public:
    Builder& setInputFileName(const std::string &inputFile) {
        inputFileName = inputFile;
        return *this;
    }

    Builder& setOutputFileName(const std::string &outputFile) {
        outputFileName = outputFile;
        return *this;
    }

    Builder& setCrcType(int crcT) {
        if (crcT == 1 || crcT == 2 || crcT == 4) {
            crcType = crcT;
        }
        return *this;
    }

    Builder& setStartAddress(uint32_t startAddr) {
        startAddress = startAddr;
        return *this;
    }

    Builder& setEndAddress(uint32_t endAddr) {
        endAddress = endAddr;
        return *this;
    }

    Builder& setCrcAddress(uint32_t crcAddr) {
        crcAddress = crcAddr;
        return *this;
    }

    HexProcessor create() const {
        return HexProcessor(inputFileName, outputFileName, crcType, startAddress, endAddress, crcAddress);
    }

private:
    std::string inputFileName;
    std::string outputFileName;
    int      crcType = 4;
    uint32_t startAddress = 0xFFFFFFFF;
    uint32_t endAddress = 0;
    uint32_t crcAddress = 0xFFFFFFFF;
};

void HexProcessor::putHexByte(std::ofstream &out, uint8_t value)
{
    out << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)value;
}

void HexProcessor::writeHexLine(std::ofstream &out, uint16_t addr, int type, const std::vector<uint8_t> &data)
{
    uint8_t chksum = data.size() + (addr >> 8) + (addr & 0xFF) + type;
    out << ':';
    putHexByte(out, data.size());
    putHexByte(out, addr >> 8);
    putHexByte(out, addr & 0xFF);
    putHexByte(out, type);
    for (uint8_t byte : data) {
        putHexByte(out, byte);
        chksum += byte;
    }
    putHexByte(out, -chksum);
    out << '\n';
}

bool HexProcessor::process()
{
    std::ifstream inputFile(inputFileName);
    if (!inputFile) {
        std::cerr << "Error opening input file!" << std::endl;
        return false;
    }

    std::cout << "Processing file: " << inputFileName << std::endl;
    uint16_t nSegment = 0;
    uint16_t nExtSegment = 0;
    uint32_t hexLastAddr = 0;
    std::string line;
    while (std::getline(inputFile, line)) {
        if (line.empty() || line[0] != ':') {
            continue;
        }
        uint8_t size = std::stoi(line.substr(1, 2), nullptr, 16);
        uint8_t chks = 0;
        for (int i = 0; i < size+5; ++i) {
            chks += std::stoi(line.substr(1 + i * 2, 2), nullptr, 16);
        }
        if (chks != 0) {
            std::cerr << "Error: checksum error in HEX file!" << std::endl;
            return false;
        }
        uint32_t dataAddress  = 0;
        uint16_t addr = std::stoi(line.substr(3, 4), nullptr, 16);
        int type = std::stoi(line.substr(7, 2), nullptr, 16);
        switch (type) {
        case 0:
            dataAddress = (nSegment << 16) + (nExtSegment << 4) + addr;
            if (startAddress == 0xFFFFFFFF) {
                startAddress = dataAddress;
            }
            for (int i = 0; i < size; ++i) {
                uint32_t offs = dataAddress + i;
                if (romBuffer.size() < offs + 1) {
                    romBuffer.resize(offs + 1, 0xFF);
                }
                romBuffer[offs] = std::stoi(line.substr(9 + i * 2, 2), nullptr, 16);
            }

            if (crcAddress >= dataAddress && crcAddress <= dataAddress + crcType) {
                std::cerr << "Error: CRC address in use!" << std::endl;
                return false;
            }

            if (hexLastAddr < dataAddress + size) {
                hexLastAddr = dataAddress + size;
            }
            break;
        case 1:
            if (endAddress == 0) {
                endAddress = hexLastAddr - 1;
            }
            break;
        case 2:
            nExtSegment = std::stoul(line.substr(9, 4), nullptr, 16);
            break;
        case 4:
            nSegment = std::stoul(line.substr(9, 4), nullptr, 16);
            break;
        }
    }

    if (crcAddress == 0xFFFFFFFF) {
        crcAddress = hexLastAddr;
    }

    std::cout << "Addresses: start: 0x" << std::hex << startAddress << ", end: 0x" << endAddress << ", CRC: 0x" << crcAddress << std::endl;
    std::vector<uint8_t> crc;
    switch (crcType) {
    case 1:
        {
        uint8_t crc8 = calculateCRC8(romBuffer, startAddress, endAddress, {crcAddress, crcType});
        std::cout << "CRC8: " << crc8 << std::endl;
        crc.push_back(crc8);
        }
        break;
    case 2:
        {
        uint16_t crc16 = calculateCRC16(romBuffer, startAddress, endAddress, {crcAddress, crcType});
        std::cout << "CRC16: " << crc16 << std::endl;
        crc.push_back(static_cast<uint8_t>(crc16 & 0xFF)); // Low byte
        crc.push_back(static_cast<uint8_t>(crc16 >> 8));   // High byte
        }
        break;
    case 4:
        {
        uint32_t crc32 = calculateCRC32(romBuffer, startAddress, endAddress, {crcAddress, crcType});
        std::cout << "CRC32: " << crc32 << std::endl;
        crc.push_back(static_cast<uint8_t>(crc32 & 0xFF)); // Lowest byte
        crc.push_back(static_cast<uint8_t>(crc32 >> 8));   // Second byte
        crc.push_back(static_cast<uint8_t>(crc32 >> 16));  // Third byte
        crc.push_back(static_cast<uint8_t>(crc32 >> 24));  // Highest byte
        }
        break;
    }

    if (outputFileName.empty()) {
        return true;    // no output file specified
    }

    std::ofstream outputFile(outputFileName);
    if (!outputFile) {
        std::cerr << "Error creating output file!" << std::endl;
        return false;
    }
    std::cout << "Writing output file: " << outputFileName << std::endl;

    inputFile.clear();
    inputFile.seekg(0, std::ios::beg);  // rewind input file
    nSegment = 0;
    nExtSegment = 0;
    bool crcWritten = false;

    while (std::getline(inputFile, line)) {
        if (line.empty() || line[0] != ':') {
            continue;
        }
        uint16_t addr = std::stoi(line.substr(3, 4), nullptr, 16);
        int type = std::stoi(line.substr(7, 2), nullptr, 16);

        switch (type) {
        case 1:
            if (!crcWritten) {
                writeHexLine(outputFile, crcAddress & 0xFFFF, 0, crc);
                crcWritten =  true;
            }
            break;
        case 2:
            nExtSegment = std::stoul(line.substr(9, 4), nullptr, 16);
            break;
        case 4:
            nSegment = std::stoul(line.substr(9, 4), nullptr, 16);
            break;
        }

        uint32_t dataAddress = (nSegment << 16) + (nExtSegment << 4) + addr;
        if (!crcWritten && dataAddress > crcAddress) {
            writeHexLine(outputFile, crcAddress & 0xFFFF, 0, crc);
            crcWritten =  true;
        }

        outputFile << line << '\n'; // write original line
    }

    std::cout << "Done." << std::endl;
    return true;
}

//------------------------------------------------------------------------------
// Static functions implementations

static uint8_t calculateCRC8(const std::vector<uint8_t> data, uint32_t startAddr, uint32_t endAddr, std::pair<uint32_t, size_t> skip)
{
    uint32_t skipAddr = skip.first;
    size_t   skipLen = skip.second;
    uint8_t  crc = 0xFF;

    std::cout << "Calculating CRC8 for " << data.size() << " bytes" << std::endl;

    for (uint32_t addr = 0; addr < data.size(); ++addr) {
        if (addr < startAddr || addr > endAddr) continue;
        if (addr >= skipAddr && addr < skipAddr + skipLen) continue;
        crc ^= data[addr];
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : crc << 1;
        }
    }
    return crc;
}

static uint16_t calculateCRC16(const std::vector<uint8_t> data, uint32_t startAddr, uint32_t endAddr, std::pair<uint32_t, size_t> skip)
{
    uint32_t skipAddr = skip.first;
    size_t   skipLen = skip.second;
    uint16_t crc = 0;

    std::cout << "Calculating CRC16 for " << data.size() << " bytes" << std::endl;

    for (uint32_t addr = 0; addr < data.size(); ++addr) {
        if (addr < startAddr || addr > endAddr) continue;
        if (addr >= skipAddr && addr < skipAddr + skipLen) continue;
        uint16_t c = (data[addr] ^ crc) & 0xFF;
        for (int i = 0; i < 8; ++i) {
            c = (c & 1) ? (c >> 1) ^ 0xA001 : c >> 1;
        }
        crc = (crc >> 8) ^ c;
    }
    return crc;
}

static uint32_t calculateCRC32(const std::vector<uint8_t> data, uint32_t startAddr, uint32_t endAddr, std::pair<uint32_t, size_t> skip)
{
    uint32_t skipAddr = skip.first;
    size_t   skipLen = skip.second;
    uint32_t polynomial = 0x04C11DB7;
    uint32_t crc = 0xFFFFFFFF;

    std::cout << "Calculating CRC32 for " << data.size() << " bytes" << std::endl;

    for (uint32_t addr = 0; addr < data.size(); ++addr) {
        if (addr < startAddr || addr > endAddr) continue;
        if (addr >= skipAddr && addr < skipAddr + skipLen) continue;
        crc ^= (data[addr] << 24);
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 0x80000000) ? (crc << 1) ^ polynomial : crc << 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

static void print_help(const char* progname)
{
    std::cout << std::endl << "Usage: " << progname << " -i <input.hex> [OPTIONS]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -o <file_name>    - output file name (if not specified the CRC is printed to stdout)" << std::endl;
    std::cout << "  -c <CRC type>     - 1=CRC8, 2=CRC16, 4=CRC32 (default: 4)" << std::endl;
    std::cout << "  -s <startAddress> - address to start CRC calculation (default: start of HEX)" << std::endl;
    std::cout << "  -e <endAddress>   - address to end CRC calculation (default: end of HEX)" << std::endl;
    std::cout << "  -a <crcAddress>   - address to put CRC (default: end of HEX)" << std::endl;
    std::cout << "Example: " << progname << " -i in.hex -o out.hex -c 1 -s 0x5C00 -e 0x45BFF -a 0xFF7C" << std::endl;
}

//------------------------------------------------------------------------------
// Main function
int main(int argc, char *argv[])
{
    std::cout << "Add CRC to HEX file v." << APP_VERSION << std::endl;
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    HexProcessor::Builder builder;
    int opt;

    while ((opt = getopt(argc, argv, "i:o:c:s:e:a:")) != -1) {
        switch (opt) {
            case 'i':
                builder.setInputFileName(optarg);
                break;
            case 'o':
                builder.setOutputFileName(optarg);
                break;
            case 'c':
                builder.setCrcType(std::stoi(optarg));
                break;
            case 's':
                (std::string(optarg).substr(0, 2) == "0x") ?
                    builder.setStartAddress(std::stoul(optarg, nullptr, 16)) :
                    builder.setStartAddress(std::stoul(optarg));
                break;
            case 'e':
                (std::string(optarg).substr(0, 2) == "0x") ?
                    builder.setEndAddress(std::stoul(optarg, nullptr, 16)) :
                    builder.setEndAddress(std::stoul(optarg));
                break;
            case 'a':
                (std::string(optarg).substr(0, 2) == "0x") ?
                    builder.setCrcAddress(std::stoul(optarg, nullptr, 16)) :
                    builder.setCrcAddress(std::stoul(optarg));
                break;
        }
    }

    HexProcessor processor = builder.create();
    if (!processor.process()) {
        return 1;
    }

    return 0;
}
