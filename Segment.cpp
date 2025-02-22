//
// Created by Edge on 2020/5/11.
//

#include <iostream>
#include <Utility.h>
#include <cassert>
#include "Segment.h"
#include "Decoder.h"

using std::ifstream;
using std::cout;
using std::endl;

constexpr char APP0::MARKER_MAGIC_NUMBER[];
constexpr char APP0::IDENTIFIER_MAGIC_NUMBER[];
constexpr char COM::MARKER_MAGIC_NUMBER[];
constexpr char DQT::MARKER_MAGIC_NUMBER[];
constexpr char SOF0::MARKER_MAGIC_NUMBER[];
constexpr char DHT::MARKER_MAGIC_NUMBER[];
constexpr char DRI::MARKER_MAGIC_NUMBER[];
constexpr char SOS::MARKER_MAGIC_NUMBER[];
constexpr char JPEG::MARKER_MAGIC_NUMBER[];
constexpr char JPEG::EIO_MARKER_MAGIC_NUMBER[];
constexpr int ComponentTable::AC_ALL_ZERO;
constexpr int ComponentTable::AC_FOLLOWING_SIXTEEN_ZERO;
constexpr int ComponentTable::AC_NORMAL_STATE;
constexpr int JPEG::AC_COMPONENT;
constexpr int JPEG::DC_COMPONENT;

std::ifstream &operator>>(std::ifstream &ifs, ColorType &data) {
    ifs >> data.r;
    ifs >> data.g;
    ifs >> data.b;
    return ifs;
}

bool APP0::checkSegment(const char header[]) {
    return checkData(header, APP0::MARKER_MAGIC_NUMBER, sizeof(APP0::MARKER_MAGIC_NUMBER));
}

std::ifstream &operator>>(std::ifstream &ifs, APP0 &data) {
    uint16_t length;
    ifs >> length;
    // subtract length itself size
    length -= 2;
    char identifier[6];
    readData(ifs, identifier, sizeof(APP0::IDENTIFIER_MAGIC_NUMBER) - 1);
    if (!checkData(identifier, APP0::IDENTIFIER_MAGIC_NUMBER, sizeof(APP0::IDENTIFIER_MAGIC_NUMBER))) {
        cout << "[ERROR] APP0 identifier mismatch." << endl;
        exit(1);
    }
    // subtract identifier length
    length -= 5;
    ifs >> data.m_version;
    ifs >> data.m_densityUnit;
    ifs >> data.m_xDensity;
    ifs >> data.m_yDensity;
    ifs >> data.m_xThumbnail;
    ifs >> data.m_yThumbnail;

    // store thumbnail
    int thumbnailSize = data.m_xThumbnail * data.m_yThumbnail;
    if (thumbnailSize) {
        data.m_thumbnailData = new Color[thumbnailSize];
        for (int i = 0; i < thumbnailSize; ++i) {
            ifs >> data.m_thumbnailData[i];
        }
    }
    length -= 9 + thumbnailSize * sizeof(Color);
    assert(length == 0);

    return ifs;
}

std::ostream &operator<<(std::ostream &os, const APP0 &data) {
    os << "========= APP0 Start ========== " << std::endl;
    os << "Major Version: " << hexify((uint8_t) (data.m_version >> 8u)) << std::endl;
    os << "Minor Version: " << hexify((uint8_t) (data.m_version & 0xffu)) << std::endl;
    os << "Density Unit: " << hexify(data.m_densityUnit) << std::endl;
    os << "X Density: " << hexify(data.m_xDensity) << std::endl;
    os << "Y Density: " << hexify(data.m_yDensity) << std::endl;
    os << "X Thumbnail: " << hexify(data.m_xThumbnail) << std::endl;
    os << "Y Thumbnail: " << hexify(data.m_yThumbnail) << std::endl;
    os << "========= APP0 End ========== " << std::endl;
    return os;
}

APP0::~APP0() {
    delete[] m_thumbnailData;
}

