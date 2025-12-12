/*
  +----------------------------------------------------------------------+
  | Copyright (c) The PHP Group                                          |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | https://www.php.net/license/3_01.txt                                 |
  +----------------------------------------------------------------------+
*/

#ifndef FB_TPB_BUILDER_HPP
#define FB_TPB_BUILDER_HPP

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
 * Transaction access mode.
 */
enum class TransactionAccess : unsigned char {
    ReadWrite,  // Default - allows modifications
    ReadOnly    // Read-only transaction
};

/**
 * Transaction isolation level.
 */
enum class IsolationLevel : unsigned char {
    Concurrency,    // Default - snapshot isolation (isc_tpb_concurrency)
    Consistency,    // Table-level consistency (isc_tpb_consistency)
    ReadCommitted,  // Read committed - sees committed changes
    Snapshot        // Alias for Concurrency (backward compatibility)
};

/**
 * Read committed record version mode.
 */
enum class RecordVersionMode : unsigned char {
    RecordVersion,    // isc_tpb_rec_version - can read uncommitted versions
    NoRecordVersion   // isc_tpb_no_rec_version - only committed versions
};

/**
 * Wait mode for lock conflicts.
 */
enum class WaitMode : unsigned char {
    Wait,   // Wait for locks (default)
    NoWait  // Return error immediately on lock conflict
};

/**
 * Table reservation lock mode.
 */
enum class TableLockMode : unsigned char {
    Shared,     // Shared read lock
    Protected,  // Protected read lock
    Exclusive   // Exclusive write lock
};

/**
 * Table reservation access type.
 */
enum class TableLockAccess : unsigned char {
    Read,   // Read access to table
    Write   // Write access to table
};

/**
 * Modern C++ builder for Transaction Parameter Block (TPB).
 * Uses the OO API's IXpbBuilder interface when available,
 * with fallback to manual buffer construction.
 *
 * Usage example:
 * @code
 *   TpbBuilder tpb(master);
 *   tpb.setIsolation(IsolationLevel::ReadCommitted)
 *      .setAccess(TransactionAccess::ReadOnly)
 *      .setWaitMode(WaitMode::NoWait)
 *      .setLockTimeout(5);  // 5 seconds
 *
 *   auto* transaction = attachment->startTransaction(&status,
 *       tpb.getBufferLength(), tpb.getBuffer());
 * @endcode
 */
class TpbBuilder {
public:
    /**
     * Construct using IMaster to get IXpbBuilder.
     * @param master Firebird master interface
     */
    explicit TpbBuilder(Firebird::IMaster* master)
        : master_(master), builder_(nullptr), use_builder_(false) {
        if (master_) {
            // Try to create the modern XPB builder
            StatusWrapper status(master_);
            builder_ = master_->getUtilInterface()->getXpbBuilder(
                status.get(), Firebird::IXpbBuilder::TPB, nullptr, 0);
            use_builder_ = (builder_ != nullptr && status.isOk());
        }

        if (!use_builder_) {
            // Fallback: start manual buffer with version byte
            buffer_.push_back(isc_tpb_version3);
        }
    }

    ~TpbBuilder() {
        if (builder_) {
            builder_->dispose();
        }
    }

    // Non-copyable (owns builder resource)
    TpbBuilder(const TpbBuilder&) = delete;
    TpbBuilder& operator=(const TpbBuilder&) = delete;

    // Movable
    TpbBuilder(TpbBuilder&& other) noexcept
        : master_(other.master_),
          builder_(other.builder_),
          buffer_(std::move(other.buffer_)),
          use_builder_(other.use_builder_),
          wait_mode_set_(other.wait_mode_set_) {
        other.builder_ = nullptr;
        other.use_builder_ = false;
    }

    TpbBuilder& operator=(TpbBuilder&& other) noexcept {
        if (this != &other) {
            if (builder_) builder_->dispose();
            master_ = other.master_;
            builder_ = other.builder_;
            buffer_ = std::move(other.buffer_);
            use_builder_ = other.use_builder_;
            wait_mode_set_ = other.wait_mode_set_;
            other.builder_ = nullptr;
            other.use_builder_ = false;
        }
        return *this;
    }

    // -------------------------------------------------------------------------
    // Transaction Parameters
    // -------------------------------------------------------------------------

