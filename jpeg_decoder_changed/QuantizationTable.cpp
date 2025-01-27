
#include "MiuJpegDecoder.h"

Miu::QuantizationTable::QuantizationTable() :
    content(nullptr),
    u_qt_size(0),
    u_qt_id(0),
    u_qt_accur(0)
{
}

Miu::QuantizationTable::~QuantizationTable()
{
    if (content != nullptr)
    {
        delete[] content;
        content = nullptr;
    }
}

Miu::QuantizationTableBox::QuantizationTableBox() : hashTable()
{
}

Miu::QuantizationTableBox::~QuantizationTableBox()
{
    clear();
}

void Miu::QuantizationTableBox::push(QuantizationTable * qt)
{
    hashTable[qt->u_qt_id] = quantTable.size();
    quantTable.push_back(qt);
}

void Miu::QuantizationTableBox::clear()
{
    for (auto it : quantTable)
    {
        delete it;
    }
}

void Miu::QuantizationTableBox::print()
{
    for (auto& it : quantTable)
    {
        std::cerr << "--Quantization table(" << (int)it->u_qt_id << "):------------------------------------------------\n";
        for (uint i = 0; i < 8; i++)
        {
            for (uint j = 0; j < 8; j++)
            {
                std::cerr << (int)it->content[i * 8 + j] << "\t";
            }
            std::cerr << "\n";
        }
        std::cerr << "------------------------------------------------------------------------\n\n";
    }
}

