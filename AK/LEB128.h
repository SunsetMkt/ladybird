/*
 * Copyright (c) 2020-2021, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/DeprecatedStream.h>
#include <AK/NumericLimits.h>
#include <AK/Types.h>

namespace AK {

struct LEB128 {
    template<typename ValueType = size_t>
    static bool read_unsigned(DeprecatedInputStream& stream, ValueType& result)
    {
        result = 0;
        size_t num_bytes = 0;
        while (true) {
            if (stream.unreliable_eof()) {
                stream.set_fatal_error();
                return false;
            }
            u8 byte = 0;
            stream >> byte;
            if (stream.has_any_error())
                return false;

            ValueType masked_byte = byte & ~(1 << 7);
            bool const shift_too_large_for_result = num_bytes * 7 > sizeof(ValueType) * 8;
            if (shift_too_large_for_result)
                return false;

            bool const shift_too_large_for_byte = ((masked_byte << (num_bytes * 7)) >> (num_bytes * 7)) != masked_byte;
            if (shift_too_large_for_byte)
                return false;

            result = (result) | (masked_byte << (num_bytes * 7));
            if (!(byte & (1 << 7)))
                break;
            ++num_bytes;
        }

        return true;
    }

    template<typename ValueType = ssize_t>
    static bool read_signed(DeprecatedInputStream& stream, ValueType& result)
    {
        // Note: We read into a u64 to simplify the parsing logic;
        //    result is range checked into ValueType after parsing.
        static_assert(sizeof(ValueType) <= sizeof(u64), "Error checking logic assumes 64 bits or less!");

        i64 temp = 0;
        size_t num_bytes = 0;
        u8 byte = 0;
        result = 0;

        do {
            if (stream.unreliable_eof()) {
                stream.set_fatal_error();
                return false;
            }

            stream >> byte;
            if (stream.has_any_error())
                return false;

            // note: 64 bit assumptions!
            u64 masked_byte = byte & ~(1 << 7);
            bool const shift_too_large_for_result = num_bytes * 7 >= 64;
            if (shift_too_large_for_result)
                return false;

            bool const shift_too_large_for_byte = (num_bytes * 7) == 63 && masked_byte != 0x00 && masked_byte != 0x7Fu;
            if (shift_too_large_for_byte)
                return false;

            temp = (temp) | (masked_byte << (num_bytes * 7));
            ++num_bytes;
        } while (byte & (1 << 7));

        if ((num_bytes * 7) < 64 && (byte & 0x40)) {
            // sign extend
            temp |= ((u64)(-1) << (num_bytes * 7));
        }

        // Now that we've accumulated into an i64, make sure it fits into result
        if constexpr (sizeof(ValueType) < sizeof(u64)) {
            if (temp > NumericLimits<ValueType>::max() || temp < NumericLimits<ValueType>::min())
                return false;
        }

        result = static_cast<ValueType>(temp);

        return true;
    }
};

}

#if USING_AK_GLOBALLY
using AK::LEB128;
#endif