    /**
     * Set transaction access mode (read-write or read-only).
     */
    TpbBuilder& setAccess(TransactionAccess access) {
        unsigned char tag = (access == TransactionAccess::ReadOnly)
            ? isc_tpb_read : isc_tpb_write;
        insertTag(tag);
        return *this;
    }

    /**
     * Set transaction isolation level.
     */
    TpbBuilder& setIsolation(IsolationLevel level) {
        unsigned char tag;
        switch (level) {
            case IsolationLevel::Consistency:
                tag = isc_tpb_consistency;
                break;
            case IsolationLevel::ReadCommitted:
                tag = isc_tpb_read_committed;
                break;
            case IsolationLevel::Concurrency:
            case IsolationLevel::Snapshot:
            default:
                tag = isc_tpb_concurrency;
                break;
        }
        insertTag(tag);
        return *this;
    }

    /**
     * Set record version mode for READ COMMITTED isolation.
     * Only meaningful when using IsolationLevel::ReadCommitted.
     */
    TpbBuilder& setRecordVersionMode(RecordVersionMode mode) {
        unsigned char tag = (mode == RecordVersionMode::RecordVersion)
            ? isc_tpb_rec_version : isc_tpb_no_rec_version;
        insertTag(tag);
        return *this;
    }

    /**
     * Set wait mode for lock conflicts.
     */
    TpbBuilder& setWaitMode(WaitMode mode) {
        unsigned char tag = (mode == WaitMode::NoWait)
            ? isc_tpb_nowait : isc_tpb_wait;
        insertTag(tag);
        wait_mode_set_ = true;
        return *this;
    }

    /**
     * Set lock timeout in seconds.
     * Automatically enables WAIT mode if not already set.
     * @param seconds Lock timeout (0 = infinite wait)
     */
    TpbBuilder& setLockTimeout(unsigned short seconds) {
        // Lock timeout requires WAIT mode
        if (!wait_mode_set_) {
            setWaitMode(WaitMode::Wait);
        }

        if (use_builder_ && builder_) {
            StatusWrapper status(master_);
            builder_->insertInt(status.get(), isc_tpb_lock_timeout, seconds);
        } else {
            buffer_.push_back(isc_tpb_lock_timeout);
            buffer_.push_back(sizeof(ISC_SHORT)); // length
            buffer_.push_back(static_cast<unsigned char>(seconds & 0xFF));
            buffer_.push_back(static_cast<unsigned char>((seconds >> 8) & 0xFF));
        }
        return *this;
    }

    // -------------------------------------------------------------------------
    // FB 4.0+ Parameters
    // -------------------------------------------------------------------------

#ifdef isc_tpb_read_consistency
    /**
     * Enable read consistency mode (FB 4.0+).
     * Provides consistent snapshot views in READ COMMITTED transactions.
     */
    TpbBuilder& setReadConsistency(bool enable = true) {
        if (enable) {
            if (use_builder_ && builder_) {
                StatusWrapper status(master_);
                builder_->insertInt(status.get(), isc_tpb_read_consistency, 1);
            } else {
                buffer_.push_back(isc_tpb_read_consistency);
                buffer_.push_back(1); // length
                buffer_.push_back(1); // value
            }
        }
        return *this;
    }
#endif

#ifdef isc_tpb_at_snapshot_number
    /**
     * Set snapshot number for SNAPSHOT AT transaction (FB 4.0+).
     */
    TpbBuilder& setSnapshotNumber(ISC_INT64 snapshot) {
        if (use_builder_ && builder_) {
            StatusWrapper status(master_);
            builder_->insertBigInt(status.get(), isc_tpb_at_snapshot_number, snapshot);
        } else {
            buffer_.push_back(isc_tpb_at_snapshot_number);
            buffer_.push_back(8); // length for 64-bit
            for (int i = 0; i < 8; ++i) {
                buffer_.push_back(static_cast<unsigned char>((snapshot >> (i * 8)) & 0xFF));
            }
        }
        return *this;
    }
#endif

    // -------------------------------------------------------------------------
    // Table Reservation (Lock Specifics)
    // -------------------------------------------------------------------------