bool COM::checkSegment(const char header[]) {
    return checkData(header, COM::MARKER_MAGIC_NUMBER, sizeof(COM::MARKER_MAGIC_NUMBER));
}

std::ifstream &operator>>(std::ifstream &ifs, COM &data) {
    uint16_t length;
    ifs >> length;
    length -= 2;
    // read in comment
    char *comment = new char[length + 1];
    ifs.read(comment, length);
    comment[length] = 0;
    data.m_comment = comment;
    delete[] comment;
    return ifs;
}

std::ostream &operator<<(std::ostream &os, const COM &data) {
    os << "========= COM Start ========== " << std::endl;
    os << "Comment: " << data.m_comment << std::endl;
    os << "========= COM End ========== " << std::endl;
    return os;
}

bool DQT::checkSegment(const char header[]) {
    return checkData(header, DQT::MARKER_MAGIC_NUMBER, sizeof(DQT::MARKER_MAGIC_NUMBER));
}

std::ifstream &operator>>(std::ifstream &ifs, DQT &data) {
    uint16_t length;
    ifs >> length;
    length -= 2;
    while (length > 0) {
        // read in precision (higher 4 bit, 0 indicate 8-bit, 1 indicate 16-bit) and id (lower 4 bit)
        uint8_t precisionAndType;
        ifs >> precisionAndType;
        data.m_PTq[precisionAndType & 0x0fu] = precisionAndType;
        // Quantization table
        data.m_qs[precisionAndType & 0x0fu] = (precisionAndType & 0xf0u)
                                              ? (void *) (new uint16_t[64])
                                              : (void *) (new uint8_t[64]);
        for (int i = 0; i < 64; ++i) {
            if ((precisionAndType & 0xf0u)) {
                ifs >> ((uint16_t *) data.m_qs[precisionAndType & 0x0fu])[i];
            } else {
                ifs >> ((uint8_t *) data.m_qs[precisionAndType & 0x0fu])[i];
            }
        }
        length -= 1 + 64 * (((precisionAndType & 0xf0u) >> 4u) + 1);
    }
    assert(length == 0);

    return ifs;
}

std::ostream &operator<<(std::ostream &os, const DQT &data) {
    os << "========= DQT Start ========== " << std::endl;
    for (int i = 0; i < 4; ++i) {
        if (data.m_qs[i]) {
            os << "Precision: " << (data.m_PTq[i] >> 4u) << std::endl;
            os << "ID: " << (data.m_PTq[i] & 0x0fu) << std::endl;
            os << "Quantization Table Content: " << std::endl;
            for (int j = 0; j < 64; ++j) {
                if ((data.m_PTq[i] & 0xf0u)) {
                    os << hexify(((uint16_t *) data.m_qs[i])[j]) << " ";
                } else {
                    os << hexify(((uint8_t *) data.m_qs[i])[j]) << " ";
                }
                if (j % 8 == 7) {
                    os << std::endl;
                }
            }
        }
    }
    os << "========= DQT End ========== " << std::endl;
    return os;
}

DQT::~DQT() {
    for (int i = 0; i < 4; ++i) {
        if (m_qs[i]) {
            if ((m_PTq[i] & 0xf0u)) {
                delete[] (uint16_t *) m_qs[i];
            } else {
                delete[] (uint8_t *) m_qs[i];
            }
        }
    }
}

std::ifstream &operator>>(std::ifstream &ifs, ColorComponent &data) {
    ifs >> data.m_id;
    // horizontal (higher 4 bit), vertical sampling factor (lower 4 bit)
    ifs >> data.m_sampleFactor;
    ifs >> data.m_dqtId;
    return ifs;
}

std::ostream &operator<<(std::ostream &os, const ColorComponent &data) {
    os << "Color Component ID: " << hexify(data.m_id) << std::endl;
    os << "Sample Factor: " << hexify(data.m_sampleFactor) << std::endl;
    os << "Corresponding DQT ID: " << hexify(data.m_dqtId) << std::endl;
    return os;
}

