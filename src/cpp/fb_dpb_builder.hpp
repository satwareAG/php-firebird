/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FB_DPB_BUILDER_HPP
#define FB_DPB_BUILDER_HPP

#include <firebird/Interface.h>
#include <ibase.h>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <cstdint>
#include "fb_status.hpp"

namespace fb {

/**
 * Modern C++ builder for Database Parameter Block (DPB).
 * Uses the OO API's IXpbBuilder interface when available,
 * with fallback to manual buffer construction.
 *
 * Usage example:
 * @code
 *   DpbBuilder dpb(master);
 *   dpb.setUser("SYSDBA")
 *      .setPassword("masterkey")
 *      .setCharset("UTF8")
 *      .setRole("ADMIN")
 *      .setNumBuffers(2048);
 *
 *   auto* provider = master->getDispatcher();
 *   auto* attachment = provider->attachDatabase(&status, "test.fdb",
 *       dpb.getBufferLength(), dpb.getBuffer());
 * @endcode
 */
class DpbBuilder {
public:
    /**
     * Construct using IMaster to get IXpbBuilder.
     * @param master Firebird master interface
     */
    explicit DpbBuilder(Firebird::IMaster* master)
        : master_(master), builder_(nullptr), use_builder_(false) {
        if (master_) {
            // Try to create the modern XPB builder
            // Use CheckStatusWrapper as required by Firebird template API
            Firebird::IStatus* raw_status = master_->getStatus();
            Firebird::CheckStatusWrapper check_status(raw_status);
            builder_ = master_->getUtilInterface()->getXpbBuilder(
                &check_status, Firebird::IXpbBuilder::DPB, nullptr, 0);
            use_builder_ = (builder_ != nullptr && !check_status.isDirty());
        }

        if (!use_builder_) {
            // Fallback: start manual buffer with version byte
            buffer_.push_back(isc_dpb_version1);
        }
    }

    ~DpbBuilder() {
        if (builder_) {
            builder_->dispose();
        }
    }

    // Non-copyable (owns builder resource)
    DpbBuilder(const DpbBuilder&) = delete;
    DpbBuilder& operator=(const DpbBuilder&) = delete;

    // Movable
    DpbBuilder(DpbBuilder&& other) noexcept
        : master_(other.master_),
          builder_(other.builder_),
          buffer_(std::move(other.buffer_)),
          use_builder_(other.use_builder_) {
        other.builder_ = nullptr;
        other.use_builder_ = false;
    }

    DpbBuilder& operator=(DpbBuilder&& other) noexcept {
        if (this != &other) {
            if (builder_) builder_->dispose();
            master_ = other.master_;
            builder_ = other.builder_;
            buffer_ = std::move(other.buffer_);
            use_builder_ = other.use_builder_;
            other.builder_ = nullptr;
            other.use_builder_ = false;
        }
        return *this;
    }

    // -------------------------------------------------------------------------
    // Standard DPB Parameters
    // -------------------------------------------------------------------------

    /**
     * Set database username.
     */
    DpbBuilder& setUser(std::string_view user) {
        insertString(isc_dpb_user_name, user);
        return *this;
    }

    /**
     * Set database password.
     */
    DpbBuilder& setPassword(std::string_view password) {
        insertString(isc_dpb_password, password);
        return *this;
    }

    /**
     * Set connection character set.
     */
    DpbBuilder& setCharset(std::string_view charset) {
        insertString(isc_dpb_lc_ctype, charset);
        return *this;
    }

    /**
     * Set SQL role name.
     */
    DpbBuilder& setRole(std::string_view role) {
        insertString(isc_dpb_sql_role_name, role);
        return *this;
    }

    /**
     * Set number of database page buffers.
     */
    DpbBuilder& setNumBuffers(unsigned short buffers) {
        insertShort(isc_dpb_num_buffers, buffers);
        return *this;
    }

    /**
     * Set forced write mode (synchronous writes).
     */
    DpbBuilder& setForceWrite(bool enable) {
        insertByte(isc_dpb_force_write, enable ? 1 : 0);
        return *this;
    }

    /**
     * Set SQL dialect.
     */
    DpbBuilder& setDialect(unsigned short dialect) {
        insertByte(isc_dpb_sql_dialect, static_cast<unsigned char>(dialect));
        return *this;
    }

    /**
     * Set connection timeout in seconds.
     */
    DpbBuilder& setConnectTimeout(unsigned int seconds) {
        insertInt(isc_dpb_connect_timeout, seconds);
        return *this;
    }

    /**
     * Set process ID (for monitoring).
     */
    DpbBuilder& setProcessId(int pid) {
        insertInt(isc_dpb_process_id, pid);
        return *this;
    }

    /**
     * Set process name (for monitoring).
     */
    DpbBuilder& setProcessName(std::string_view name) {
        insertString(isc_dpb_process_name, name);
        return *this;
    }

    // -------------------------------------------------------------------------
    // FB 4.0+ Parameters
    // -------------------------------------------------------------------------

#ifdef isc_dpb_set_bind
    /**
     * Set data type binding rules (FB 4.0+).
     * Example: "decfloat to varchar;int128 to varchar"
     */
    DpbBuilder& setBindRules(std::string_view rules) {
        insertString(isc_dpb_set_bind, rules);
        return *this;
    }
#endif

#ifdef isc_dpb_session_time_zone
    /**
     * Set session time zone (FB 4.0+).
     * Example: "Europe/Berlin" or "+02:00"
     */
    DpbBuilder& setSessionTimeZone(std::string_view tz) {
        insertString(isc_dpb_session_time_zone, tz);
        return *this;
    }
#endif