    /**
     * Add table reservation to the transaction.
     * @param tableName Name of the table to reserve
     * @param lockMode Lock mode (Shared, Protected, Exclusive)
     * @param access Access type (Read, Write)
     */
    TpbBuilder& reserveTable(std::string_view tableName,
                              TableLockMode lockMode,
                              TableLockAccess access) {
        // Access type
        unsigned char accessTag = (access == TableLockAccess::Write)
            ? isc_tpb_lock_write : isc_tpb_lock_read;

        if (use_builder_ && builder_) {
            StatusWrapper status(master_);
            builder_->insertString(status.get(), accessTag, tableName.data());
        } else {
            buffer_.push_back(accessTag);
            auto len = static_cast<unsigned char>(std::min(tableName.length(),
                                                           static_cast<size_t>(255)));
            buffer_.push_back(len);
            buffer_.insert(buffer_.end(), tableName.begin(),
                          tableName.begin() + len);
        }

        // Lock mode
        unsigned char modeTag;
        switch (lockMode) {
            case TableLockMode::Protected:
                modeTag = isc_tpb_protected;
                break;
            case TableLockMode::Exclusive:
                modeTag = isc_tpb_exclusive;
                break;
            case TableLockMode::Shared:
            default:
                modeTag = isc_tpb_shared;
                break;
        }
        insertTag(modeTag);

        return *this;
    }

    // -------------------------------------------------------------------------
    // Buffer Access
    // -------------------------------------------------------------------------

    /**
     * Get the TPB buffer for use with startTransaction.
     */
    [[nodiscard]] const unsigned char* getBuffer() const noexcept {
        if (use_builder_ && builder_) {
            StatusWrapper status(master_);
            return builder_->getBuffer(status.get());
        }
        return buffer_.empty() ? nullptr : buffer_.data();
    }

    /**
     * Get the TPB buffer length.
     */
    [[nodiscard]] unsigned int getBufferLength() const noexcept {
        if (use_builder_ && builder_) {
            StatusWrapper status(master_);
            return builder_->getBufferLength(status.get());
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
     * Note: IXpbBuilder::clear() requires IStatus* in FB 4.0+
     */
    void clear() {
        if (use_builder_ && builder_ && master_) {
            // FB 4.0 compatible: clear() requires a status parameter
            StatusWrapper status(master_);
            builder_->clear(status.get());
        }
        buffer_.clear();
        buffer_.push_back(isc_tpb_version3);
        wait_mode_set_ = false;
    }

private:
    Firebird::IMaster* master_;
    Firebird::IXpbBuilder* builder_;
    std::vector<unsigned char> buffer_;
    bool use_builder_;
    bool wait_mode_set_ = false;

    // Insert a simple tag (no value)
    void insertTag(unsigned char tag) {
        if (use_builder_ && builder_) {
            StatusWrapper status(master_);
            builder_->insertTag(status.get(), tag);
        } else {
            buffer_.push_back(tag);
        }
    }
};

/**
 * Helper to build a standard transaction TPB with common parameters.
 */
[[nodiscard]] inline TpbBuilder createStandardTpb(
    Firebird::IMaster* master,
    IsolationLevel isolation = IsolationLevel::Concurrency,
    TransactionAccess access = TransactionAccess::ReadWrite,
    WaitMode waitMode = WaitMode::Wait,
    std::optional<unsigned short> lockTimeout = std::nullopt) {

    TpbBuilder tpb(master);

    tpb.setAccess(access);
    tpb.setIsolation(isolation);
    tpb.setWaitMode(waitMode);

    if (lockTimeout.has_value()) {
        tpb.setLockTimeout(lockTimeout.value());
    }

    return tpb;
}

/**
 * Helper to build a READ COMMITTED transaction TPB.
 */
[[nodiscard]] inline TpbBuilder createReadCommittedTpb(
    Firebird::IMaster* master,
    RecordVersionMode versionMode = RecordVersionMode::RecordVersion,
    TransactionAccess access = TransactionAccess::ReadWrite,
    std::optional<unsigned short> lockTimeout = std::nullopt) {

    TpbBuilder tpb(master);

    tpb.setAccess(access);
    tpb.setIsolation(IsolationLevel::ReadCommitted);
    tpb.setRecordVersionMode(versionMode);

    if (lockTimeout.has_value()) {
        tpb.setLockTimeout(lockTimeout.value());
    }

    return tpb;
}

/**
 * Helper to build a read-only snapshot transaction TPB.
 */
[[nodiscard]] inline TpbBuilder createReadOnlySnapshotTpb(
    Firebird::IMaster* master) {

    TpbBuilder tpb(master);
    tpb.setAccess(TransactionAccess::ReadOnly);
    tpb.setIsolation(IsolationLevel::Snapshot);
    return tpb;
}

} // namespace fb

#endif // FB_TPB_BUILDER_HPP
