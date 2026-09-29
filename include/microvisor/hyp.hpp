#pragma once

#include <cstdint>

extern "C"
{
    // The vector table symbol from assembly
    extern uint32_t hyp_vector_table[];

    // Set the HVBAR register
    void hyp_set_hvbar(uintptr_t addr);

    // Get Current Program Status Register
    uint32_t hyp_get_cpsr();
}

namespace microvisor
{

    // ARM CPSR Mode Bits
    constexpr uint32_t CPSR_MODE_MASK = 0x1F;
    constexpr uint32_t CPSR_MODE_HYP = 0x1A;
    constexpr uint32_t CPSR_MODE_SVC = 0x13;

    /**
     * @brief Verifies if the CPU is currently in Hyp mode.
     */
    inline bool is_hyp_mode()
    {
        return (hyp_get_cpsr() & CPSR_MODE_MASK) == CPSR_MODE_HYP;
    }

    /**
     * @brief Installs the Hyp vector table.
     */
    inline void init_hvbar()
    {
        // Pass the physical address of our vector table array
        hyp_set_hvbar(reinterpret_cast<uintptr_t>(hyp_vector_table));
    }

} // namespace microvisor
