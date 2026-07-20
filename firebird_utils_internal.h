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

#ifndef FIREBIRD_UTILS_INTERNAL_H
#define FIREBIRD_UTILS_INTERNAL_H

#include <firebird/Interface.h>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include <string>
#include <array>
#include <algorithm>
#include <cassert>
#include <utility>
#include <tuple>

namespace {
    constexpr size_t DEFAULT_STATUS_VECTOR_SIZE = 20;

    constexpr void copy_status_vector(const ISC_STATUS* source, size_t source_size,
                                     ISC_STATUS* destination, size_t dest_size) noexcept {
        if (!source || !destination) return;

        // Use parentheses to prevent Windows min/max macro expansion
        const auto copy_count = (std::min)(source_size, dest_size);

        for (size_t i = 0; i < copy_count; ++i) {
            destination[i] = source[i];
            if (source[i] == isc_arg_end) break;
        }
    }
}

// RAII wrapper for Firebird IMaster interface
class FirebirdMasterWrapper {
private:
    Firebird::IMaster* master_;

public:
    explicit FirebirdMasterWrapper(void* master_ptr) noexcept
        : master_(static_cast<Firebird::IMaster*>(master_ptr)) {
        assert(master_ != nullptr && "Master pointer cannot be null");
    }

    // Non-copyable, movable
    FirebirdMasterWrapper(const FirebirdMasterWrapper&) = delete;
    FirebirdMasterWrapper& operator=(const FirebirdMasterWrapper&) = delete;
    FirebirdMasterWrapper(FirebirdMasterWrapper&&) = default;
    FirebirdMasterWrapper& operator=(FirebirdMasterWrapper&&) = default;

    Firebird::IUtil* getUtil() const noexcept {
        return master_->getUtilInterface();
    }

    Firebird::IMaster* getMaster() const noexcept {
        return master_;
    }

};

#if FB_API_VER >= 40
// RAII wrapper for exception-safe status handling
class FirebirdStatusManager {
private:
    ISC_STATUS* target_status_;
    std::unique_ptr<Firebird::ThrowStatusWrapper> status_wrapper_;
    Firebird::IStatus* raw_status_;

public:
    explicit FirebirdStatusManager(ISC_STATUS* target_status, Firebird::IMaster* master)
        : target_status_(target_status),
          raw_status_(master->getStatus()),
          status_wrapper_(std::make_unique<Firebird::ThrowStatusWrapper>(raw_status_)) {
        // CRITICAL: Clear status before any Firebird API call.
        // IMaster::getStatus() may return a reusable (cached) IStatus instance.
        // If previous calls populated errors, they can leak into subsequent calls
        // causing hasError()/hasData() to return true on stale state.
        // Fixes: tests/003.phpt (column deduplication aborted by stale validation error)
        if (raw_status_) {
            raw_status_->init();
        }
    }

    ~FirebirdStatusManager() {
        if (target_status_ && status_wrapper_->hasData()) {
            copy_status_safe(status_wrapper_->getErrors(), target_status_);
        }
        // Destroy wrapper before disposing IStatus (wrapper holds raw pointer).
        // Original code leaked the IStatus — this also fixes that leak.
        status_wrapper_.reset();
        if (raw_status_) {
            raw_status_->dispose();
        }
    }

    // Non-copyable, movable
    FirebirdStatusManager(const FirebirdStatusManager&) = delete;
    FirebirdStatusManager& operator=(const FirebirdStatusManager&) = delete;
    FirebirdStatusManager(FirebirdStatusManager&&) = default;
    FirebirdStatusManager& operator=(FirebirdStatusManager&&) = default;

    Firebird::ThrowStatusWrapper* get() noexcept {
        return status_wrapper_.get();
    }

    bool hasError() const noexcept {
        return status_wrapper_->hasData();
    }

private:
    void copy_status_safe(const ISC_STATUS* from, ISC_STATUS* to) noexcept {
        if (!from || !to) return;

        copy_status_vector(from, DEFAULT_STATUS_VECTOR_SIZE, to, DEFAULT_STATUS_VECTOR_SIZE);
    }
};

// RAII wrapper for Firebird metadata management
class FirebirdMetadataWrapper {
private:
    Firebird::IMessageMetadata* metadata_;
    bool owns_metadata_;

public:
    explicit FirebirdMetadataWrapper(Firebird::IMessageMetadata* meta, bool owns = false) noexcept
        : metadata_(meta), owns_metadata_(owns) {}

    ~FirebirdMetadataWrapper() {
        if (owns_metadata_ && metadata_) {
            metadata_->release();
        }
    }

    // Non-copyable, movable
    FirebirdMetadataWrapper(const FirebirdMetadataWrapper&) = delete;
    FirebirdMetadataWrapper& operator=(const FirebirdMetadataWrapper&) = delete;
    FirebirdMetadataWrapper(FirebirdMetadataWrapper&& other) noexcept
        : metadata_(std::exchange(other.metadata_, nullptr)),
          owns_metadata_(std::exchange(other.owns_metadata_, false)) {}

    Firebird::IMessageMetadata* get() const noexcept {
        return metadata_;
    }

    std::string_view getFieldName(Firebird::ThrowStatusWrapper* status, unsigned index) const {
        if (!metadata_ || !status) return {};

        try {
            const char* name = metadata_->getField(status, index);
            return name ? std::string_view(name) : std::string_view{};
        } catch (...) {
            return {};
        }
    }

    std::string_view getAlias(Firebird::ThrowStatusWrapper* status, unsigned index) const {
        if (!metadata_ || !status) return {};

        try {
            const char* alias = metadata_->getAlias(status, index);
            return alias ? std::string_view(alias) : std::string_view{};
        } catch (...) {
            return {};
        }
    }

    std::string_view getRelation(Firebird::ThrowStatusWrapper* status, unsigned index) const {
        if (!metadata_ || !status) return {};

        try {
            const char* relation = metadata_->getRelation(status, index);
            return relation ? std::string_view(relation) : std::string_view{};
        } catch (...) {
            return {};
        }
    }
};
#endif // FB_API_VER >= 40

#endif // FIREBIRD_UTILS_INTERNAL_H