bool SOF0::checkSegment(const char header[]) {
    return checkData(header, SOF0::MARKER_MAGIC_NUMBER, sizeof(SOF0::MARKER_MAGIC_NUMBER));
}

std::ifstream &operator>>(std::ifstream &ifs, SOF0 &data) {
    uint16_t length;
    ifs >> length;
    length -= 2;

    ifs >> data.m_precision;
    ifs >> data.m_height;
    ifs >> data.m_width;
    ifs >> data.m_componentSize;
    data.m_maxHorizontalComponent = data.m_maxVerticalComponent = 0;
    for (int i = 0; i < (int) data.m_componentSize; ++i) {
        ColorComponent colorComponent{};
        ifs >> colorComponent;
        data.m_component[colorComponent.m_id - 1] = colorComponent;
        // select max horizontal sampling factor as sampling factor per mcu
        data.m_maxHorizontalComponent = std::max(data.m_maxHorizontalComponent,
                                                 (uint8_t) (data.m_component[i].m_sampleFactor >> 4u));
        // select max vertical sampling factor as sampling factor per mcu
        data.m_maxVerticalComponent = std::max(data.m_maxVerticalComponent,
                                               (uint8_t) (data.m_component[i].m_sampleFactor & 0x0fu));
    }
    length -= 6 + data.m_componentSize * sizeof(ColorComponent);
    assert(length == 0);
    return ifs;
}

std::ostream &operator<<(std::ostream &os, const SOF0 &data) {
    os << "========= SOF0 Start ========== " << std::endl;
    os << "Precision: " << hexify(data.m_precision) << std::endl;
    os << "Height: " << hexify(data.m_height) << std::endl;
    os << "Width: " << hexify(data.m_width) << std::endl;
    for (int i = 0; i < data.m_componentSize; ++i) {
        os << "Color Component " << i << ": " << std::endl;
        os << data.m_component[i] << std::endl;
    }
    os << "========= SOF0 End ========== " << std::endl;
    return os;
}

std::ifstream &operator>>(std::ifstream &ifs, HuffmanTable &data) {
    ifs >> data.m_typeAndId;
    // Huffman table
    data.m_length += 1;
    data.m_length += 16;
    for (int i = 1; i <= 16; ++i) {
        ifs >> data.m_codeAmountOfBit[i];
        data.m_length += data.m_codeAmountOfBit[i];
        // canonical huffman table
        // accelerate getCode() table lookup using pre-calculated start address of each codeword length
        // because this kind of huffman table move nodes of same depth to their right, so we can implement getCode() fas-
        // ter by using pre-calculated start address of each codeword length
        data.m_table[i] = ((data.m_table[i - 1] + (uint32_t) data.m_codeAmountOfBit[i - 1]) << 1u);
        if (data.m_codeAmountOfBit[i]) {
            data.m_codeword[i] = new uint8_t[data.m_codeAmountOfBit[i]];
        }
    }
    for (int i = 1; i <= 16; ++i) {
        for (int j = 0; j < data.m_codeAmountOfBit[i]; ++j) {
            ifs >> data.m_codeword[i][j];
        }
    }
    return ifs;
}

std::ostream &operator<<(std::ostream &os, const HuffmanTable &data) {
    os << "D/AC: " << ((data.m_typeAndId >> 4u) ? "AC" : "DC") << std::endl;
    os << "ID: " << (data.m_typeAndId & 0x0fu) << std::endl;
    uint16_t codeword = 0;
    for (int i = 1; i <= 16; ++i) {
        for (int j = 0; j < data.m_codeAmountOfBit[i]; ++j) {
            os << binify(codeword, i) << " " << hexify(data.m_codeword[i][j]) << std::endl;
            codeword++;
        }
        codeword <<= 1u;
    }
    return os;
}

uint8_t HuffmanTable::getType() const {
    return (m_typeAndId >> 4u);
}

