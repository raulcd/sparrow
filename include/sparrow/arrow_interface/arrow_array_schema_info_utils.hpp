// Copyright 2024 Man Group Operations Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <algorithm>

#include "sparrow/buffer/buffer_adaptor.hpp"
#include "sparrow/buffer/buffer_view.hpp"
#include "sparrow/c_interface.hpp"
#include "sparrow/types/data_type.hpp"
#include "sparrow/utils/contracts.hpp"

namespace sparrow
{
    /// @returns `true` if the number of buffers in an `ArrowArray` for a given data type is valid, `false`
    /// otherwise.
    constexpr bool validate_buffers_count(data_type data_type, int64_t n_buffers)
    {
        const std::size_t expected_buffer_count = get_expected_buffer_count(data_type);
        return static_cast<std::size_t>(n_buffers) == expected_buffer_count;
    }

    /// @returns The the expected number of children for a given data type.
    constexpr std::size_t get_expected_children_count(data_type data_type)
    {
        switch (data_type)
        {
            case data_type::NA:
            case data_type::RUN_ENCODED:
            case data_type::BOOL:
            case data_type::UINT8:
            case data_type::INT8:
            case data_type::UINT16:
            case data_type::INT16:
            case data_type::UINT32:
            case data_type::INT32:
            case data_type::FLOAT:
            case data_type::UINT64:
            case data_type::INT64:
            case data_type::DOUBLE:
            case data_type::HALF_FLOAT:
            case data_type::TIMESTAMP:
            case data_type::FIXED_SIZE_BINARY:
            case data_type::DECIMAL:
            case data_type::FIXED_WIDTH_BINARY:
            case data_type::STRING:
            case data_type::BINARY:
                return 0;
            case data_type::LIST:
            case data_type::LARGE_LIST:
            case data_type::LIST_VIEW:
            case data_type::LARGE_LIST_VIEW:
            case data_type::FIXED_SIZED_LIST:
            case data_type::STRUCT:
            case data_type::MAP:
            case data_type::SPARSE_UNION:
                return 1;
            case data_type::DENSE_UNION:
                return 2;
        }
        mpl::unreachable();
    }

    /// @returns `true` if  the format of an `ArrowArray` for a given data type is valid, `false` otherwise.
    inline bool validate_format_with_arrow_array(data_type, const ArrowArray&)
    {
        return true;
        /* THE CODE BELOW MAKES WRONG ASSUMPTIONS AND NEEDS TO BE REFACTORED IN A SEPERATE PR*/
        // const bool buffers_count_valid = validate_buffers_count(data_type, array.n_buffers);
        // // const bool children_count_valid = static_cast<std::size_t>(array.n_children)
        // //                                   == get_expected_children_count(data_type);

        // //std::cout<<"child cound: "<<array.n_children<<" expected:
        // "<<get_expected_children_count(data_type)<<std::endl; return buffers_count_valid //&&
        // children_count_valid;
    }

    enum class buffer_type : uint8_t
    {
        VALIDITY,
        DATA,
        OFFSETS_32BIT,
        OFFSETS_64BIT,
        VIEWS,
        TYPE_IDS,
        SIZES_32BIT,
        SIZES_64BIT,
    };

    /// @returns A vector of buffer types for a given data type.
    /// This information helps how interpret and parse each buffer in an ArrowArray.
    constexpr std::vector<buffer_type> get_buffer_types_from_data_type(data_type data_type)
    {
        switch (data_type)
        {
            case data_type::BOOL:
            case data_type::UINT8:
            case data_type::INT8:
            case data_type::UINT16:
            case data_type::INT16:
            case data_type::UINT32:
            case data_type::INT32:
            case data_type::FLOAT:
            case data_type::UINT64:
            case data_type::INT64:
            case data_type::DOUBLE:
            case data_type::HALF_FLOAT:
            case data_type::TIMESTAMP:
            case data_type::FIXED_SIZE_BINARY:
            case data_type::DECIMAL:
            case data_type::FIXED_WIDTH_BINARY:
                return {buffer_type::VALIDITY, buffer_type::DATA};
            case data_type::BINARY:
            case data_type::STRING:
                return {buffer_type::VALIDITY, buffer_type::OFFSETS_32BIT, buffer_type::DATA};
            case data_type::LIST:
                return {buffer_type::VALIDITY, buffer_type::OFFSETS_32BIT};
            case data_type::LARGE_LIST:
                return {buffer_type::VALIDITY, buffer_type::OFFSETS_64BIT};
            case data_type::LIST_VIEW:
                return {buffer_type::VALIDITY, buffer_type::OFFSETS_32BIT, buffer_type::SIZES_32BIT};
            case data_type::LARGE_LIST_VIEW:
                return {buffer_type::VALIDITY, buffer_type::OFFSETS_64BIT, buffer_type::SIZES_64BIT};
            case data_type::FIXED_SIZED_LIST:
            case data_type::STRUCT:
                return {buffer_type::VALIDITY};
            case data_type::SPARSE_UNION:
                return {buffer_type::TYPE_IDS};
            case data_type::DENSE_UNION:
                return {buffer_type::TYPE_IDS, buffer_type::OFFSETS_32BIT};
            case data_type::NA:
            case data_type::MAP:
            case data_type::RUN_ENCODED:
                return {};
        }
        mpl::unreachable();
    }

