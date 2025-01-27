/*
Module Name:
    MiuJpegDecoder.h
Author:
    LanpurZ
Function:
    This module declare the functions that decode jpeg including YUV444, YUV422, etc.

*/

#ifndef MIU_JPEG_DECODER
#define MIU_JPEG_DECODER

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#define                 MAKE_MIU_ERROR_INFORMATION(err)             __LINE__, __FILE__, err
#define                 RGBA(r,g,b,a)                               (dword)(((byte)(r) |((word)((byte)(g)) << 8)) |(((dword)((byte)(b)) << 16)) |(((dword)((byte)(a)) << 24)))
#define                 BYTE_HIWORD(b)                              (b >> 4) & 0xFL
#define                 BYTE_LOWORD(b)                              b & 0xFL

#define                 FILE_IS_SO_BIG                              "文件过大"
#define                 FILE_OPEN_FAILED                            "文件打开失败"
#define                 FILE_NOT_JPEG                               "文件打开失败非JPEG文件"
#define                 JPEG_NOT_YUV                                "不支持除YCbCr外的格式的JPEG"
#define                 JPEG_SIDE_NOT_SUPPORT                       "图片边长不为8的倍数!"
#define                 JPEG_SAMP_NOT_SUPPORT                       "不支持该格式图片!"
#define                 JPEG_HUFFMAN_DATA_ERROR                     "Huffman解码:数据错误"
#define                 JPEG_SAMPLING_DATA_ERROR                    "解码时采样数据错误"

namespace Miu {

using       uint        =       unsigned int;
using       ulong       =       unsigned long;
using       byte        =       unsigned char;
using       word        =       unsigned short;
using       dword       =       uint;
using       pixel       =       dword;

inline word reverse(word w) { return (word)((byte)(w >> 8) | ((word)((byte)(w)) << 8)); }

void MemoryCopy(void* tagMemAddr, const void* srcMemAddr, ulong tagSize);

void MemoryZero(void* tagMemAddr, ulong tagSize);

class MiuError : public std::exception {
public:
    MiuError(int line, const char* file, const char* e) {
        strErrorInfo = "line:" + std::to_string(line) +
            "\nfile:" + file +
            "\ninfo:" + e;
    }
    ~MiuError() {}

    char const* what() const override
    {
        return strErrorInfo.c_str();
    }

private:
    std::string strErrorInfo;
};

template <typename String>
class FileReader {
public:
    FileReader() = default;
    FileReader(String path) { open(path); };
    ~FileReader() { file.close(); };

    void open(String path)
    {
        file.open(path, std::ios::binary | std::ios::in | std::ios::ate);
        if (!file.is_open())
            return;

        file_size = (ulong)file.tellg();
        file.seekg(0, std::ios::beg);
    }
    void read(void* out, ulong size)
    {
        file.read((char*)out, size);
    }
    bool eof()
    {
        return file.eof();
    }
    ulong get_cur()
    {
        return (ulong)file.tellg();
    }
    void seek_cur(ulong offset)
    {
        file.seekg(offset, std::ios::cur);
    }
    void close()
    {
        if(file.is_open())
            file.close();
    }

    ulong get_file_size() { return file_size; }
    bool is_open() { return file.is_open(); }

private:
    ulong               file_size;
    std::fstream        file;
};

class BinaryReader
{
public:
    typedef struct _binStruct {
        bool isEnd;
        bool data;

        _binStruct() : isEnd(false), data(0) {}
    }BIN_STRUCT, *LPBIN_STRUCT;

public:
    BinaryReader(ulong dwSize, byte* bData);
    ~BinaryReader();

    void get(LPBIN_STRUCT lpbs);
    ulong getpos();

private:
    ulong       dwGlobalIndex;
    ulong       dwBitIndex;
    ulong       dwSize;
    ulong       dwBitSize;
    byte*       bData;

    void init(byte* data);

};

class QuantizationTable {
public:
    byte u_qt_size;
    byte u_qt_id;
    byte u_qt_accur; // 量化表精度

    byte* content;

public:
    QuantizationTable();
    ~QuantizationTable();
};
class QuantizationTableBox {
public:
    QuantizationTableBox();
    ~QuantizationTableBox();

public:
    void push(QuantizationTable* qt);
    void clear();
    void print();

    QuantizationTable* operator[](int id) { return quantTable[hashTable[id]]; }

private:
    int                                  hashTable[128];     //最多容纳128个量化表
    std::vector<QuantizationTable*>      quantTable;

};

class Huffman_Key {
public:
    byte bit;
    uint key;
    uint value;

    Huffman_Key() : bit(0), key(0), value(0) {}
};
class HuffmanTable {
public:
    byte u_ht_id;
    byte u_ht_type;

