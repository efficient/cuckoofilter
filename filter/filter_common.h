/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _FILTER_COMMON_H_
#define _FILTER_COMMON_H_

#include <stdio.h>
#include <string.h>

#include "printutil.h"
#include "debug.h"

namespace hashfilter{

    enum Status {
        Ok = 0,
        NotFound = 1,
        NotEnoughSpace = 2,
        NotSupported = 3,
    }; // status returned by a filter

    const unsigned char op[8] = {1,2,4,8,16,32,64,128};

    //
    // steal from Iulian's rolling hashing
    // used for Bloom filter
    //
    uint32_t rot[20] = {
        0xF53E1837, 0x5F14C86B, 0x9EE3964C, 0xFA796D53,
        0x32223FC3, 0x4D82BC98, 0xA0C7FA62, 0x63E2C982,
        0x24994A5B, 0x1ECE7BEE, 0x292B38EF, 0xD5CD4E56,
        0x514F4303, 0x7BE12B83, 0x7192F195, 0x82DC7300,
        0x084380B4, 0x480B55D3, 0x5F430471, 0x13F75991
    };



}
#endif // #ifndef _FILTER_COMMON_H_
