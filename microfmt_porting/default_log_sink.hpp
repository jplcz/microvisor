#include "microfmt/log/sink.hpp"

#include <microfmt/hw/pl011_sink.hpp>

namespace microfmt::log
{

    class uart_log_sink;

    /** @brief Tag selecting `uart_log_sink` as a `log_sink` backend. */
    struct uart_log_sink_tag
    {
    };

    template <>
    struct log_sink_traits<uart_log_sink_tag>
    {
        using context_type = uart_log_sink;

        static void log(value_ref<context_type> ctx, const log_msg &msg) noexcept;
    };

    /** @brief Adapter that writes structured records to the FreeBSD kernel's
     * `log(9)`. */
    class uart_log_sink
    {
        microfmt::pl011_sink pl011_;

        // Helper to map log levels to ANSI color codes
        static constexpr const char *level_color(level lvl) noexcept
        {
            switch (lvl)
            {
            case level::trace:
                return "\x1b[90m"; // Bright Gray
            case level::debug:
                return "\x1b[36m"; // Cyan
            case level::info:
                return "\x1b[32m"; // Green
            case level::warn:
                return "\x1b[33m"; // Yellow
            case level::err:
                return "\x1b[31m"; // Red
            case level::critical:
                return "\x1b[1;31m"; // Bold Red
            default:
                return "\x1b[0m"; // Reset
            }
        }

        // Helper to provide a fixed-width string for the level
        static constexpr const char *level_name(level lvl) noexcept
        {
            switch (lvl)
            {
            case level::trace:
                return "TRC";
            case level::debug:
                return "DBG";
            case level::info:
                return "INF";
            case level::warn:
                return "WRN";
            case level::err:
                return "ERR";
            case level::critical:
                return "CRT";
            default:
                return "OFF";
            }
        }

    public:
        uart_log_sink() : pl011_(0x09000000, /*translate_crlf=*/true)
        {
            pl011_.enable();
        }

        [[nodiscard]] log_sink as_sink() noexcept RELOCO_LIFETIMEBOUND
        {
            return log_sink(uart_log_sink_tag{}, *this);
        }

        void log_impl(const log_msg &msg) noexcept
        {
            if (msg.lvl == level::off)
            {
                return;
            }
            microfmt::format_to(
                pl011_.as_sink(),
                "\x1b[90m[{}]\x1b[0m {}[{}]\x1b[0m {}\n",
                msg.logger_name,
                level_color(msg.lvl),
                level_name(msg.lvl),
                msg.payload);
        }
    };

    inline void
    log_sink_traits<uart_log_sink_tag>::log(value_ref<context_type> ctx,
                                            const log_msg &msg) noexcept
    {
        ctx->log_impl(msg);
    }

    namespace detail
    {

        /** @brief Built-in-shaped replacement: a `logger` backed by `uart_log_sink`. */
        inline logger &built_in_default_logger() noexcept
        {
            static uart_log_sink sink_instance;
            static logger instance("kernel", sink_instance.as_sink());
            return instance;
        }

    } // namespace detail

} // namespace microfmt::log
