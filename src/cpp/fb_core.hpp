/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FB_CORE_HPP
#define FB_CORE_HPP

#include <firebird/Interface.h>
#include <memory>
#include <string_view>
#include <vector>
#include <atomic>

namespace fb {

/**
 * Global shutdown flag to prevent use-after-free during process exit.
 * Set to true in PHP_RSHUTDOWN/PHP_MSHUTDOWN.
 */
extern std::atomic<bool> g_shutdown_active;
extern std::atomic<bool> g_process_exiting;

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
        // Skip release entirely. Firebird client library handles its own cleanup.
        (void)attachment;
    }
};

struct TransactionDeleter {
    void operator()(Firebird::ITransaction* transaction) const {
        // Skip release entirely.
        (void)transaction;
    }
};

struct StatementDeleter {
    void operator()(Firebird::IStatement* statement) const {
        // Skip release entirely.
        (void)statement;
    }
};

struct BlobDeleter {
    void operator()(Firebird::IBlob* blob) const {
        // Skip release entirely.
        (void)blob;
    }
};

struct ResultSetDeleter {
    void operator()(Firebird::IResultSet* rs) const {
        // Skip release entirely.
        (void)rs;
    }
};

struct ServiceDeleter {
    void operator()(Firebird::IService* service) const {
        // Skip release entirely.
        (void)service;
    }
};

} // namespace fb

#endif // FB_CORE_HPP