uint8_t HuffmanTable::getId() const {
    return (m_typeAndId & 0x0fu);
}

int HuffmanTable::getTableLength() const {
    return m_length;
}

bool HuffmanTable::getCode(uint16_t codeword, int length, uint8_t &output) const {
    if (m_codeAmountOfBit[length] && codeword >= m_table[length] &&
        codeword - m_table[length] < m_codeAmountOfBit[length]) {
        // using pre-calculated start address of each codeword length, we can use codeword and subtract it from start add-
        // ress of that length to get the corresponding output
        output = m_codeword[length][codeword - m_table[length]];
        return true;
    }
    return false;
}

HuffmanTable::~HuffmanTable() {
    for (int i = 1; i <= 16; ++i) {
        if (m_codeAmountOfBit[i]) {
            delete[] m_codeword[i];
        }
    }
}

DHT::~DHT() {
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            if (m_huffmanTable[i][j]) {
                delete m_huffmanTable[i][j];
            }
        }
    }
}

bool DHT::checkSegment(const char header[]) {
    return checkData(header, DHT::MARKER_MAGIC_NUMBER, sizeof(DHT::MARKER_MAGIC_NUMBER));
}

std::ifstream &operator>>(std::ifstream &ifs, DHT &data) {
    uint16_t length;
    ifs >> length;
    length -= 2;

    while (length > 0) {
        HuffmanTable *newHuffmanTable = new HuffmanTable();
        ifs >> *newHuffmanTable;
        data.m_huffmanTable[newHuffmanTable->getType()][newHuffmanTable->getId()] = newHuffmanTable;
        length -= data.m_huffmanTable[newHuffmanTable->getType()][newHuffmanTable->getId()]->getTableLength();
    }
    assert(length == 0);

    return ifs;
}

std::ostream &operator<<(std::ostream &os, const DHT &data) {
    os << "========= DHT Start ========== " << std::endl;
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            if (data.m_huffmanTable[i][j]) {
                os << *data.m_huffmanTable[i][j];
            }
        }
    }
    os << "========= DHT End ========== " << std::endl;
    return os;
}

bool DRI::checkSegment(const char header[]) {
    return checkData(header, DRI::MARKER_MAGIC_NUMBER, sizeof(DRI::MARKER_MAGIC_NUMBER));
}

std::ifstream &operator>>(std::ifstream &ifs, DRI &data) {
    uint16_t length;
    ifs >> length;

    ifs >> data.m_restartInterval;
    return ifs;
}

std::ostream &operator<<(std::ostream &os, const DRI &data) {
    os << "========= DRI Start ========== " << std::endl;
    os << "Every " << hexify(data.m_restartInterval) << " number of MCU will be 1 RSTn tag" << std::endl;
    os << "========= DRI End ========== " << std::endl;
    return os;
}

std::ifstream &operator>>(std::ifstream &ifs, DHTComponent &data) {
    ifs >> data.m_id;
    ifs >> data.m_dcac;
    return ifs;
}

std::ostream &operator<<(std::ostream &os, const DHTComponent &data) {
    os << "Component ID: " << hexify(data.m_id) << std::endl;
    os << "Use DC Table id: " << (data.m_dcac >> 4u) << std::endl;
    os << "Use AC Table id: " << (data.m_dcac & 0x0fu) << std::endl;
    return os;
}

bool SOS::checkSegment(const char header[]) {
    return checkData(header, SOS::MARKER_MAGIC_NUMBER, sizeof(SOS::MARKER_MAGIC_NUMBER));
}

std::ifstream &operator>>(std::ifstream &ifs, SOS &data) {
    uint16_t length;
    ifs >> length;

    ifs >> data.m_componentSize;
    for (int i = 0; i < data.m_componentSize; ++i) {
        DHTComponent dhtComponent{};
        ifs >> dhtComponent;
        data.m_component[dhtComponent.m_id - 1] = dhtComponent;
    }
    ifs >> data.m_spectrumSelectionStart;
    ifs >> data.m_spectrumSelectionEnd;
    ifs >> data.m_spectrumSelection;
    return ifs;
}

