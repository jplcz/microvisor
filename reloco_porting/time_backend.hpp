
namespace reloco
{

    template <>
    struct instant_clock_traits<cntpct_clock_tag>
    {
        [[nodiscard]] static duration now() noexcept
        {
            std::uint64_t pct;
            std::uint32_t frq;

            // Read the 64-bit Physical Count Register (CNTPCT)
            // %Q0 maps to the lower 32-bit register, %R0 maps to the upper 32-bit register
            asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r"(pct));

            // Read the 32-bit Counter Frequency Register (CNTFRQ)
            asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(frq));

            // Safeguard against division by zero if the timer isn't initialized yet
            if (frq == 0)
            {
                return duration::from_secs(0);
            }

            // Calculate full seconds
            std::uint64_t secs = pct / frq;

            // Calculate remaining ticks, then convert to nanoseconds
            std::uint64_t rem_ticks = pct % frq;
            std::uint64_t nanos = (rem_ticks * 1'000'000'000ULL) / frq;

            return duration::from_secs(secs) + duration::from_nanos(nanos);
        }
    };

}
