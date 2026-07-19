/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FB_TRANSACTION_HPP
#define FB_TRANSACTION_HPP

/**
 * @file fb_transaction.hpp
 * @brief RAII Transaction wrapper for Firebird OO API
 *
 * This file provides a modern C++ wrapper for database transactions using
 * the Firebird C++ Object-Oriented API (ITransaction interface). It replaces
 * the legacy isc_start_transaction() / isc_commit_transaction() / isc_rollback_transaction().
 *
 * Features:
 * - RAII resource management with automatic rollback on destruction
 * - Commit/rollback with optional retaining mode
 * - C interop functions for gradual migration from firebird.c
 *
 * @see MODERNIZATION_PLAN_FB3_TO_FB5.md Phase 4: Transaction Layer
 */

#include <firebird/Interface.h>
#include <ibase.h>
#include <cstdint>

#include "fb_core.hpp"
#include "fb_status.hpp"

namespace fb {

/**
 * RAII wrapper for Firebird database transactions using the OO API.
 *
 * This class encapsulates an ITransaction handle and provides:
 * - Automatic resource cleanup via RAII (rollback on destruction if not committed)
 * - Commit and rollback operations with retaining variants
 * - Status preservation for PHP error reporting
 *
 * Usage example:
 * @code
 *   auto* attachment = conn.get();  // IAttachment*
 *   auto* master = getMaster();
 *
 *   Transaction trans = Transaction::start(master, attachment);
 *   // ... perform operations ...
 *   trans.commit();
 * @endcode
 */
class Transaction {
public:
    /**
     * Start a new transaction on the given attachment.
     *
     * @param master Firebird master interface
     * @param attachment Database attachment (from Connection::get())
     * @param tpbLength Length of TPB buffer (0 for default)
     * @param tpb Transaction parameter buffer (nullptr for default)
     * @return Transaction object (throws fb::Exception on failure)
     * @throws fb::Exception if transaction start fails
     */
    [[nodiscard]] static Transaction start(
        Firebird::IMaster* master,
        Firebird::IAttachment* attachment,
        unsigned tpbLength = 0,
        const unsigned char* tpb = nullptr);

    /**
     * Create a Transaction wrapper from an existing ITransaction pointer.
     * Used for reconnecting to limbo transactions.
     *
     * @param master Firebird master interface
     * @param transaction Existing ITransaction pointer (ownership transferred)
     * @return Transaction object owning the given transaction
     */
    [[nodiscard]] static Transaction fromRaw(
        Firebird::IMaster* master,
        Firebird::ITransaction* transaction) noexcept;

    /**
     * Default constructor creates an invalid (empty) transaction.
     */
    Transaction() noexcept = default;

    /**
     * Destructor - automatically rolls back if not committed/rolled back.
     */
    ~Transaction();

    // Non-copyable (transaction handles are unique)
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    // Movable
    Transaction(Transaction&& other) noexcept;
    Transaction& operator=(Transaction&& other) noexcept;

    /**
     * Check if the transaction is active (started and not ended).
     */
    [[nodiscard]] bool isActive() const noexcept {
        return transaction_ != nullptr;
    }

    /**
     * Get the raw ITransaction pointer (for Firebird API calls).
     * @warning Do not release() this pointer; Transaction owns it.
     */
    [[nodiscard]] Firebird::ITransaction* get() noexcept {
        return transaction_.get();
    }

    /**
     * Commit the transaction.
     * After this call, isActive() returns false.
     *
     * @throws fb::Exception if commit fails
     */
    void commit();

    /**
     * Rollback the transaction.
     * After this call, isActive() returns false.
     *
     * @throws fb::Exception if rollback fails
     */
    void rollback();

    /**
     * Commit the transaction but retain the transaction context.
     * After this call, isActive() remains true and the transaction can continue.
     *
     * @throws fb::Exception if commit fails
     */
    void commitRetaining();

    /**
     * Rollback the transaction but retain the transaction context.
     * After this call, isActive() remains true and the transaction can continue.
     *
     * @throws fb::Exception if rollback fails
     */
    void rollbackRetaining();

    /**
     * Rollback without throwing exceptions (for use in destructors).
     * @return true if rollback succeeded, false otherwise
     */
    bool rollbackNoThrow() noexcept;

