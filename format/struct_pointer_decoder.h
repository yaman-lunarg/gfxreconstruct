/*
** Copyright (c) 2018 LunarG, Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef BRIMSTONE_FORMAT_STRUCT_POINTER_DECODER_H
#define BRIMSTONE_FORMAT_STRUCT_POINTER_DECODER_H

#include <cassert>
#include <memory>

#include "util/defines.h"
#include "format/format.h"
#include "format/pointer_decoder_base.h"
#include "format/value_decoder.h"

#include "generated/generated_struct_decode_declarations.inc"

BRIMSTONE_BEGIN_NAMESPACE(brimstone)
BRIMSTONE_BEGIN_NAMESPACE(format)

template<typename T>
class StructPointerDecoder : public PointerDecoderBase
{
public:
    StructPointerDecoder() : decoded_structs_(nullptr), struct_memory_(nullptr), capacity_(0), is_memory_external_(false) { }

    ~StructPointerDecoder()
    {
        if ((struct_memory_ != nullptr) && !is_memory_external_)
        {
            delete [] struct_memory_;
        }

        if (decoded_structs_ != nullptr)
        {
            delete [] decoded_structs_;
        }
    }

    T* GetMetaStructPointer() const { return decoded_structs_; }

    typename T::struct_type* GetPointer() const { return struct_memory_; }

    void SetExternalMemory(typename T::struct_type* data, size_t capacity)
    {
        if ((data != nullptr) && (capacity > 0))
        {
            struct_memory_ = data;
            capacity_ = capacity;
            is_memory_external_ = true;
        }
        else
        {
            // TODO: Log an error message
        }
    }

    size_t Decode(const uint8_t* buffer, size_t buffer_size)
    {
        size_t bytes_read = DecodeAttributes(buffer, buffer_size);

        // We should only be decoding structs.
        assert((GetAttributeMask() & PointerAttributes::kIsStruct) == PointerAttributes::kIsStruct);

        if (!IsNull() && HasData())
        {
            size_t len = GetLength();

            if (!is_memory_external_)
            {
                assert(struct_memory_ == nullptr);

                struct_memory_ = new typename T::struct_type[len];
                capacity_ = len;
            }
            else
            {
                assert(struct_memory_ != nullptr);
                assert(len <= capacity_);

                // TODO: Log a message if len > capacity, which should never happen.
                if ((struct_memory_ == nullptr) || (len > capacity_))
                {
                    is_memory_external_ = false;
                    struct_memory_ = new typename T::struct_type[len];
                    capacity_ = len;
                }
            }

            decoded_structs_ = new T[len];

            for (size_t i = 0; i < len; ++i)
            {
                decoded_structs_[i].value = &struct_memory_[i];

                // Note: We only expect this class to be used with structs that have a decode_struct function.
                //       If an error is encoutered here due to a new struct type, the struct decoders need to be
                //       updated to support the new type.
                bytes_read += decode_struct((buffer + bytes_read), (buffer_size - bytes_read), &decoded_structs_[i]);
            }
        }

        return bytes_read;
    }

private:
    T*                          decoded_structs_;
    typename T::struct_type*    struct_memory_;
    size_t                      capacity_;
    bool                        is_memory_external_;
};

BRIMSTONE_END_NAMESPACE(format)
BRIMSTONE_END_NAMESPACE(brimstone)

#endif // BRIMSTONE_FORMAT_STRUCT_POINTER_DECODER_H