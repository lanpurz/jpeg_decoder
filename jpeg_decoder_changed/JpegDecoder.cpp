
#include "MiuJpegDecoder.h"

// #define _OUTPUT_

#include <Windows.h>
#include <stdio.h>
void SetConsoleCursorX(int X)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

    COORD coo1;
    coo1.X = X;
    coo1.Y = csbi.dwCursorPosition.Y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coo1);
}
void SetConsoleCursorY(int Y)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

    COORD coo1;
    coo1.X = csbi.dwCursorPosition.X;
    coo1.Y = Y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coo1);
}
void SetConsoleCursorYOffset(int offset)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

    COORD coo1;
    coo1.X = csbi.dwCursorPosition.X;
    coo1.Y = csbi.dwCursorPosition.Y + offset;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coo1);
}

Miu::JpegDecoder::JpegDecoder()
{
}

Miu::JpegDecoder::~JpegDecoder()
{
    clear();
}

void Miu::JpegDecoder::load(const char * path)
{
    reader.open(path);
    if (!reader.is_open())
    {
        throw MiuError(MAKE_MIU_ERROR_INFORMATION(FILE_OPEN_FAILED));
    }

    word type = 0;
    reader.read(&type, 2);

    if (type != 0xd8ff)
    {
        throw MiuError(MAKE_MIU_ERROR_INFORMATION(FILE_NOT_JPEG));
    }

    InitIDCTTable();
    LoadJpegData();
    GetRGBFromData();
}

void Miu::JpegDecoder::clear()
{
    if (sof.bComps != nullptr)
    {
        delete[] sof.bComps;
        sof.bComps = nullptr;
    }

    if (sos.bComps != nullptr)
    {
        delete[] sos.bComps;
        sof.bComps = nullptr;
    }

    if (compData != nullptr)
    {
        delete[] compData;
        compData = nullptr;
    }

    if (rgbData != nullptr)
    {
        delete[] rgbData;
        rgbData = nullptr;
    }

    sof = { 0 };
    sos = { 0 };
    compDataSize = 0;
    sampType = SAMP_UNKNOWN;
    reader.close();
}

void Miu::JpegDecoder::InitIDCTTable()
{
    int IDCTIndex = 0;
    float pi = 3.14f;
    int npos = 0;

    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            float result = .0f;
            for (int u = 0; u < 8; u++)
            {
                for (int v = 0; v < 8; v++)
                {
                    float temp = .0f;
                    float Cu = (float)(u == 0 ? (sqrt(2) / 4) : 0.5f), Cv = (float)(v == 0 ? (sqrt(2) / 4) : 0.5f);
                    idctTable.push_back(Cu * Cv * (float)(cos((i + 0.5) * pi * u / 8.0f) * cos((j + 0.5) * pi * v / 8.0f)));
                }
            }
        }
    }
}