    constexpr size_t get_buffer_type_index(data_type data_type, buffer_type buffer_type)
    {
        const auto buffer_types = get_buffer_types_from_data_type(data_type);
        const auto it = std::find(buffer_types.begin(), buffer_types.end(), buffer_type);
        if (it == buffer_types.end())
        {
            throw std::runtime_error("Unsupported buffer type");
        }
        return static_cast<size_t>(std::distance(buffer_types.begin(), it));
    }

    /// @returns The expected offset element count for a given data type and array length.
    constexpr std::size_t get_offset_element_count(data_type data_type, size_t length, size_t offset)
    {
        switch (data_type)
        {
            case data_type::STRING:
            case data_type::LIST:
            case data_type::LARGE_LIST:
                return length + offset + 1;
            case data_type::LIST_VIEW:
            case data_type::LARGE_LIST_VIEW:
            case data_type::DENSE_UNION:
                return length + offset;
            default:
                throw std::runtime_error("Unsupported data type");
        }
    }

    /// @returns The number of bytes required according to the provided buffer type, length, offset and data
    /// type.
    inline std::size_t compute_buffer_size(
        buffer_type bt,
        size_t length,
        size_t offset,
        data_type dt,
        const std::vector<sparrow::buffer_view<uint8_t>>& previous_buffers,
        buffer_type previous_buffer_type
    )
    {
        constexpr size_t bit_per_byte = 8;
        switch (bt)
        {
            case buffer_type::VALIDITY:
                return (length + offset + bit_per_byte - 1) / bit_per_byte;
            case buffer_type::DATA:
                if (bt == buffer_type::DATA && (dt == data_type::STRING || dt == data_type::BINARY))
                {
                    SPARROW_ASSERT_TRUE(
                        previous_buffer_type == buffer_type::OFFSETS_32BIT
                        || previous_buffer_type == buffer_type::OFFSETS_64BIT
                    );
                    if (previous_buffers.back().empty())
                    {
                        return 0;
                    }
                    else if (previous_buffer_type == buffer_type::OFFSETS_32BIT)
                    {
                        const auto offset_buf = make_buffer_adaptor<int32_t>(previous_buffers.back());
                        return static_cast<std::size_t>(offset_buf.back());
                    }
                    else
                    {
                        const auto offset_buf = make_buffer_adaptor<int64_t>(previous_buffers.back());
                        return static_cast<std::size_t>(offset_buf.back());
                    }
                }
                return primitive_bytes_count(dt, length + offset);
            case buffer_type::OFFSETS_32BIT:
            case buffer_type::SIZES_32BIT:
                return get_offset_element_count(dt, length, offset) * sizeof(std::int32_t);
            case buffer_type::OFFSETS_64BIT:
            case buffer_type::SIZES_64BIT:
                return get_offset_element_count(dt, length, offset) * sizeof(std::int64_t);
            case buffer_type::VIEWS:
                // TODO: Implement
                SPARROW_ASSERT(false, "Not implemented");
                return 0;
            case buffer_type::TYPE_IDS:
                return length + offset;
        }
        mpl::unreachable();
    }

    constexpr bool has_bitmap(data_type dt)
    {
        switch (dt)
        {
            // List all data types. We use the default warning to catch missing cases.
            case data_type::BOOL:
            case data_type::INT8:
            case data_type::INT16:
            case data_type::INT32:
            case data_type::INT64:
            case data_type::UINT8:
            case data_type::UINT16:
            case data_type::UINT32:
            case data_type::UINT64:
            case data_type::HALF_FLOAT:
            case data_type::FLOAT:
            case data_type::DOUBLE:
            case data_type::TIMESTAMP:
            case data_type::DECIMAL:
            case data_type::LIST:
            case data_type::STRUCT:
            case data_type::MAP:
            case data_type::STRING:
            case data_type::BINARY:
            case data_type::FIXED_SIZE_BINARY:
            case data_type::FIXED_WIDTH_BINARY:
            case data_type::LARGE_LIST:
            case data_type::LIST_VIEW:
            case data_type::LARGE_LIST_VIEW:
            case data_type::FIXED_SIZED_LIST:
                return true;
            case data_type::NA:
            case data_type::SPARSE_UNION:
            case data_type::DENSE_UNION:
            case data_type::RUN_ENCODED:
                return false;
        }
        mpl::unreachable();
    }
}