std::ostream &operator<<(std::ostream &os, const SOS &data) {
    os << "========= SOS Start ========== " << std::endl;
    os << "Component Size: " << hexify(data.m_componentSize) << std::endl;
    for (int i = 0; i < data.m_componentSize; ++i) {
        os << "Component " << i << " of " << hexify(data.m_componentSize) << std::endl;
        os << data.m_component[i];
    }
    os << "Spectrum Selection Start: " << hexify(data.m_spectrumSelectionStart) << std::endl;
    os << "Spectrum Selection End: " << hexify(data.m_spectrumSelectionEnd) << std::endl;
    os << "Spectrum Selection: " << hexify(data.m_spectrumSelection) << std::endl;
    os << "========= SOS End ========== " << std::endl;
    return os;
}

std::ifstream &operator>>(std::ifstream &ifs, BitStreamBuffer &data) {
    if (data.m_readLength == 8) {
        ifs >> data.m_buffer;
        uint8_t dummy;
        if (data.m_buffer == 0xFF) {
            ifs >> dummy;
        }
        data.m_readLength = 0;
    }
    return ifs;
}

int BitStream::putWord(BitStreamBuffer &input, int length) {
    int bitToRead = std::min(8 - input.m_readLength, length);
    // put corresponding length of bit in input into bitstream
    // if there are some remaining bit not read from input
    // output remaining bit to read
    m_buffer <<= bitToRead;
    m_buffer |= ((input.m_buffer >> ((8 - input.m_readLength) - bitToRead)) & ((1 << bitToRead) - 1));
    m_length += bitToRead;
    input.m_readLength += bitToRead;
    return length - bitToRead;
}

void BitStream::clear() {
    m_buffer = 0;
    m_length = 0;
}

int BitStream::getLength() const {
    return m_length;
}

uint16_t BitStream::getWord() const {
    return m_buffer;
}

void ComponentTable::init(uint8_t verticalSize, uint8_t horizontalSize) {
    m_verticalSize = verticalSize;
    m_horizontalSize = horizontalSize;
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            m_table[i][j] = new float *[verticalSize];
            for (int k = 0; k < verticalSize; ++k) {
                m_table[i][j][k] = new float[horizontalSize];
            }
        }
    }
}

void ComponentTable::read(std::ifstream &ifs, float lastComponentDcValue, const HuffmanTable &dcTable,
                          const HuffmanTable &acTable, BitStreamBuffer &bsb) {

    for (int i = 0; i < m_verticalSize; ++i) {
        for (int j = 0; j < m_horizontalSize; ++j) {
            uint32_t count = 1;
            // first element of corresponding table is dc
            m_table[0][0][i][j] = readDc(ifs, dcTable, bsb);
            // if this is not in first mcu, then it must contain its previous mcu's component dc value
            m_table[0][0][i][j] += lastComponentDcValue;
            lastComponentDcValue = m_table[0][0][i][j];
            // the remaining element are ac value
            while (count < 64) {
                ComponentTable::ACValue acValue = readAc(ifs, acTable, bsb);
                switch (acValue.state) {
                    case AC_ALL_ZERO : {
                        while (count < 64) {
                            m_table[count >> 3u][count & 0x07u][i][j] = 0.0;
                            ++count;
                        }
                        break;
                    }
                    case AC_FOLLOWING_SIXTEEN_ZERO : {
                        for (int k = 0; k < 16; ++k) {
                            m_table[count >> 3u][count & 0x07u][i][j] = 0.0;
                            ++count;
                        }
                        break;
                    }
                    case AC_NORMAL_STATE : {
                        for (int k = 0; k < acValue.trailingZero; ++k) {
                            m_table[count >> 3u][count & 0x07u][i][j] = 0.0;
                            ++count;
                        }
                        m_table[count >> 3u][count & 0x07u][i][j] = acValue.value;
                        ++count;
                        break;
                    }
                }
            }
        }
    }
}