void Miu::JpegDecoder::LoadJpegData()
{
    word type = 0, size = 0;        // 2位

    for (;;)
    {
        reader.read((char*)&type, 2);      // 读取头类型
        reader.read((char*)&size, 2);      // 读取块大小(字节反掉的)
        size = reverse(size);               // 翻转回来

        // 判断文件是否读取完
        if (reader.eof())
            break;

        // 接下来判断每个块的类型并作出相应的处理

        if (type == 0xc0ff) // SOF
        {
            reader.read((char*)&sof.bPreSize, 1);
            //读取宽高
            reader.read((char*)&sof.wHeight, 2);
            sof.wHeight = reverse(sof.wHeight);
            reader.read((char*)&sof.wWidth, 2);
            sof.wWidth = reverse(sof.wWidth);

            reader.read((char*)&sof.nComp, 1);

            // 建立组件表
            sof.bComps = new COMP[sof.nComp];
            reader.read((char*)sof.bComps, sof.nComp * sizeof(COMP));

            // 检查颜色格式
            if (sof.nComp != 3 || sof.bPreSize != 8)
            {
                throw MiuError(MAKE_MIU_ERROR_INFORMATION(JPEG_NOT_YUV));
                return;
            }

            char* yuv_type_info = new char[3], *buf = yuv_type_info;
            for (byte i = 0; i < sof.nComp; i++)
            {
                *buf++ = sof.bComps[i].sc;
            }

            if (yuv_type_info[0] == 0x11 &&
                yuv_type_info[1] == 0x11 &&
                yuv_type_info[2] == 0x11
                )
                sampType = SAMP_YUV444;
            else if (yuv_type_info[0] == 0x22 &&
                yuv_type_info[1] == 0x11 &&
                yuv_type_info[2] == 0x11)
                sampType = SAMP_YUV420;
            else if (yuv_type_info[0] == 0x21 &&
                yuv_type_info[1] == 0x11 &&
                yuv_type_info[2] == 0x11)
                sampType = SAMP_YUV422;
            else
                sampType = SAMP_UNKNOWN;

            delete[] yuv_type_info;
        }
        else if (type == 0xdaff) // SOS
        {
            reader.read((char*)&sos.nComp, 1);
            sos.bComps = new COMPSOS[sos.nComp];
            reader.read((char*)sos.bComps, sos.nComp * sizeof(COMPSOS));

            reader.read((char*)sos.reserve, 3);

            ulong comp_data_size = reader.get_file_size() - reader.get_cur() - 2;

            //在数据中若一个字节数值为0xff,则后面就会填充一个字节,值为0x0,读取时需要跳过这个字节
            byte* data = nullptr;
            ulong nFFNum = 0;

            data = new byte[comp_data_size];
            reader.read((char*)data, comp_data_size);

            //获取实际大小
            for (ulong u_data_index = 0; u_data_index < comp_data_size; u_data_index++)
            {
                if (data[u_data_index] == 0xFFL)
                    nFFNum++;
            }
            compData = new byte[comp_data_size - nFFNum];

            for (ulong u_data_index = 0; u_data_index < comp_data_size; u_data_index++)
            {
                compData[compDataSize++] = data[u_data_index];
                if (data[u_data_index] == 0xFF)  //若该字节为0xff
                    u_data_index++;       //则跳过下一个值为0x0的字节
            }

            delete[] data;
        }
        else if (type == 0xdbff) // DQT   量化表
        {
            ulong u_dqt_size = (size - 2) / 65;
            for (ulong u_dqt_index = 0; u_dqt_index < u_dqt_size; u_dqt_index++)
            {
                QuantizationTable* quant_table = new QuantizationTable;

                byte qti = 0;
                reader.read((char*)&qti, 1);

                quant_table->u_qt_size = 64;
                quant_table->u_qt_id = (qti & 0xF);
                quant_table->u_qt_accur = ((qti >> 4) & 0xF);

                quant_table->content = new byte[64];
                reader.read((char*)quant_table->content, 64);

                quantTableBox.push(quant_table);
            }
        }
        else if (type == 0xc4ff) // DHT  哈夫曼表
        {
            HuffmanTable* huffman_table = new HuffmanTable;

            byte hti = 0;
            reader.read((char*)&hti, 1);

            huffman_table->u_ht_id = (hti & 0xF);               // ht表号
            huffman_table->u_ht_type = ((hti >> 4) & 0xF);      // ht表类型(AC或DC)

            byte chLength[16] = { 0 }, *chHTV = 0, bHuffmanType = 0;
            reader.read((char*)chLength, 16);
            int HTVLength = 0;
            for (int index = 0; index < 16; index++)    //求和算HTV大小
                HTVLength += (int)(chLength[index]);

            chHTV = new byte[HTVLength];
            reader.read((char*)chHTV, HTVLength);

            //建表
            for (int nLengthIndex = 0, nValueIndex = 0, value = 0; nLengthIndex < 16; nLengthIndex++, value <<= 1)
            {
                for (ulong nIndex = 0; nIndex < chLength[nLengthIndex]; nIndex++, nValueIndex++, value++)
                {
                    Huffman_Key hk;
                    hk.bit = nLengthIndex + 1;
                    hk.key = value;
                    hk.value = chHTV[nValueIndex];

                    huffman_table->real_table.push_back(hk);
                }
            }

            huffmanTableBox.push(huffman_table);

            delete[] chHTV;
        }
        // else if (type == 0xe0ff) // APP0 暂时不需要考虑
        else if (type == 0xd9ff)    // EOI 表示数据读取完成
        {
            break;
        }
        else        // 如果判断不出来就跳过
        {
            reader.seek_cur(size - 2);
        }

    }

    // quantTableBox.print();
    // huffmanTableBox.print();

    // 关闭文件
    reader.close();
}

