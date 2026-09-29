// SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>

namespace microvisor::mmio
{

    enum class status
    {
        handled,
        unhandled,
        halt_vm
    };

    class device
    {
    public:
        virtual ~device() {}

        /**
         * @brief Reads from the device at a device-relative offset.
         * @param offset Offset from the device's registered base IPA.
         * @param size   Access size in bytes (1, 2, or 4).
         * @param val    Output value to place into the guest register.
         */
        virtual status read(uint32_t offset, uint32_t size, uint32_t &val) noexcept = 0;

        /**
         * @brief Writes to the device at a device-relative offset.
         * @param offset Offset from the device's registered base IPA.
         * @param size   Access size in bytes (1, 2, or 4).
         * @param val    Data value written by the guest.
         */
        virtual status write(uint32_t offset, uint32_t size, uint32_t val) noexcept = 0;
    };

} // namespace microvisor::mmio