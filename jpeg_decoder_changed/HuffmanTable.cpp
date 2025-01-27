
#include "MiuJpegDecoder.h"

Miu::HuffmanTable::HuffmanTable() :
    u_ht_id(0),
    u_ht_type(0)
{
}

int Miu::HuffmanTable::find(Huffman_Key & hk)
{
    for (auto& it : real_table)
    {
        if (it.bit == hk.bit && it.key == hk.key)
            return (int)it.value;
    }

    return -1;
}

Miu::HuffmanTableBox::~HuffmanTableBox()
{
    clear();
}

void Miu::HuffmanTableBox::push(HuffmanTable * ht)
{
    huffmanTable.push_back(ht);
}

void Miu::HuffmanTableBox::clear()
{
    for (auto it : huffmanTable)
    {
        delete it;
    }
}

Miu::HuffmanTable * Miu::HuffmanTableBox::find(byte id, byte type)
{
    for (auto& it : huffmanTable)
    {
        if (it->u_ht_id == id && it->u_ht_type == type)
            return it;
    }
    return nullptr;
}

void Miu::HuffmanTableBox::print()
{
    for (auto& it : huffmanTable)
    {
        std::string htype = (it->u_ht_type == 0 ? "DC" : "AC");
        std::cerr << "--Huffman table(" << (int)it->u_ht_id << "," << htype << "):-----------------------------------------------------\n";
        for (Huffman_Key& itk : it->real_table)
        {
            char ch[256] = { 0 };
            itob(itk.key, itk.bit, ch);

            std::cerr << (int)itk.bit << "\t|" <<
                (int)itk.key << "\t|" <<
                (int)itk.value << "\t|" <<
                ch << "\n";
        }
        std::cerr << "------------------------------------------------------------------------\n\n";
    }
}

void Miu::HuffmanTableBox::itob(uint value, int bit, char * ch)
{
    char tmp_ch[2] = "";
    for (int nIndex = 0; nIndex < bit; nIndex++)
    {
        int tmp_value = value >> nIndex;
        tmp_value &= 0x1;

        _itoa_s(tmp_value, tmp_ch, 2, 10);
        ch[bit - nIndex - 1] = tmp_ch[0];
    }
}


