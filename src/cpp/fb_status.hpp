/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FB_STATUS_HPP
#define FB_STATUS_HPP

#include <firebird/Interface.h>
#include <ibase.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <cstring>
#include <array>

namespace fb {

/**
 * Status vector size constant (matches Firebird's ISC_STATUS_LENGTH)
 */
constexpr std::size_t STATUS_VECTOR_SIZE = 20;

/**
 * Check if IStatus has error data (FB 4.0+ compatible).
 * In FB 5.0+, hasData() is available. In FB 4.0, we use getState().
 * @param status The IStatus pointer to check
 * @return true if status contains error information
 */
inline bool statusHasError(Firebird::IStatus* status) noexcept {
    if (!status) return false;
    // FB 4.0 compatible: check state flags instead of hasData()
    // STATE_ERRORS (0x01) indicates errors, STATE_WARNINGS (0x02) indicates warnings
    return (status->getState() & Firebird::IStatus::STATE_ERRORS) != 0;
}

/**
 * Check if IStatus has any data (errors or warnings) - FB 4.0+ compatible.
 * @param status The IStatus pointer to check
 * @return true if status contains any information
 */
inline bool statusHasData(Firebird::IStatus* status) noexcept {
    if (!status) return false;
    // Check for either errors or warnings
    return (status->getState() & (Firebird::IStatus::STATE_ERRORS | Firebird::IStatus::STATE_WARNINGS)) != 0;
}

/**
 * Exception class for Firebird errors with rich status information.
 * Parses ISC_STATUS vectors into human-readable error messages.
 */
class Exception : public std::runtime_error {
public:
    /**
     * Construct from a simple message string.
     */
    explicit Exception(const std::string& message)
        : std::runtime_error(message),
          sqlcode_(0),
          gdscode_(0) {}

    /**
     * Construct from Firebird IStatus interface.
     * Extracts and formats all error information from the status vector.
     */
    explicit Exception(Firebird::IStatus* status)
        : std::runtime_error(formatStatus(status)),
          sqlcode_(extractSqlcode(status)),
          gdscode_(extractGdscode(status)) {
        if (statusHasData(status)) {
            copyStatusVector(status->getErrors());
        }
    }

    /**
     * Construct from legacy ISC_STATUS array.
     */
    explicit Exception(const ISC_STATUS* status_vector)
        : std::runtime_error(formatStatusVector(status_vector)),
          sqlcode_(extractSqlcodeFromVector(status_vector)),
          gdscode_(extractGdscodeFromVector(status_vector)) {
        if (status_vector) {
            copyStatusVector(status_vector);
        }
    }

    /**
     * Get the SQLCODE associated with this error.
     * @return SQLCODE value (negative for errors, 0 for success)
     */
    [[nodiscard]] ISC_LONG sqlcode() const noexcept { return sqlcode_; }

    /**
     * Get the primary GDS error code.
     * @return GDS error code (isc_* constant)
     */
    [[nodiscard]] ISC_LONG gdscode() const noexcept { return gdscode_; }

    /**
     * Get the raw status vector copy.
     * @return Pointer to internal status vector array
     */
    [[nodiscard]] const ISC_STATUS* statusVector() const noexcept {
        return status_vector_.data();
    }

private:
    ISC_LONG sqlcode_;
    ISC_LONG gdscode_;
    std::array<ISC_STATUS, STATUS_VECTOR_SIZE> status_vector_{};

    void copyStatusVector(const ISC_STATUS* src) noexcept {
        if (!src) return;
        for (std::size_t i = 0; i < STATUS_VECTOR_SIZE && src[i] != isc_arg_end; ++i) {
            status_vector_[i] = src[i];
        }
    }

    static std::string formatStatus(Firebird::IStatus* status) {
        if (!statusHasData(status)) {
            return "Unknown Firebird error";
        }
        return formatStatusVector(status->getErrors());
    }

    static std::string formatStatusVector(const ISC_STATUS* status_vector) {
        if (!status_vector) {
            return "Unknown Firebird error (null status vector)";
        }

        std::string result;
        result.reserve(512);  // Pre-allocate for typical error message

        // Buffer for fb_interpret (Firebird's error message function)
        constexpr std::size_t MSG_BUFFER_SIZE = 512;
        std::array<char, MSG_BUFFER_SIZE> msg_buffer{};

        const ISC_STATUS* status_ptr = status_vector;
        while (fb_interpret(msg_buffer.data(), MSG_BUFFER_SIZE, &status_ptr) != 0) {
            if (!result.empty()) {
                result += "\n";
            }
            result += msg_buffer.data();
        }

        if (result.empty()) {
            result = "Firebird error (no message available)";
        }

        return result;
    }

    static ISC_LONG extractSqlcode(Firebird::IStatus* status) noexcept {
        if (!statusHasData(status)) {
            return 0;
        }
        return extractSqlcodeFromVector(status->getErrors());
    }

    static ISC_LONG extractSqlcodeFromVector(const ISC_STATUS* status_vector) noexcept {
        if (!status_vector) {
            return 0;
        }
        return isc_sqlcode(status_vector);
    }

    static ISC_LONG extractGdscode(Firebird::IStatus* status) noexcept {
        if (!statusHasData(status)) {
            return 0;
        }
        return extractGdscodeFromVector(status->getErrors());
    }