    std::vector<Huffman_Key> real_table;

public:
    HuffmanTable();
    ~HuffmanTable() = default;

    int find(Huffman_Key& hk);
};
class HuffmanTableBox {
public:
    HuffmanTableBox() = default;
    ~HuffmanTableBox();

public:
    void push(HuffmanTable* ht);
    void clear();
    void print();
    HuffmanTable* find(byte id, byte type);

private:
    std::vector<HuffmanTable*>           huffmanTable;

private:
    void itob(uint value, int bit, char* ch);
};

class JpegDecoder {
public:
    typedef struct _jpegComp
    {
        byte id;     //组件id
        byte sc;     //采样系数 Sampling coefficient
        byte qtn;    //量化表号 quantization table number
    }COMP, *LPCOMP;
    typedef struct _jpegSOF {
        byte bPreSize;  //样本位数
        word wHeight;   //高
        word wWidth;    //宽
        byte nComp;     //组件数
        COMP* bComps;   //组件

        byte operator[](std::string id)
        {
            uint uid = 0;
            if (id == "Y")
                uid = 1;
            else if (id == "Cb")
                uid = 2;
            else if (id == "Cr")
                uid = 3;

            return find(uid)->qtn;
        }

    private:
        COMP* find(uint id)
        {
            for (uint i = 0; i < nComp; i++)
            {
                if (bComps[i].id == id)
                    return &bComps[i];
            }
            return nullptr;
        }
    }SOF, *LPSOF;
    typedef enum _tagSamplingType {
        SAMP_YUV444 = 1,
        SAMP_YUV422 = 2,
        SAMP_YUV420 = 4,
        SAMP_UNKNOWN = 0
    }SamplingType;
    typedef struct _tagCompSOS
    {
        byte id;        //组件id
        byte htid;      //哈夫曼表id
    }COMPSOS, *LPCOMPSOS;
    typedef struct _jpegSOS {
        byte nComp;         //组件数
        COMPSOS* bComps;    //组件
        byte reserve[3];    //保留

        byte operator[](std::string id)
        {
            uint uid = 0;
            if (id == "Y")
                uid = 1;
            else if (id == "Cb")
                uid = 2;
            else if (id == "Cr")
                uid = 3;

            return find(uid)->htid;
        }

    private:
        COMPSOS* find(uint id)
        {
            for (uint i = 0; i < nComp; i++)
            {
                if (bComps[i].id == id)
                    return &bComps[i];
            }
            return nullptr;
        }
    }SOS;

    const int ZigZag[64] =
    {
        0,  1,  5,  6,  14, 15, 27, 28,
        2,  4,  7,  13, 16, 26, 29, 42,
        3,  8,  12, 17, 25, 30, 41, 43,
        9,  11, 18, 24, 31, 40, 44, 53,
        10, 19, 23, 32, 39, 45, 52, 54,
        20, 22, 33, 38, 46, 51, 55, 60,
        21, 34, 37, 47, 50, 56, 59, 61,
        35, 36, 48, 49, 57, 58, 62, 63
    };

public:
    ulong debug_offset = 0;

public:
    JpegDecoder();
    JpegDecoder(const char* path) { load(path); }
    ~JpegDecoder();

    void load(const char* path);
    void clear();

    ulong width() { return sof.wWidth; }
    ulong height() { return sof.wHeight; }
    pixel* getRGB() { return rgbData; }

private:
    FileReader<const char*>         reader;

    std::vector<float>              idctTable;
    QuantizationTableBox            quantTableBox;
    HuffmanTableBox                 huffmanTableBox;
    SOF                             sof;
    SOS                             sos;
    SamplingType                    sampType;

    ulong                           compDataSize;
    byte*                           compData;
    pixel*                          rgbData;

private:
    void InitIDCTTable();
    void LoadJpegData();
    void GetRGBFromData();

private:
    bool HuffmanDecoder(
        BinaryReader& br, 
        HuffmanTable* DC_Huffman_Table, 
        HuffmanTable* AC_Huffman_Table,
        int& prevDC, 
        int* out, 
        bool& isReadEnd
    );

    void ReZigzag(int* sData, int* outputData);
    void ReQuantify(int* v, QuantizationTable& qt);
    void IDCT(int* DCTData, char* YUVData);
    pixel YUVToRGB(char Y, char U, char V);

    void GetMCU_YUV444(char* Y, char* U, char* V, pixel* mcu);
    void GetMCU_YUV420(char** Y, char* U, char* V, pixel** mcu);

    void WriteMCU(ulong x, ulong y, pixel* mcu);
    void MCUDecoder(BinaryReader& br, int& prevDC_Y, int& prevDC_Cb, int& prevDC_Cr, pixel** mcu);

};



}

#endif // !MIU_JPEG_DECODER