    // -------------------------------------------------------------------------
    // Buffer Access
    // -------------------------------------------------------------------------

    /**
     * Get the DPB buffer for use with attachDatabase.
     */
    [[nodiscard]] const unsigned char* getBuffer() const noexcept {
        if (use_builder_ && builder_ && master_) {
            Firebird::IStatus* raw_status = master_->getStatus();
            Firebird::CheckStatusWrapper check_status(raw_status);
            return builder_->getBuffer(&check_status);
        }
        return buffer_.empty() ? nullptr : buffer_.data();
    }

    /**
     * Get the DPB buffer length.
     */
    [[nodiscard]] unsigned int getBufferLength() const noexcept {
        if (use_builder_ && builder_ && master_) {
            Firebird::IStatus* raw_status = master_->getStatus();
            Firebird::CheckStatusWrapper check_status(raw_status);
            return builder_->getBufferLength(&check_status);
        }
        return static_cast<unsigned int>(buffer_.size());
    }

    /**
     * Check if the builder is valid.
     */
    [[nodiscard]] bool isValid() const noexcept {
        return use_builder_ ? (builder_ != nullptr) : !buffer_.empty();
    }

    /**
     * Clear all parameters and reset to initial state.
     * Note: IXpbBuilder::clear() requires CheckStatusWrapper for FB template API
     */
    void clear() {
        if (use_builder_ && builder_ && master_) {
            // FB 4.0 compatible: use CheckStatusWrapper for template API
            Firebird::IStatus* raw_status = master_->getStatus();
            Firebird::CheckStatusWrapper check_status(raw_status);
            builder_->clear(&check_status);
        }
        buffer_.clear();
        buffer_.push_back(isc_dpb_version1);
    }

private:
    Firebird::IMaster* master_;
    Firebird::IXpbBuilder* builder_;
    std::vector<unsigned char> buffer_;
    bool use_builder_;

    // Insert a string parameter
    void insertString(unsigned char tag, std::string_view value) {
        if (use_builder_ && builder_ && master_) {
            Firebird::IStatus* raw_status = master_->getStatus();
            Firebird::CheckStatusWrapper check_status(raw_status);
            builder_->insertString(&check_status, tag, value.data());
        } else {
            // Manual buffer construction
            if (value.length() > 255) {
                // Truncate to max DPB string length
                value = value.substr(0, 255);
            }
            buffer_.push_back(tag);
            buffer_.push_back(static_cast<unsigned char>(value.length()));
            buffer_.insert(buffer_.end(), value.begin(), value.end());
        }
    }

    // Insert a single byte parameter
    void insertByte(unsigned char tag, unsigned char value) {
        if (use_builder_ && builder_ && master_) {
            Firebird::IStatus* raw_status = master_->getStatus();
            Firebird::CheckStatusWrapper check_status(raw_status);
            builder_->insertInt(&check_status, tag, value);
        } else {
            buffer_.push_back(tag);
            buffer_.push_back(1); // length
            buffer_.push_back(value);
        }
    }

    // Insert a short (2-byte) parameter
    void insertShort(unsigned char tag, unsigned short value) {
        if (use_builder_ && builder_ && master_) {
            Firebird::IStatus* raw_status = master_->getStatus();
            Firebird::CheckStatusWrapper check_status(raw_status);
            builder_->insertInt(&check_status, tag, value);
        } else {
            buffer_.push_back(tag);
            buffer_.push_back(2); // length
            buffer_.push_back(static_cast<unsigned char>(value & 0xFF));
            buffer_.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
        }
    }

    // Insert an int (4-byte) parameter
    void insertInt(unsigned char tag, unsigned int value) {
        if (use_builder_ && builder_ && master_) {
            Firebird::IStatus* raw_status = master_->getStatus();
            Firebird::CheckStatusWrapper check_status(raw_status);
            builder_->insertInt(&check_status, tag, static_cast<int>(value));
        } else {
            buffer_.push_back(tag);
            buffer_.push_back(4); // length
            buffer_.push_back(static_cast<unsigned char>(value & 0xFF));
            buffer_.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
            buffer_.push_back(static_cast<unsigned char>((value >> 16) & 0xFF));
            buffer_.push_back(static_cast<unsigned char>((value >> 24) & 0xFF));
        }
    }
};

/**
 * Helper to build a standard connection DPB with common parameters.
 */
[[nodiscard]] inline DpbBuilder createConnectionDpb(
    Firebird::IMaster* master,
    std::string_view user = {},
    std::string_view password = {},
    std::string_view charset = {},
    std::string_view role = {},
    unsigned short dialect = 3,
    std::optional<unsigned short> num_buffers = std::nullopt) {

    DpbBuilder dpb(master);

    if (!user.empty()) {
        dpb.setUser(user);
    }
    if (!password.empty()) {
        dpb.setPassword(password);
    }
    if (!charset.empty()) {
        dpb.setCharset(charset);
    }
    if (!role.empty()) {
        dpb.setRole(role);
    }
    if (dialect > 0) {
        dpb.setDialect(dialect);
    }
    if (num_buffers.has_value()) {
        dpb.setNumBuffers(num_buffers.value());
    }

    return dpb;
}

} // namespace fb

#endif // FB_DPB_BUILDER_HPP
