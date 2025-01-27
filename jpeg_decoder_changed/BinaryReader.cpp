
#include "MiuJpegDecoder.h"


Miu::BinaryReader::BinaryReader(ulong dwSize, byte * bData) :
    dwSize(dwSize),
    dwGlobalIndex(0),
    dwBitIndex(0),
    dwBitSize(0)
{
    init(bData);
}

Miu::BinaryReader::~BinaryReader()
{
    if (bData != nullptr)
    {
        delete[] bData;
        bData = nullptr;
    }
}

void Miu::BinaryReader::get(LPBIN_STRUCT lpbs)

{
    if (lpbs->isEnd)
        return;

    if (dwBitIndex + 1 == dwBitSize)
    {
        lpbs->isEnd = true;
        lpbs->data = 0;
    }

    lpbs->data = (bData[dwGlobalIndex] >> (7 - (dwBitIndex++ % 8))) & 0x1;
    if (dwBitIndex % 8 == 0)
        dwGlobalIndex++;

    return;
}

Miu::ulong Miu::BinaryReader::getpos()
{
    return dwBitIndex;
}

void Miu::BinaryReader::init(byte * data)
{
    dwBitSize = dwSize * 8;
    bData = new byte[dwSize];
    MemoryCopy(bData, data, dwSize);
}