std::ostream &operator<<(std::ostream &os, const ComponentTable &data) {
    for (int i = 0; i < data.m_verticalSize; ++i) {
        for (int j = 0; j < data.m_horizontalSize; ++j) {
            os << "=========== Sample of (" << i << ", " << j << ") Start =========" << std::endl;
            for (int k = 0; k < 8; ++k) {
                for (int l = 0; l < 8; ++l) {
                    os << data.m_table[k][l][i][j] << " ";
                }
                os << std::endl;
            }
            os << "=========== Sample of (" << i << ", " << j << ") End =========" << std::endl;
        }
    }
    return os;
}

void ComponentTable::multiplyWith(const DQT &dqt, int tableIndex) {
    // multiply component table with dqt
    if ((dqt.m_PTq[tableIndex] >> 4u)) {
        const uint16_t *dqtTable = reinterpret_cast<uint16_t *>(dqt.m_qs[tableIndex]);
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 8; ++j) {
                for (int k = 0; k < m_verticalSize; ++k) {
                    for (int l = 0; l < m_horizontalSize; ++l) {
                        m_table[i][j][k][l] *= (float) dqtTable[i * 8 + j];
                    }
                }
            }
        }
    } else {
        const uint8_t *dqtTable = reinterpret_cast<uint8_t *>(dqt.m_qs[tableIndex]);
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 8; ++j) {
                for (int k = 0; k < m_verticalSize; ++k) {
                    for (int l = 0; l < m_horizontalSize; ++l) {
                        m_table[i][j][k][l] *= (float) dqtTable[i * 8 + j];
                    }
                }
            }
        }
    }
}

void ComponentTable::replaceWith(const ComponentTable &table, int (*replaceTable)[8]) {
    // replace with another component table using replace table
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            for (int k = 0; k < m_verticalSize; ++k) {
                for (int l = 0; l < m_horizontalSize; ++l) {
                    uint8_t replacePosition = replaceTable[i][j];
                    m_table[i][j][k][l] = table.m_table[replacePosition >> 3u][replacePosition & 0x7u][k][l];
                }
            }
        }
    }
}

void ComponentTable::inPlaceReplaceWith(int (*swapTable)[8]) {
    // in place swap using swap table
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            uint8_t swapPosition = swapTable[i][j];
            for (int k = 0; k < m_verticalSize; ++k) {
                for (int l = 0; l < m_horizontalSize; ++l) {
                    float value = m_table[i][j][k][l];
                    m_table[i][j][k][l] = m_table[swapPosition >> 3u][swapPosition & 0x7u][k][l];
                    m_table[swapPosition >> 3u][swapPosition & 0x7u][k][l] = value;
                }
            }
        }
    }
}

float ComponentTable::convertToCorrectCoefficient(uint16_t rawCoefficient, int length) {
    float result;
    // like 1's complement, if first bit is 0, then return its negative
    // else return input
    if ((rawCoefficient >> (length - 1)) == 0) {
        rawCoefficient ^= ((1 << length) - 1);
        result = rawCoefficient;
        result = -result;
        return result;
    }
    result = rawCoefficient;
    return result;
}

float ComponentTable::readDc(std::ifstream &ifs, const HuffmanTable &dcTable, BitStreamBuffer &bsb) {
    ifs >> bsb;
    BitStream bs;
    bs.putWord(bsb, 1);
    uint8_t output;
    // Decode n from huffman table
    // read 1 bit per time until it match corresponding dc huffman table
    while (!dcTable.getCode(bs.getWord(), bs.getLength(), output)) {
        if (bs.putWord(bsb, 1)) {
            ifs >> bsb;
        }
    }
    bs.clear();
    int readLength = output;
    // read following length of decoded word
    while ((output = bs.putWord(bsb, output))) {
        ifs >> bsb;
    }
    uint16_t rawCoefficient = bs.getWord();
    bs.clear();
    return convertToCorrectCoefficient(rawCoefficient, readLength);
}

