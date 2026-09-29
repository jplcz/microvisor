// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>
#include <cstddef>
#include "microvisor/mmio/device.hpp"

namespace microvisor::mmio
{

    struct region
    {
        uint32_t base;
        uint32_t size;
        device *dev;
    };

    template <size_t MaxDevices = 16>
    class bus
    {
    public:
        constexpr bus() noexcept : m_count(0), m_regions{} {}

        /**
         * @brief Registers an MMIO device over an IPA window.
         */
        bool register_device(uint32_t base_ipa, uint32_t size, device *dev) noexcept
        {
            if (m_count >= MaxDevices || dev == nullptr || size == 0)
            {
                return false;
            }

            // Detect overlapping regions
            for (size_t i = 0; i < m_count; ++i)
            {
                uint32_t r_start = m_regions[i].base;
                uint32_t r_end = r_start + m_regions[i].size;
                uint32_t n_start = base_ipa;
                uint32_t n_end = base_ipa + size;

                if (n_start < r_end && n_end > r_start)
                {
                    return false; // Overlap detected
                }
            }

            m_regions[m_count++] = region{base_ipa, size, dev};
            return true;
        }

        /**
         * @brief Routes an MMIO access to the target device.
         */
        status dispatch(uint32_t ipa, uint32_t size, bool is_write, uint32_t &value) noexcept
        {
            for (size_t i = 0; i < m_count; ++i)
            {
                const auto &reg = m_regions[i];
                if (ipa >= reg.base && ipa < (reg.base + reg.size))
                {
                    uint32_t offset = ipa - reg.base;
                    if (is_write)
                    {
                        return reg.dev->write(offset, size, value);
                    }
                    else
                    {
                        return reg.dev->read(offset, size, value);
                    }
                }
            }
            return status::unhandled;
        }

    private:
        size_t m_count;
        region m_regions[MaxDevices];
    };

} // namespace microvisor::mmio