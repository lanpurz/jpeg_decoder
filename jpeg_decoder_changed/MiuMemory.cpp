
#include "MiuJpegDecoder.h"

void Miu::MemoryCopy(void* tagMemAddr, const void* srcMemAddr, ulong tagSize)
{
    long d = ((long)tagMemAddr - (long)srcMemAddr);
    char* tbuf = (char*)tagMemAddr, * sbuf = (char*)srcMemAddr;

    //�ж��ڴ������Ƿ��ص�    
    if (((d > 0 ? d : -d) < (long)tagSize) && (d > 0))
    {
        //�Ӻ���ǰ����
        for (long index = (long)tagSize - 1; index >= 0; index--)
            tbuf[index] = sbuf[index];
    }
    else
    {
        for (ulong index = 0; index < tagSize; index++)
            tbuf[index] = sbuf[index];
    }
}

void Miu::MemoryZero(void* tagMemAddr, ulong tagSize)
{
    char* buf = (char*)tagMemAddr;
    for (uint index = 0; index < tagSize; index++)
        buf[index] = 0x0;
}