    static ISC_LONG extractGdscodeFromVector(const ISC_STATUS* status_vector) noexcept {
        if (!status_vector) {
            return 0;
        }
        // GDS code is typically at position 1 in the status vector
        // Status vector format: [isc_arg_gds, gds_code, ...]
        if (status_vector[0] == isc_arg_gds) {
            return static_cast<ISC_LONG>(status_vector[1]);
        }
        return 0;
    }
};

/**
 * RAII wrapper for Firebird IStatus interface.
 * Automatically manages status lifecycle and provides convenient error checking.
 */
class StatusWrapper {
public:
    /**
     * Construct a StatusWrapper from IMaster.
     * Creates a new IStatus instance that will be disposed on destruction.
     */
    explicit StatusWrapper(Firebird::IMaster* master)
        : master_(master),
          status_(master ? master->getStatus() : nullptr),
          owns_status_(true) {}

    /**
     * Construct a StatusWrapper from existing IStatus.
     * @param status Existing status (ownership NOT transferred)
     */
    explicit StatusWrapper(Firebird::IStatus* status)
        : master_(nullptr),
          status_(status),
          owns_status_(false) {}

    ~StatusWrapper() {
        if (owns_status_ && status_) {
            status_->dispose();
        }
    }

    // Non-copyable
    StatusWrapper(const StatusWrapper&) = delete;
    StatusWrapper& operator=(const StatusWrapper&) = delete;

    // Movable
    StatusWrapper(StatusWrapper&& other) noexcept
        : master_(other.master_),
          status_(other.status_),
          owns_status_(other.owns_status_) {
        other.status_ = nullptr;
        other.owns_status_ = false;
    }

    StatusWrapper& operator=(StatusWrapper&& other) noexcept {
        if (this != &other) {
            if (owns_status_ && status_) {
                status_->dispose();
            }
            master_ = other.master_;
            status_ = other.status_;
            owns_status_ = other.owns_status_;
            other.status_ = nullptr;
            other.owns_status_ = false;
        }
        return *this;
    }

    /**
     * Get the raw IStatus pointer for Firebird API calls.
     */
    [[nodiscard]] Firebird::IStatus* get() noexcept { return status_; }
    [[nodiscard]] const Firebird::IStatus* get() const noexcept { return status_; }

    /**
     * Check if an error has occurred (FB 4.0+ compatible).
     */
    [[nodiscard]] bool hasError() const noexcept {
        return statusHasError(status_);
    }

    /**
     * Check if the status is clean (no error).
     */
    [[nodiscard]] bool isOk() const noexcept {
        return !hasError();
    }

    /**
     * Clear any error state.
     */
    void clear() noexcept {
        if (status_) {
            status_->init();
        }
    }

    /**
     * Throw an Exception if an error has occurred.
     */
    void checkAndThrow() {
        if (hasError()) {
            throw Exception(status_);
        }
    }

    /**
     * Copy the status vector to a legacy ISC_STATUS array.
     */
    void copyTo(ISC_STATUS* dest, std::size_t dest_size = STATUS_VECTOR_SIZE) const noexcept {
        if (!dest || dest_size == 0) return;

        if (statusHasData(status_)) {
            const ISC_STATUS* errors = status_->getErrors();
            for (std::size_t i = 0; i < dest_size && errors[i] != isc_arg_end; ++i) {
                dest[i] = errors[i];
            }
        } else {
            // Clear destination
            std::memset(dest, 0, dest_size * sizeof(ISC_STATUS));
        }
    }

private:
    Firebird::IMaster* master_;
    Firebird::IStatus* status_;
    bool owns_status_;
};

/**
 * RAII wrapper using ThrowStatusWrapper for automatic exception throwing.
 * Use this when you want exceptions thrown automatically on errors.
 */
class ThrowingStatusWrapper {
public:
    explicit ThrowingStatusWrapper(Firebird::IMaster* master)
        : status_(master ? master->getStatus() : nullptr),
          wrapper_(status_) {}

    ~ThrowingStatusWrapper() {
        if (status_) {
            status_->dispose();
        }
    }

    // Non-copyable, non-movable (due to ThrowStatusWrapper reference semantics)
    ThrowingStatusWrapper(const ThrowingStatusWrapper&) = delete;
    ThrowingStatusWrapper& operator=(const ThrowingStatusWrapper&) = delete;
    ThrowingStatusWrapper(ThrowingStatusWrapper&&) = delete;
    ThrowingStatusWrapper& operator=(ThrowingStatusWrapper&&) = delete;

    /**
     * Get the ThrowStatusWrapper for Firebird API calls.
     * This will automatically throw if an error occurs.
     */
    [[nodiscard]] Firebird::ThrowStatusWrapper* get() noexcept { return &wrapper_; }

private:
    Firebird::IStatus* status_;
    Firebird::ThrowStatusWrapper wrapper_;
};

/**
 * Check a status and throw if there's an error.
 * Utility function for quick error checking.
 */
inline void checkStatus(Firebird::IStatus* status) {
    if (statusHasError(status)) {
        throw Exception(status);
    }
}

/**
 * Check a legacy status vector and throw if there's an error.
 */
inline void checkStatusVector(const ISC_STATUS* status_vector) {
    if (status_vector && status_vector[0] == isc_arg_gds && status_vector[1] != 0) {
        throw Exception(status_vector);
    }
}

} // namespace fb

#endif // FB_STATUS_HPP
