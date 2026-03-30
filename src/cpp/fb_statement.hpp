/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FB_STATEMENT_HPP
#define FB_STATEMENT_HPP

#include <firebird/Interface.h>
#include <ibase.h>
#include <cstring>
#include "fb_status.hpp"  // For statusHasError()

namespace fb {

/**
 * @brief Copy status errors from IStatus to ISC_STATUS array
 */
inline void copyStatusToVector(Firebird::IStatus* status, ISC_STATUS* dest) noexcept {
    if (!dest) return;

    // Clear destination first
    std::memset(dest, 0, ISC_STATUS_LENGTH * sizeof(ISC_STATUS));

    if (status) {
        const ISC_STATUS* errors = status->getErrors();
        if (errors) {
            // Copy until we hit isc_arg_end or max length
            for (size_t i = 0; i < ISC_STATUS_LENGTH - 1 && errors[i] != isc_arg_end; ++i) {
                dest[i] = errors[i];
            }
        }
    }
}

/**
 * @brief RAII wrapper for IStatement with cursor management
 *
 * Provides a C++-friendly interface to the Firebird OO API statement handling.
 * Handles statement preparation, execution, cursor operations, and proper cleanup.
 *
 * Phase 5: Statement Infrastructure for FB 3.0+ OO API Migration
 */
class StatementWrapper {
public:
    StatementWrapper() noexcept = default;

    ~StatementWrapper() noexcept {
        // Clean up cursor and statement
        if (result_set_) {
            result_set_->release();
            result_set_ = nullptr;
        }
        if (statement_) {
            statement_->release();
            statement_ = nullptr;
        }
    }

    // Non-copyable
    StatementWrapper(const StatementWrapper&) = delete;
    StatementWrapper& operator=(const StatementWrapper&) = delete;

    // Movable
    StatementWrapper(StatementWrapper&& other) noexcept
        : statement_(other.statement_),
          result_set_(other.result_set_),
          prepared_(other.prepared_),
          cursor_open_(other.cursor_open_) {
        other.statement_ = nullptr;
        other.result_set_ = nullptr;
        other.prepared_ = false;
        other.cursor_open_ = false;
    }

    StatementWrapper& operator=(StatementWrapper&& other) noexcept {
        if (this != &other) {
            // Clean up current resources
            if (result_set_) result_set_->release();
            if (statement_) statement_->release();

            statement_ = other.statement_;
            result_set_ = other.result_set_;
            prepared_ = other.prepared_;
            cursor_open_ = other.cursor_open_;

            other.statement_ = nullptr;
            other.result_set_ = nullptr;
            other.prepared_ = false;
            other.cursor_open_ = false;
        }
        return *this;
    }

    /**
     * @brief Prepare a SQL statement
     * @return true on success, false on failure
     */
    bool prepare(
        Firebird::IMaster* master,
        Firebird::IAttachment* attachment,
        Firebird::ITransaction* transaction,
        const char* sql,
        unsigned sql_length,
        unsigned dialect,
        ISC_STATUS* status_vector
    ) noexcept {
        if (!master || !attachment || !sql) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_bad_req_handle;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }

        try {
            Firebird::IStatus* fb_status = master->getStatus();
            /* CRITICAL: Clear status before call.
             * Firebird's IMaster::getStatus() may return a reusable IStatus instance.
             * If previous calls populated errors, they can leak into subsequent calls
             * unless we explicitly reset it.
             *
             * Fixes: tests/003.phpt (stale validation error after suppressed query)
             */
            fb_status->init();
            Firebird::CheckStatusWrapper status(fb_status);

            // Prepare the statement
            statement_ = attachment->prepare(
                &status,
                transaction,
                sql_length ? sql_length : static_cast<unsigned>(std::strlen(sql)),
                sql,
                dialect,
                Firebird::IStatement::PREPARE_PREFETCH_METADATA
            );

            if (statusHasError(fb_status)) {
                copyStatusToVector(fb_status, status_vector);
                return false;
            }

            prepared_ = (statement_ != nullptr);
            return prepared_;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_except2;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }
    }

    /**
     * @brief Execute a non-SELECT statement
     * @return true on success, false on failure
     */
    bool execute(
        Firebird::IMaster* master,
        Firebird::ITransaction* transaction,
        void* in_msg,
        Firebird::IMessageMetadata* in_metadata,
        void* out_msg,
        Firebird::IMessageMetadata* out_metadata,
        ISC_STATUS* status_vector
    ) noexcept {
        if (!statement_ || !master) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_bad_stmt_handle;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }

        try {
            Firebird::IStatus* fb_status = master->getStatus();
            fb_status->init();
            Firebird::CheckStatusWrapper status(fb_status);

            statement_->execute(
                &status,
                transaction,
                in_metadata,
                static_cast<unsigned char*>(in_msg),
                out_metadata,
                static_cast<unsigned char*>(out_msg)
            );

            if (statusHasError(fb_status)) {
                copyStatusToVector(fb_status, status_vector);
                return false;
            }

            return true;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_except2;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }
    }

    /**
     * @brief Open cursor for SELECT statement
     * @return true on success, false on failure
     */
    bool openCursor(
        Firebird::IMaster* master,
        Firebird::ITransaction* transaction,
        void* in_msg,
        Firebird::IMessageMetadata* in_metadata,
        unsigned cursor_flags,
        ISC_STATUS* status_vector
    ) noexcept {
        if (!statement_ || !master) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_bad_stmt_handle;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }

        try {
            Firebird::IStatus* fb_status = master->getStatus();
            fb_status->init();
            Firebird::CheckStatusWrapper status(fb_status);

            // Close any existing cursor first
            if (result_set_) {
                result_set_->close(&status);
                result_set_->release();
                result_set_ = nullptr;
                cursor_open_ = false;
            }

            // Open new cursor
            result_set_ = statement_->openCursor(
                &status,
                transaction,
                in_metadata,
                static_cast<unsigned char*>(in_msg),
                nullptr,  // Use statement's output metadata
                cursor_flags
            );

            if (statusHasError(fb_status)) {
                copyStatusToVector(fb_status, status_vector);
                return false;
            }

            cursor_open_ = (result_set_ != nullptr);
            return cursor_open_;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_except2;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }
    }

    /**
     * @brief Fetch next row from cursor
     * @return 1 = row fetched, 0 = end of data, -1 = error
     */
    int fetchNext(
        Firebird::IMaster* master,
        void* out_msg,
        ISC_STATUS* status_vector
    ) noexcept {
        if (!result_set_ || !master || !cursor_open_) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_bad_stmt_handle;
                status_vector[2] = isc_arg_end;
            }
            return -1;
        }

        try {
            Firebird::IStatus* fb_status = master->getStatus();
            fb_status->init();
            Firebird::CheckStatusWrapper status(fb_status);

            int fetch_result = result_set_->fetchNext(
                &status,
                static_cast<unsigned char*>(out_msg)
            );

            if (statusHasError(fb_status)) {
                copyStatusToVector(fb_status, status_vector);
                /* Mark cursor as closed on error — the result set is likely
                 * invalidated (e.g. transaction committed/rolled back) */
                cursor_open_ = false;
                return -1;
            }

            // Firebird::IStatus::RESULT_OK = 0, RESULT_NO_DATA = 100
            if (fetch_result != Firebird::IStatus::RESULT_OK) {
                cursor_open_ = false;
                return 0;
            }
            return 1;

        } catch (...) {
            cursor_open_ = false;
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_except2;
                status_vector[2] = isc_arg_end;
            }
            return -1;
        }
    }

    /**
     * @brief Fetch previous row from scrollable cursor
     * @return 1 = row fetched, 0 = end of data, -1 = error
     */
    int fetchPrior(
        Firebird::IMaster* master,
        void* out_msg,
        ISC_STATUS* status_vector
    ) noexcept {
        if (!result_set_ || !master) return -1;
        try {
            Firebird::IStatus* fb_status = master->getStatus();
            fb_status->init();
            Firebird::CheckStatusWrapper status(fb_status);
            int r = result_set_->fetchPrior(&status, static_cast<unsigned char*>(out_msg));
            if (statusHasError(fb_status)) { copyStatusToVector(fb_status, status_vector); return -1; }
            return (r == Firebird::IStatus::RESULT_OK) ? 1 : 0;
        } catch (...) { return -1; }
    }

    /**
     * @brief Fetch first row from scrollable cursor
     * @return 1 = row fetched, 0 = end of data, -1 = error
     */
    int fetchFirst(
        Firebird::IMaster* master,
        void* out_msg,
        ISC_STATUS* status_vector
    ) noexcept {
        if (!result_set_ || !master) return -1;
        try {
            Firebird::IStatus* fb_status = master->getStatus();
            fb_status->init();
            Firebird::CheckStatusWrapper status(fb_status);
            int r = result_set_->fetchFirst(&status, static_cast<unsigned char*>(out_msg));
            if (statusHasError(fb_status)) { copyStatusToVector(fb_status, status_vector); return -1; }
            return (r == Firebird::IStatus::RESULT_OK) ? 1 : 0;
        } catch (...) { return -1; }
    }

    /**
     * @brief Fetch last row from scrollable cursor
     * @return 1 = row fetched, 0 = end of data, -1 = error
     */
    int fetchLast(
        Firebird::IMaster* master,
        void* out_msg,
        ISC_STATUS* status_vector
    ) noexcept {
        if (!result_set_ || !master) return -1;
        try {
            Firebird::IStatus* fb_status = master->getStatus();
            fb_status->init();
            Firebird::CheckStatusWrapper status(fb_status);
            int r = result_set_->fetchLast(&status, static_cast<unsigned char*>(out_msg));
            if (statusHasError(fb_status)) { copyStatusToVector(fb_status, status_vector); return -1; }
            return (r == Firebird::IStatus::RESULT_OK) ? 1 : 0;
        } catch (...) { return -1; }
    }

    /**
     * @brief Fetch row at absolute position from scrollable cursor
     * @return 1 = row fetched, 0 = end of data, -1 = error
     */
    int fetchAbsolute(
        Firebird::IMaster* master,
        int position,
        void* out_msg,
        ISC_STATUS* status_vector
    ) noexcept {
        if (!result_set_ || !master) return -1;
        try {
            Firebird::IStatus* fb_status = master->getStatus();
            fb_status->init();
            Firebird::CheckStatusWrapper status(fb_status);
            int r = result_set_->fetchAbsolute(&status, position, static_cast<unsigned char*>(out_msg));
            if (statusHasError(fb_status)) { copyStatusToVector(fb_status, status_vector); return -1; }
            return (r == Firebird::IStatus::RESULT_OK) ? 1 : 0;
        } catch (...) { return -1; }
    }

    /**
     * @brief Fetch row at relative offset from scrollable cursor
     * @return 1 = row fetched, 0 = end of data, -1 = error
     */
    int fetchRelative(
        Firebird::IMaster* master,
        int offset,
        void* out_msg,
        ISC_STATUS* status_vector
    ) noexcept {
        if (!result_set_ || !master) return -1;
        try {
            Firebird::IStatus* fb_status = master->getStatus();
            fb_status->init();
            Firebird::CheckStatusWrapper status(fb_status);
            int r = result_set_->fetchRelative(&status, offset, static_cast<unsigned char*>(out_msg));
            if (statusHasError(fb_status)) { copyStatusToVector(fb_status, status_vector); return -1; }
            return (r == Firebird::IStatus::RESULT_OK) ? 1 : 0;
        } catch (...) { return -1; }
    }

    /**
     * @brief Close the cursor, releasing server-side cursor resources.
     *
     * Uses IResultSet::close() which explicitly closes the cursor on the server
     * and then releases the interface (equivalent to isc_dsql_free_statement
     * DSQL_close). Without this, the cursor stays open on the server until the
     * attachment closes — causing excessive server RAM with repeated queries.
     *
     * @return true on success
     */
    bool closeCursor(ISC_STATUS* status_vector) noexcept {
        if (!result_set_) {
            cursor_open_ = false;
            return true;  // Already closed
        }

        try {
            // IResultSet::close() closes the server-side cursor AND releases the
            // interface (no separate release() needed afterwards).
            Firebird::IMaster* master = Firebird::fb_get_master_interface();
            if (master) {
                Firebird::IStatus* fb_status = master->getStatus();
                if (fb_status) {
                    fb_status->init();
                    Firebird::CheckStatusWrapper cs(fb_status);
                    bool had_error = false;
                    if (cursor_open_) {
                        // Cursor still open on server: close() sends DSQL_close + release.
                        // This is the key server-side RAM fix from #135.
                        result_set_->close(&cs);
                        had_error = statusHasError(fb_status);
                        if (had_error && status_vector) {
                            copyStatusToVector(fb_status, status_vector);
                        }
                    } else {
                        // fetchNext() already returned EOF/error: server implicitly
                        // closed the cursor. On Firebird 3.0, calling close() on an
                        // already-closed result set causes a segfault (double-close,
                        // see #137). Use release() — always safe, just decrements
                        // the C++ interface refcount.
                        result_set_->release();
                    }
                    // NOTE: do NOT call fb_status->dispose() here.
                    // CheckStatusWrapper destructor accesses fb_status on scope exit.
                    result_set_ = nullptr;
                    cursor_open_ = false;
                    return !had_error;
                }
            }
            // Fallback: just release (may not close cursor on server immediately)
            result_set_->release();
            result_set_ = nullptr;
            cursor_open_ = false;
            return true;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_except2;
                status_vector[2] = isc_arg_end;
            }
            result_set_ = nullptr;
            cursor_open_ = false;
            return false;
        }
    }

    /**
     * @brief Free/unprepare statement, releasing server-side prepared statement resources.
     *
     * Uses IStatement::free() which explicitly frees the prepared statement on the
     * server and then releases the interface (equivalent to isc_dsql_free_statement
     * DSQL_drop). Without this, the prepared statement accumulates in server memory
     * for every fbird_query() DML call — causing excessive server RAM usage
     * (see GitHub issue #135).
     *
     * @return true on success
     */
    bool free(ISC_STATUS* status_vector) noexcept {
        // Close cursor first
        closeCursor(status_vector);

        if (!statement_) {
            prepared_ = false;
            return true;
        }

        try {
            // Use release() to decrement the C++ interface refcount.
            //
            // NOTE: IStatement::free() (DSQL_drop equivalent) was tried here to
            // fix server-side RAM accumulation from DML queries (#135), but it
            // causes a segfault on Firebird 3.0 during connection cleanup (#137).
            // The IStatement::free() call appears to leave the server-side
            // connection in an invalid state on FB 3.0 when the statement was
            // associated with an active (uncommitted) transaction.
            //
            // The DML-specific #135 fix must be reimplemented with Firebird client
            // version detection (fb_get_master_interface()->getVersion() >= 4).
            // Until then, release() is the safe choice for all FB versions.
            statement_->release();
            statement_ = nullptr;
            prepared_ = false;
            return true;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_except2;
                status_vector[2] = isc_arg_end;
            }
            statement_ = nullptr;
            prepared_ = false;
            return false;
        }
    }

    /**
     * @brief Get input parameter metadata
     */
    Firebird::IMessageMetadata* getInputMetadata(
        Firebird::IMaster* master,
        ISC_STATUS* status_vector
    ) noexcept {
        if (!statement_ || !master) return nullptr;

        try {
            Firebird::IStatus* fb_status = master->getStatus();
            fb_status->init();
            Firebird::CheckStatusWrapper status(fb_status);

            auto* meta = statement_->getInputMetadata(&status);

            if (statusHasError(fb_status)) {
                copyStatusToVector(fb_status, status_vector);
                return nullptr;
            }

            return meta;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_except2;
                status_vector[2] = isc_arg_end;
            }
            return nullptr;
        }
    }

    /**
     * @brief Get output field metadata
     */
    Firebird::IMessageMetadata* getOutputMetadata(
        Firebird::IMaster* master,
        ISC_STATUS* status_vector
    ) noexcept {
        if (!statement_ || !master) return nullptr;

        try {
            Firebird::IStatus* fb_status = master->getStatus();
            fb_status->init();
            Firebird::CheckStatusWrapper status(fb_status);

            auto* meta = statement_->getOutputMetadata(&status);

            if (statusHasError(fb_status)) {
                copyStatusToVector(fb_status, status_vector);
                return nullptr;
            }

            return meta;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_except2;
                status_vector[2] = isc_arg_end;
            }
            return nullptr;
        }
    }

    /**
     * @brief Get statement type
     */
    unsigned getType(
        Firebird::IMaster* master,
        ISC_STATUS* status_vector
    ) noexcept {
        if (!statement_ || !master) return 0;

        try {
            Firebird::IStatus* fb_status = master->getStatus();
            fb_status->init();
            Firebird::CheckStatusWrapper status(fb_status);

            unsigned stmt_type = statement_->getType(&status);

            if (statusHasError(fb_status)) {
                copyStatusToVector(fb_status, status_vector);
                return 0;
            }

            return stmt_type;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_except2;
                status_vector[2] = isc_arg_end;
            }
            return 0;
        }
    }

    /**
     * @brief Get affected rows count
     */
    ISC_UINT64 getAffectedRecords(
        Firebird::IMaster* master,
        ISC_STATUS* status_vector
    ) noexcept {
        if (!statement_ || !master) return 0;

        try {
            Firebird::IStatus* fb_status = master->getStatus();
            fb_status->init();
            Firebird::CheckStatusWrapper status(fb_status);

            ISC_UINT64 count = statement_->getAffectedRecords(&status);

            if (statusHasError(fb_status)) {
                copyStatusToVector(fb_status, status_vector);
                return 0;
            }

            return count;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = isc_arg_gds;
                status_vector[1] = isc_except2;
                status_vector[2] = isc_arg_end;
            }
            return 0;
        }
    }

    // Accessors
    [[nodiscard]] Firebird::IStatement* getStatement() const noexcept { return statement_; }
    [[nodiscard]] Firebird::IResultSet* getResultSet() const noexcept { return result_set_; }
    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }
    [[nodiscard]] bool isCursorOpen() const noexcept { return cursor_open_; }

private:
    Firebird::IStatement* statement_{nullptr};
    Firebird::IResultSet* result_set_{nullptr};
    bool prepared_{false};
    bool cursor_open_{false};
};

} // namespace fb

#endif // FB_STATEMENT_HPP