ComponentTable::ACValue ComponentTable::readAc(std::ifstream &ifs, const HuffmanTable &acTable, BitStreamBuffer &bsb) {
    ifs >> bsb;
    BitStream bs;
    bs.putWord(bsb, 1);
    uint8_t output;
    // Decode n from huffman table
    // read 1 bit per time until it match corresponding dc huffman table
    while (!acTable.getCode(bs.getWord(), bs.getLength(), output)) {
        if (bs.putWord(bsb, 1)) {
            ifs >> bsb;
        }
    }
    ComponentTable::ACValue acValue{};
    // if decoded word is 0x00, then it indicated following elements of component table is 0
    if (output == 0x00) {
        acValue.state = AC_ALL_ZERO;
    // else if decoded word is 0xF0, then it indicated following 16 elements of component table is 0
    } else if (output == 0xF0) {
        acValue.state = AC_FOLLOWING_SIXTEEN_ZERO;
    } else {
        // decoded higher 4 bit indicate following n elements of component table is 0, lower 4 bit indicate bit length to read
        acValue.state = AC_NORMAL_STATE;
        acValue.trailingZero = (output >> 4u);
        output = (output & 0x0Fu);
        int readLength = output;
        bs.clear();
        // read following length of decoded word
        while ((output = bs.putWord(bsb, output))) {
            ifs >> bsb;
        }
        uint16_t rawCoefficient = bs.getWord();
        bs.clear();
        acValue.value = convertToCorrectCoefficient(rawCoefficient, readLength);
    }
    return acValue;
}

ComponentTable::~ComponentTable() {
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            for (int k = 0; k < m_verticalSize; ++k) {
                delete[] m_table[i][j][k];
            }
            delete[] m_table[i][j];
        }
    }
}

void MCU::read(std::ifstream &ifs, const JPEG &jpeg, BitStreamBuffer &bsb) {
    for (int i = 0; i < jpeg.m_sof0.m_componentSize; ++i) {
        // higher 4 bit is the dc table use to decode this component's, lower 4 bit is the ac table use to decode this component's
        const HuffmanTable *dc = jpeg.m_dht.m_huffmanTable[JPEG::DC_COMPONENT][jpeg.m_sos.m_component[i].m_dcac >> 4u];
        const HuffmanTable *ac = jpeg.m_dht.m_huffmanTable[JPEG::AC_COMPONENT][jpeg.m_sos.m_component[i].m_dcac &
                                                                               0x0fu];
        m_component[i] = new ComponentTable[jpeg.m_sof0.m_componentSize];
        // higher 4 bit is this component's horizontal sample factor, lower 4 bit is this component's vertical sample factor
        int verticalSize = static_cast<int>(jpeg.m_sof0.m_component[i].m_sampleFactor & 0x0fu);
        int horizontalSize = (jpeg.m_sof0.m_component[i].m_sampleFactor >> 4u);
        m_component[i]->init(verticalSize, horizontalSize);
        if (jpeg.m_mcus.m_lastMcu) {
            m_component[i]->read(ifs,
                                 jpeg.m_mcus.m_lastMcu->m_component[i]->m_table[0][0][verticalSize - 1][horizontalSize -
                                                                                                        1], *dc, *ac,
                                 bsb);

        } else {
            m_component[i]->read(ifs, 0, *dc, *ac, bsb);
        }
    }
}

std::ostream &operator<<(std::ostream &os, const MCU &data) {
    for (int i = 0; i < 3; ++i) {
        os << "=========== Component " << i << " Start =========" << std::endl;
        os << *data.m_component[i];
        os << "=========== Component " << i << " End =========" << std::endl;
    }
    return os;
}