void Miu::JpegDecoder::GetRGBFromData()
{
    if (sof.wWidth % 8 != 0 || sof.wHeight % 8 != 0)
    {
        throw MiuError(MAKE_MIU_ERROR_INFORMATION(JPEG_SIDE_NOT_SUPPORT));
        return;
    }
    else if (sampType == SAMP_UNKNOWN || sampType == SAMP_YUV422)
    {
        throw MiuError(MAKE_MIU_ERROR_INFORMATION(JPEG_SAMP_NOT_SUPPORT));
        return;
    }

    BinaryReader br(compDataSize, compData);

    pixel** mcu = new pixel*[sampType];
    for (int i = 0; i < sampType; i++)
    {
        mcu[i] = new pixel[64];
        MemoryZero(mcu[i], 64 * sizeof(pixel));
    }
    
    rgbData = new pixel[sof.wWidth * sof.wHeight];
    MemoryZero(rgbData, sof.wWidth * sof.wHeight * sizeof(pixel));
    // memset(rgbData, 0xff, sof.wWidth * sof.wHeight * sizeof(pixel));
    
    ulong mcuW = sof.wWidth / 8, mcuH = sof.wHeight / 8, mcuNum = mcuW * mcuH;


    ulong mcuIndex = 0, umx = 0, umy = 0;
    int prevDC_Y = 0, prevDC_Cb = 0, prevDC_Cr = 0;
    int um_offset[5] = { 0, 1, 0, 0, 2 };
    for (; mcuIndex < mcuNum; mcuIndex += sampType)
    {
        MCUDecoder(br, prevDC_Y, prevDC_Cb, prevDC_Cr, mcu);

        WriteMCU(umx, umy, mcu[0]);

        if (sampType == SAMP_YUV420)
        {
            WriteMCU(umx + 1, umy, mcu[1]);
            WriteMCU(umx, umy + 1, mcu[2]);
            WriteMCU(umx + 1, umy + 1, mcu[3]);
        }

        umx += um_offset[sampType];
        if (umx >= mcuW)
        {
            umy += um_offset[sampType];
            umx = 0;
        }
        if (umy >= mcuH)
        {
            mcuIndex = mcuNum;
            break;
        }
#ifdef _OUTPUT_
        std::cerr << "MCU:" << mcuIndex << "\\" << mcuNum << "\noffset:" << debug_offset << "\n";
        std::cerr << "MCU Index:" << umy << "," << umx << "\n";

        SetConsoleCursorX(0);
        SetConsoleCursorYOffset(-3);
#endif
    }
#ifdef _OUTPUT_
    std::cerr << "MCU:" << mcuIndex << "\\" << mcuNum << "\noffset:" << debug_offset << "\n";
    std::cerr << "MCU Index:" << umy << "," << umx - um_offset[sampType] << "\n";
    SetConsoleCursorX(0);
    SetConsoleCursorYOffset(-3);
#endif

    for (int i = 0; i < sampType; i++)
        delete[] mcu[i];
    delete[] mcu;
}

