#ifndef SHA256_H_
#define SHA256_H_

#include <stddef.h>
#include <stdint.h>

#include "mainHeader.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct {
        UINT8  buf[64];
        UINT32 hash[8];
        UINT32 bits[2];
        UINT32 len;
        UINT32 rfu__;
        UINT32 W[64];
    } sha256_context;

    void sha256_init(sha256_context* ctx);
    void sha256_h(sha256_context* ctx, CONST UINT16* data, size_t len);
    void sha256_done(sha256_context* ctx, UINT8* hash);

    void sha256_hash(CONST UINT16* data, UINTN len, CONST UINT8* hash);

#ifdef __cplusplus
}
#endif

#endif