void MCUS::read(std::ifstream &ifs, const JPEG &jpeg) {
    // Calculate how many mcu in row and column
    m_mcuWidth = (jpeg.m_sof0.m_width - 1) / (8 * jpeg.m_sof0.m_maxHorizontalComponent) + 1;
    m_mcuHeight = (jpeg.m_sof0.m_height - 1) / (8 * jpeg.m_sof0.m_maxVerticalComponent) + 1;
    m_mcu = new MCU *[m_mcuHeight];
    BitStreamBuffer bsb;
    // read each mcu
    for (int i = 0; i < m_mcuHeight; ++i) {
        m_mcu[i] = new MCU[m_mcuWidth];
        for (int j = 0; j < m_mcuWidth; ++j) {
            m_mcu[i][j].read(ifs, jpeg, bsb);
            m_lastMcu = &m_mcu[i][j];
        }
    }
}

std::ostream &operator<<(std::ostream &os, const MCUS &data) {
    for (int i = 0; i < data.m_mcuHeight; ++i) {
        for (int j = 0; j < data.m_mcuWidth; ++j) {
            os << "========= MCU (" << i << ", " << j << ") Start ========== " << std::endl;
            os << data.m_mcu[i][j];
            os << "========= MCU (" << i << ", " << j << ") End ========== " << std::endl;
        }
    }
    return os;
}

std::ifstream &operator>>(std::ifstream &ifs, JPEG &data) {
    char header[3] = {};
    readData(ifs, header, 2);
    cout << "[INFO] Header " << hexify(header, 2) << "." << endl;
    if (!checkData(header, JPEG::MARKER_MAGIC_NUMBER, sizeof(JPEG::MARKER_MAGIC_NUMBER))) {
        cout << "[ERROR] JPEG marker mismatch." << endl;
        exit(1);
    }
    readData(ifs, header, 2);
    do {
        cout << "[INFO] Header " << hexify(header, 2) << "." << endl;
        if (SOS::checkSegment(header)) {
            ifs >> data.m_sos;
#ifdef DEBUG
            std::cout << data.m_sos;
#endif
            break;
        } else if (DRI::checkSegment(header)) {
            ifs >> data.m_dri;
#ifdef DEBUG
            std::cout << data.m_dri;
#endif
        } else if (DHT::checkSegment(header)) {
            ifs >> data.m_dht;
#ifdef DEBUG
            std::cout
                    << data.m_dht;
#endif
        } else if (SOF0::checkSegment(header)) {
            ifs >> data.m_sof0;
#ifdef DEBUG
            std::cout << data.m_sof0;
#endif
        } else if (DQT::checkSegment(header)) {
            ifs >> data.m_dqt;
#ifdef DEBUG
            std::cout << data.m_dqt;
#endif
        } else if (APP0::checkSegment(header)) {
            ifs >> data.m_app0;
#ifdef DEBUG
            std::cout << data.m_app0;
#endif
        } else if (COM::checkSegment(header)) {
            ifs >> data.m_com;
#ifdef DEBUG
            std::cout << data.m_com;
#endif
        } else {
            cout << "[ERROR] Unable to recognize header " << hexify(header, 2) << "." << endl;
            exit(1);
        }
        readData(ifs, header, 2);
    } while (true);

    data.m_mcus.read(ifs, data);
    readData(ifs, header, 2);
    if (!checkData(header, JPEG::EIO_MARKER_MAGIC_NUMBER, sizeof(JPEG::EIO_MARKER_MAGIC_NUMBER))) {
        cout << "[ERROR] Unable to recognize header " << hexify(header, 2) << "." << endl;
        exit(1);
    } else {
        cout << "[INFO] Successfully parse the file." << endl;
    }
    return ifs;
}

std::ostream &operator<<(std::ostream &os, const JPEG &data) {
    os << data.m_app0;
    os << data.m_dqt;
    os << data.m_sof0;
    os << data.m_dht;
    os << data.m_dri;
    os << data.m_sos;
    return os;
}

JPEG::~JPEG() {
    delete m_image;
}