bool Miu::JpegDecoder::HuffmanDecoder(
    BinaryReader & br, 
    HuffmanTable * DC_Huffman_Table, 
    HuffmanTable * AC_Huffman_Table, 
    int& prevDC, 
    int* out, 
    bool& isReadEnd)
{
    using namespace std;
    if (!DC_Huffman_Table || !AC_Huffman_Table || !out)
        return false;

    Miu::BinaryReader::BIN_STRUCT bin;
    //string binstr = "";
    uint outIndex = 0;
    bool state = true;
    Huffman_Key hkey;

    //decoding dc
    for (; state && (outIndex < 64); hkey.key <<= 1)
    {
        br.get(&bin);
        if (bin.isEnd)
        {
            out[outIndex++] = prevDC;
            break;
        }
        //assert(bin.isEnd, "数据不完整");

        hkey.key |= (byte)bin.data; hkey.bit++;

        //itob((uint)hkey.key, hkey.bit, binstr);

        //assert((hkey.bit > 16), "数据错误");
        if (hkey.bit > 16)
        {
            throw MiuError(MAKE_MIU_ERROR_INFORMATION(JPEG_HUFFMAN_DATA_ERROR));
            return false;
        }

        //查找匹配项
        int ans = DC_Huffman_Table->find(hkey);
        if (ans == -1)
            continue;
        else if (ans == 0)
        {
            out[outIndex++] = prevDC;
            break;
        }

        uint nextReadBit = (uint)ans;
        dword readbuf = 0;
        for (uint index = 0; index < nextReadBit; index++, readbuf <<= 1)
        {
            br.get(&bin);
            if (bin.isEnd)
            {
                state = false;
                isReadEnd = true;
                readbuf <<= nextReadBit - index;
                out[outIndex++] = prevDC;
                break;
            }
            //assert(bin.isEnd, "数据不完整");
            readbuf |= (dword)bin.data;
        }
        readbuf >>= 1;

        bool bCodeFirstBit = (readbuf >> (nextReadBit - 1)) & 0x1L;
        if (bCodeFirstBit == 0)
            readbuf = ~readbuf & (0xFFFFFFFFL >> (32 - nextReadBit));

        out[outIndex] = (int)readbuf * (bCodeFirstBit ? 1 : -1) + prevDC;

        prevDC = out[outIndex++];
        hkey.bit = 0;
        hkey.key = 0;
        break;
    }

    //decoding ac
    for (hkey.bit = 0, hkey.key = 0; state && (outIndex < 64); hkey.key <<= 1)
    {
        br.get(&bin);
        if (bin.isEnd)
        {
            hkey.key |= 0; hkey.bit++;
        }
        //assert(bin.isEnd, "数据不完整");
        else
        {
            hkey.key |= (byte)bin.data; hkey.bit++;
            if (hkey.bit > 16)
            {
                throw MiuError(MAKE_MIU_ERROR_INFORMATION(JPEG_HUFFMAN_DATA_ERROR));
                return false;
            }
        }

        //itob((uint)hkey.key, hkey.bit, binstr);

        //查找匹配项
        int ans = AC_Huffman_Table->find(hkey);
        if (ans == -1)
            continue;
        else if (ans == 0)
            break;

        byte dwValue = (byte)ans;
        uint zeroBit = (uint)((dwValue >> 4) & 0xFL);
        uint nextReadBit = (uint)(dwValue & 0xFL);

        for (uint nZero = 0; nZero < zeroBit; nZero++, outIndex++)
        {
            if (outIndex >= 64)
                break;

            out[outIndex] = 0;
        }

        dword readbuf = 0;
        int nReadBit = 0;
        for (uint index = 0; index < nextReadBit; index++, readbuf <<= 1)
        {
            br.get(&bin);
            if (bin.isEnd)
            {
                state = false;
                isReadEnd = true;
                readbuf <<= nextReadBit - index;
                break;
            }
            readbuf |= (dword)bin.data;
        }
        readbuf >>= 1;

        bool bCodeFirstBit = (readbuf >> (nextReadBit - 1)) & 0x1L;
        if (bCodeFirstBit == 0)
            readbuf = ~readbuf & (0xFFFFFFFFL >> (32 - nextReadBit));

        out[outIndex] = (int)readbuf * (!isReadEnd && !bCodeFirstBit ? -1 : 1);

        outIndex++;
        hkey.key = 0;
        hkey.bit = 0;
    }

    return true;
}

void Miu::JpegDecoder::ReZigzag(int* sData, int* outputData)
{
    int original[64] = { 0 };
    MemoryCopy(original, sData, 64 * sizeof(int));

    for (int i = 0; i < 64; i++)
    {
        outputData[i] = original[ZigZag[i]];
    }
}

void Miu::JpegDecoder::ReQuantify(int * v, QuantizationTable & qt)
{
    for (int n = 0; n < 64; n++)
    {
        v[n] *= qt.content[ZigZag[n]];
    }
}

void Miu::JpegDecoder::IDCT(int * DCTData, char * YUVData)
{
    float result = .0f;

    for (int i = 0; i < 64; i++, result = 0)
    {
        for (int n = 0; n < 64; n++)
            result += idctTable[i * 64 + n] * DCTData[n];
        YUVData[i] = (char)(int)result;
    }
}

//
const char* file = 
//R"(YUV444.jpg)"                          // YUV444
R"(YUV420.jpg)"                            // YUV420
//
;

Miu::pixel Miu::JpegDecoder::YUVToRGB(char Y, char U, char V)
{
    int R = 0, G = 0, B = 0, A = 255;

    R = Y + debug_offset + 2.03211 * (U);
    G = Y + debug_offset - 0.39465 * (U) - 0.58060 * (V);
    B = Y + debug_offset + 1.13983 * (V);

    return RGBA(R, G, B, A);
}

void Miu::JpegDecoder::GetMCU_YUV444(char * Y, char * U, char * V, pixel * mcu)
{
    for (int i = 0; i < 64; i++)
    {
        mcu[i] = YUVToRGB(Y[i], U[i], V[i]);
    }
}