    /**
     * Copy the last error status vector to a legacy ISC_STATUS array.
     * Used for integration with existing PHP error handling.
     *
     * @param dest Destination status vector
     * @param dest_size Size of destination array
     */
    void copyLastStatus(ISC_STATUS* dest, std::size_t dest_size = STATUS_VECTOR_SIZE) const noexcept;

private:
    /**
     * Private constructor from transaction pointer.
     */
    explicit Transaction(TransactionPtr transaction, Firebird::IMaster* master);

    TransactionPtr transaction_;            ///< RAII-managed transaction
    Firebird::IMaster* master_ = nullptr;   ///< Master interface (not owned)
    mutable StatusWrapper last_status_{static_cast<Firebird::IStatus*>(nullptr)}; ///< Last error status
};

// =============================================================================
// Implementation
// =============================================================================

inline Transaction Transaction::start(
    Firebird::IMaster* master,
    Firebird::IAttachment* attachment,
    unsigned tpbLength,
    const unsigned char* tpb)
{
    if (!master) {
        throw Exception("Firebird master interface is null");
    }
    if (!attachment) {
        throw Exception("Attachment is null - cannot start transaction");
    }

    // Create CheckStatusWrapper for start operation
    Firebird::IStatus* raw_status = master->getStatus();
    Firebird::CheckStatusWrapper check_status(raw_status);

    // Start transaction using OO API
    Firebird::ITransaction* raw_transaction = attachment->startTransaction(
        &check_status,
        tpbLength,
        tpb
    );

    if (fb::statusHasError(raw_status) || !raw_transaction) {
        throw Exception(raw_status);
    }

    // Wrap in RAII pointer
    TransactionPtr transaction(raw_transaction);

    return Transaction(std::move(transaction), master);
}

inline Transaction::Transaction(TransactionPtr transaction, Firebird::IMaster* master)
    : transaction_(std::move(transaction)),
      master_(master),
      last_status_(master) {
}

inline Transaction Transaction::fromRaw(
    Firebird::IMaster* master,
    Firebird::ITransaction* transaction) noexcept
{
    if (!transaction) {
        return Transaction();
    }
    TransactionPtr ptr(transaction, TransactionDeleter{});
    Transaction result;
    result.transaction_ = std::move(ptr);
    result.master_ = master;
    if (master) {
        result.last_status_ = StatusWrapper(master);
    }
    return result;
}

inline Transaction::~Transaction() {
    rollbackNoThrow();
}

inline Transaction::Transaction(Transaction&& other) noexcept
    : transaction_(std::move(other.transaction_)),
      master_(other.master_),
      last_status_(std::move(other.last_status_)) {
    other.master_ = nullptr;
}

inline Transaction& Transaction::operator=(Transaction&& other) noexcept {
    if (this != &other) {
        rollbackNoThrow();
        transaction_ = std::move(other.transaction_);
        master_ = other.master_;
        last_status_ = std::move(other.last_status_);
        other.master_ = nullptr;
    }
    return *this;
}

inline void Transaction::commit() {
    if (!transaction_) {
        return;
    }

    /* Issue #295: Use getMaster() instead of cached master_ to detect MSHUTDOWN.
     * getMaster() returns nullptr when IBG(in_mshutdown) is set, preventing a
     * server-side ITransaction::commit() on a dead attachment during cleanup.
     * Mirrors the 881d375 fix for Connection::detachNoThrow(). */
    Firebird::IMaster* master = getMaster();
    if (!master) {
        transaction_.reset();
        return;
    }

    Firebird::IStatus* raw_status = master->getStatus();
    Firebird::CheckStatusWrapper check_status(raw_status);
    transaction_->commit(&check_status);

    if (fb::statusHasError(raw_status)) {
        last_status_ = StatusWrapper(master);
        last_status_.get()->setErrors(raw_status->getErrors());
        throw Exception(raw_status);
    }

    transaction_.reset();
}

inline void Transaction::rollback() {
    if (!transaction_) {
        return;
    }

    /* Issue #295: Use getMaster() instead of cached master_ (mirrors 881d375). */
    Firebird::IMaster* master = getMaster();
    if (!master) {
        transaction_.reset();
        return;
    }

    Firebird::IStatus* raw_status = master->getStatus();
    Firebird::CheckStatusWrapper check_status(raw_status);
    transaction_->rollback(&check_status);

    if (fb::statusHasError(raw_status)) {
        last_status_ = StatusWrapper(master);
        last_status_.get()->setErrors(raw_status->getErrors());
        throw Exception(raw_status);
    }

    transaction_.reset();
}

inline void Transaction::commitRetaining() {
    if (!transaction_) {
        throw Exception("Transaction is not active");
    }

    if (!master_) {
        throw Exception("Master interface not available");
    }

    Firebird::IStatus* raw_status = master_->getStatus();
    Firebird::CheckStatusWrapper check_status(raw_status);
    transaction_->commitRetaining(&check_status);

    if (fb::statusHasError(raw_status)) {
        last_status_ = StatusWrapper(master_);
        last_status_.get()->setErrors(raw_status->getErrors());
        throw Exception(raw_status);
    }
    // Transaction remains active after retaining commit
}

inline void Transaction::rollbackRetaining() {
    if (!transaction_) {
        throw Exception("Transaction is not active");
    }

    if (!master_) {
        throw Exception("Master interface not available");
    }

    Firebird::IStatus* raw_status = master_->getStatus();
    Firebird::CheckStatusWrapper check_status(raw_status);
    transaction_->rollbackRetaining(&check_status);

    if (fb::statusHasError(raw_status)) {
        last_status_ = StatusWrapper(master_);
        last_status_.get()->setErrors(raw_status->getErrors());
        throw Exception(raw_status);
    }
    // Transaction remains active after retaining rollback
}

inline bool Transaction::rollbackNoThrow() noexcept {
    if (!transaction_) {
        return true;
    }

    try {
        /* Issue #295: Use getMaster() instead of cached master_ (mirrors 881d375). */
        Firebird::IMaster* master = getMaster();
        if (master) {
            Firebird::IStatus* raw_status = master->getStatus();
            Firebird::CheckStatusWrapper check_status(raw_status);
            transaction_->rollback(&check_status);
            if (fb::statusHasError(raw_status)) {
                transaction_.reset();
                return false;
            }
        }
        transaction_.reset();
        return true;
    } catch (...) {
        transaction_.reset();
        return false;
    }
}

inline void Transaction::copyLastStatus(ISC_STATUS* dest, std::size_t dest_size) const noexcept {
    last_status_.copyTo(dest, dest_size);
}

} // namespace fb

