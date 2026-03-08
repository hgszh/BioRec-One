// vim: ts=4 sw=4 expandtab

// THIS IS GENERATED C CODE.
// https://bues.ch/h/crcgen
// 
// This code is Public Domain.
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted.
// 
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
// RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
// NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
// USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CRC_H_
#define CRC_H_

#include <stdint.h>

// CRC polynomial coefficients: x^8 + x^2 + x + 1
//                              0x7 (hex)
// CRC width:                   8 bits
// CRC shift direction:         left (big endian)
// Input word width:            8 bits

#ifdef b
# undef b
#endif
#define b(x, b) (((x) >> (b)) & 1u)

uint8_t crc8_compute(uint8_t crc_in, uint8_t data)
{
    uint8_t ret;
    ret  = (uint8_t)(b(crc_in, 0) ^ b(crc_in, 6) ^ b(crc_in, 7) ^ b(data, 0) ^ b(data, 6) ^ b(data, 7)) << 0;
    ret |= (uint8_t)(b(crc_in, 0) ^ b(crc_in, 1) ^ b(crc_in, 6) ^ b(data, 0) ^ b(data, 1) ^ b(data, 6)) << 1;
    ret |= (uint8_t)(b(crc_in, 0) ^ b(crc_in, 1) ^ b(crc_in, 2) ^ b(crc_in, 6) ^ b(data, 0) ^ b(data, 1) ^ b(data, 2) ^ b(data, 6)) << 2;
    ret |= (uint8_t)(b(crc_in, 1) ^ b(crc_in, 2) ^ b(crc_in, 3) ^ b(crc_in, 7) ^ b(data, 1) ^ b(data, 2) ^ b(data, 3) ^ b(data, 7)) << 3;
    ret |= (uint8_t)(b(crc_in, 2) ^ b(crc_in, 3) ^ b(crc_in, 4) ^ b(data, 2) ^ b(data, 3) ^ b(data, 4)) << 4;
    ret |= (uint8_t)(b(crc_in, 3) ^ b(crc_in, 4) ^ b(crc_in, 5) ^ b(data, 3) ^ b(data, 4) ^ b(data, 5)) << 5;
    ret |= (uint8_t)(b(crc_in, 4) ^ b(crc_in, 5) ^ b(crc_in, 6) ^ b(data, 4) ^ b(data, 5) ^ b(data, 6)) << 6;
    ret |= (uint8_t)(b(crc_in, 5) ^ b(crc_in, 6) ^ b(crc_in, 7) ^ b(data, 5) ^ b(data, 6) ^ b(data, 7)) << 7;
    return ret;
}
#undef b

#endif /* CRC_H_ */