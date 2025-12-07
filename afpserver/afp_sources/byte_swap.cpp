#include "byte_swap.h"

uint32 _byteswap_ulong(uint32 i)
{
    uint32 j;
    j =  (i << 24);
    j += (i <<  8) & 0x00FF0000;
    j += (i >>  8) & 0x0000FF00;
    j += (i >> 24);
    return j;
}

uint16 _byteswap_ushort(uint16 i)
{
    uint16 j;
    j =  (i << 8) ;
    j += (i >> 8) ;
    return j;
}

uint64 _byteswap_uint64(uint64 i)
{
    uint64 j;
    j =  (i << 56);
    j += (i << 40)&0x00FF000000000000LL;
    j += (i << 24)&0x0000FF0000000000LL;
    j += (i <<  8)&0x000000FF00000000LL;
    j += (i >>  8)&0x00000000FF000000LL;
    j += (i >> 24)&0x0000000000FF0000LL;
    j += (i >> 40)&0x000000000000FF00LL;
    j += (i >> 56);
    return j;
}

