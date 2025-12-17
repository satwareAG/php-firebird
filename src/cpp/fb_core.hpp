/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FB_CORE_HPP
#define FB_CORE_HPP

#include <firebird/Interface.h>
#include <memory>
#include <string_view>
#include <vector>

namespace fb {

/**
 * Get the global Firebird IMaster instance.
 * This function retrieves the master interface from the PHP extension globals (IBG).
 * It is implemented in firebird_utils.cpp to access PHP global state.
 *
 * @return Pointer to IMaster interface, or nullptr if not initialized
 */
Firebird::IMaster* getMaster() noexcept;


// Forward declarations
struct AttachmentDeleter;
struct TransactionDeleter;
struct StatementDeleter;
struct BlobDeleter;
struct ResultSetDeleter;
struct ServiceDeleter;

// Smart pointer types with custom deleters
using AttachmentPtr = std::unique_ptr<Firebird::IAttachment, AttachmentDeleter>;
using TransactionPtr = std::unique_ptr<Firebird::ITransaction, TransactionDeleter>;
using StatementPtr = std::unique_ptr<Firebird::IStatement, StatementDeleter>;
using BlobPtr = std::unique_ptr<Firebird::IBlob, BlobDeleter>;
using ResultSetPtr = std::unique_ptr<Firebird::IResultSet, ResultSetDeleter>;
using ServicePtr = std::unique_ptr<Firebird::IService, ServiceDeleter>;

// Deleter implementations
struct AttachmentDeleter {
    void operator()(Firebird::IAttachment* attachment) const {
        if (attachment) {
            // IAttachment usually requires status for detach,
            // but release() is always safe for ref-counting.
            // The Connection wrapper class should handle explicit detach().
            attachment->release();
        }
    }
};

struct TransactionDeleter {
    void operator()(Firebird::ITransaction* transaction) const {
        if (transaction) {
            // Similarly, rollback/commit usually requires status.
            // release() frees the interface handle.
            transaction->release();
        }
    }
};

struct StatementDeleter {
    void operator()(Firebird::IStatement* statement) const {
        if (statement) {
            statement->release();
        }
    }
};

struct BlobDeleter {
    void operator()(Firebird::IBlob* blob) const {
        if (blob) {
            blob->release();
        }
    }
};

struct ResultSetDeleter {
    void operator()(Firebird::IResultSet* rs) const {
        if (rs) {
            rs->release();
        }
    }
};

struct ServiceDeleter {
    void operator()(Firebird::IService* service) const {
        if (service) {
            service->release();
        }
    }
};

} // namespace fb

#endif // FB_CORE_HPP