void Miu::JpegDecoder::GetMCU_YUV420(char ** Y, char * U, char * V, pixel** mcu)
{
    char realY[256] = { 0 }, realU[256] = { 0 }, realV[256] = { 0 };
    pixel rgb[256] = { 0 };

    for (int i = 0; i < 8; i++)
    {
        MemoryCopy(realY + i * 16, Y[0] + i * 8, 8);
        MemoryCopy(realY + i * 16 + 8, Y[1] + i * 8, 8);
        MemoryCopy(realY + (i + 8) * 16, Y[2] + i * 8, 8);
        MemoryCopy(realY + (i + 8) * 16 + 8, Y[3] + i * 8, 8);
    }

    int mx = 0, my = 0;
    for (int y = 0; y < 16; y += 2)
    {
        for (int x = 0; x < 16; x += 2)
        {
            int rUVindex = y * 16 + x, UVindex = my * 8 + mx;

            realU[rUVindex] = U[UVindex];
            realU[rUVindex + 1] = U[UVindex];
            realU[rUVindex + 16] = U[UVindex];
            realU[rUVindex + 17] = U[UVindex];

            realV[rUVindex] = V[UVindex];
            realV[rUVindex + 1] = V[UVindex];
            realV[rUVindex + 16] = V[UVindex];
            realV[rUVindex + 17] = V[UVindex];

            mx++;
        }
        my++;
        mx = 0;
    }

    for (int i = 0; i < 256; i++)
    {
        rgb[i] = YUVToRGB(realY[i], realU[i], realV[i]);
    }
    for (int i = 0; i < 8; i++)
    {
        MemoryCopy(mcu[0] + i * 8, rgb + i * 16, 8 * 4);
        MemoryCopy(mcu[1] + i * 8, rgb + i * 16 + 8, 8 * 4);
        MemoryCopy(mcu[2] + i * 8, rgb + (i + 8) * 16, 8 * 4);
        MemoryCopy(mcu[3] + i * 8, rgb + (i + 8) * 16 + 8, 8 * 4);
    }


    return;
}

void Miu::JpegDecoder::WriteMCU(ulong x, ulong y, pixel* mcu)
{
    for (int iy = 0; iy < 8; iy++)
    {
        MemoryCopy(rgbData + (y * 8 + iy) * sof.wWidth + x * 8, mcu + iy * 8, 32);
    }
}

void Miu::JpegDecoder::MCUDecoder(BinaryReader & br, int & prevDC_Y, int & prevDC_Cb, int & prevDC_Cr, pixel ** mcu)
{
    bool isReadEnd = false;
    int DCU[64] = { 0 }, nY = 0;
    char** Y = nullptr, Cb[64] = { 0 }, Cr[64] = { 0 };

    Y = new char*[sampType];
    for (int i = 0; i < sampType; i++)
    {
        Y[i] = new char[64];
        MemoryZero(Y[i], 64);
    }

    for (int i = 0; i < sampType; i++)
    {
        HuffmanDecoder(
            br,
            huffmanTableBox.find(BYTE_LOWORD(sos["Y"]), 0),
            huffmanTableBox.find(BYTE_LOWORD(sos["Y"]), 1),
            prevDC_Y,
            DCU,
            isReadEnd
        );

        ReZigzag(DCU, DCU);
        ReQuantify(DCU, *quantTableBox[0]);
        IDCT(DCU, Y[nY++]);

        MemoryZero(DCU, 64 * 4);
    }

    HuffmanDecoder(
        br,
        huffmanTableBox.find(BYTE_LOWORD(sos["Cb"]), 0),
        huffmanTableBox.find(BYTE_LOWORD(sos["Cb"]), 1),
        prevDC_Cb,
        DCU,
        isReadEnd
    );
    ReZigzag(DCU, DCU);
    ReQuantify(DCU, *quantTableBox[1]);
    IDCT(DCU, Cb);

    MemoryZero(DCU, 64 * 4);

    HuffmanDecoder(
        br,
        huffmanTableBox.find(BYTE_LOWORD(sos["Cr"]), 0),
        huffmanTableBox.find(BYTE_LOWORD(sos["Cr"]), 1),
        prevDC_Cr,
        DCU,
        isReadEnd
    );
    ReZigzag(DCU, DCU);
    ReQuantify(DCU, *quantTableBox[1]);
    IDCT(DCU, Cr);

    switch (sampType)
    {
    case SAMP_YUV444:
        GetMCU_YUV444(Y[0], Cb, Cr, mcu[0]);
        break;
    case SAMP_YUV420:
        GetMCU_YUV420(Y, Cb, Cr, mcu);
        break;
    case SAMP_UNKNOWN:
        throw MiuError(MAKE_MIU_ERROR_INFORMATION(JPEG_SAMPLING_DATA_ERROR));
        return;
    }

    for (int i = 0; i < sampType; i++)
        delete[] Y[i];
    delete[] Y;
}