// =============================================================================
// C Interop Functions (extern "C")
// =============================================================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start a transaction using the OO API.
 * This is the primary C interop function for transaction creation.
 *
 * @param master_ptr Pointer to IMaster interface
 * @param attachment_ptr Pointer to IAttachment interface (from fbc_get_attachment())
 * @param tpb_len Length of TPB buffer
 * @param tpb Transaction parameter buffer (may be NULL for defaults)
 * @param status_vector Output status vector for errors
 * @return Pointer to fb::Transaction object, or NULL on failure
 */
void* fbt_start(
    void* master_ptr,
    void* attachment_ptr,
    unsigned tpb_len,
    const unsigned char* tpb,
    ISC_STATUS* status_vector
);

/**
 * Commit a transaction created with fbt_start().
 *
 * @param transaction Pointer returned by fbt_start()
 * @param status_vector Output status vector for errors
 * @return 0 on success, non-zero on failure
 */
int fbt_commit(void* transaction, ISC_STATUS* status_vector);

/**
 * Rollback a transaction created with fbt_start().
 *
 * @param transaction Pointer returned by fbt_start()
 * @param status_vector Output status vector for errors
 * @return 0 on success, non-zero on failure
 */
int fbt_rollback(void* transaction, ISC_STATUS* status_vector);

/**
 * Commit with retaining (keeps transaction context).
 *
 * @param transaction Pointer returned by fbt_start()
 * @param status_vector Output status vector for errors
 * @return 0 on success, non-zero on failure
 */
int fbt_commit_retaining(void* transaction, ISC_STATUS* status_vector);

/**
 * Rollback with retaining (keeps transaction context).
 *
 * @param transaction Pointer returned by fbt_start()
 * @param status_vector Output status vector for errors
 * @return 0 on success, non-zero on failure
 */
int fbt_rollback_retaining(void* transaction, ISC_STATUS* status_vector);

/**
 * Check if a transaction is active.
 *
 * @param transaction Pointer returned by fbt_start()
 * @return 1 if active, 0 if not
 */
int fbt_is_active(void* transaction);

/**
 * Get the ITransaction pointer from a transaction wrapper.
 *
 * @param transaction Pointer returned by fbt_start()
 * @return Raw ITransaction pointer, or NULL
 */
void* fbt_get_handle(void* transaction);

/**
 * Free a transaction wrapper without commit/rollback (for abnormal cleanup).
 * Use only when the transaction was already ended via other means.
 *
 * @param transaction Pointer returned by fbt_start()
 */
void fbt_free(void* transaction);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // FB_TRANSACTION_HPP
