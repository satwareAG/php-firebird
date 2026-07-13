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
        // Use free() for proper server-side cleanup (closes cursor + drops
        // prepared statement on FB 4.0+).  If free() was already called by
        // fbs_free(), both pointers are nullptr and free() is a no-op.
        // This is a safety net for abnormal destruction paths (e.g. stack
        // unwinding, move-assignment displacement) where fbs_free() was
        // never called.  Fixes GitHub issue #135 (belt-and-suspenders).
        ISC_STATUS dummy[ISC_STATUS_LENGTH] = {0};
        free(dummy);
    }

    // Non-copyable
    StatementWrapper(const StatementWrapper&) = delete;
    StatementWrapper& operator=(const StatementWrapper&) = delete;

    // Movable
    StatementWrapper(StatementWrapper&& other) noexcept
        : statement_(other.statement_),
          result_set_(other.result_set_),
          master_(other.master_),
          prepared_(other.prepared_),
          cursor_open_(other.cursor_open_) {
        other.statement_ = nullptr;
        other.result_set_ = nullptr;
        other.master_ = nullptr;
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
            master_ = other.master_;
            prepared_ = other.prepared_;
            cursor_open_ = other.cursor_open_;

            other.statement_ = nullptr;
            other.result_set_ = nullptr;
            other.master_ = nullptr;
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
            set_status_error(status_vector, isc_bad_req_handle);
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
                fb_status->dispose();
                return false;
            }

            master_ = master;  // Store for FB4+ closeCursor/free status handling
            prepared_ = (statement_ != nullptr);
            fb_status->dispose();
            return prepared_;

        } catch (...) {
            set_status_error(status_vector, isc_except2);
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
            set_status_error(status_vector, isc_bad_stmt_handle);
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
                fb_status->dispose();
                return false;
            }

            fb_status->dispose();
            return true;

        } catch (...) {
            set_status_error(status_vector, isc_except2);
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
            set_status_error(status_vector, isc_bad_stmt_handle);
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
                fb_status->dispose();
                return false;
            }

            cursor_open_ = (result_set_ != nullptr);
            fb_status->dispose();
            return cursor_open_;

        } catch (...) {
            set_status_error(status_vector, isc_except2);
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
            set_status_error(status_vector, isc_bad_stmt_handle);
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
                cursor_open_ = false;
                fb_status->dispose();
                return -1;
            }

            // Firebird::IStatus::RESULT_OK = 0, RESULT_NO_DATA = 100
            if (fetch_result != Firebird::IStatus::RESULT_OK) {
                cursor_open_ = false;
                fb_status->dispose();
                return 0;
            }
            fb_status->dispose();
            return 1;

        } catch (...) {
            cursor_open_ = false;
            set_status_error(status_vector, isc_except2);
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
            if (statusHasError(fb_status)) { copyStatusToVector(fb_status, status_vector); fb_status->dispose(); return -1; }
            fb_status->dispose();
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
            if (statusHasError(fb_status)) { copyStatusToVector(fb_status, status_vector); fb_status->dispose(); return -1; }
            fb_status->dispose();
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
            if (statusHasError(fb_status)) { copyStatusToVector(fb_status, status_vector); fb_status->dispose(); return -1; }
            fb_status->dispose();
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
            if (statusHasError(fb_status)) { copyStatusToVector(fb_status, status_vector); fb_status->dispose(); return -1; }
            fb_status->dispose();
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
            if (statusHasError(fb_status)) { copyStatusToVector(fb_status, status_vector); fb_status->dispose(); return -1; }
            fb_status->dispose();
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
#if FB_API_VER >= 40
            // FB 4.0+: IResultSet::close() explicitly closes the server cursor and
            // releases the interface (fixes #135 RAM accumulation for SELECT queries).
            //
            // cursor_open_ guards against double-close: when fetchNext() returns
            // RESULT_NO_DATA (EOF), the FB server implicitly closes the cursor.
            // If cursor_open_ is false, release() avoids a use-after-free crash.
            if (cursor_open_ && master_) {
                Firebird::IStatus* fb_status_ptr = master_->getStatus();
                fb_status_ptr->init();
                Firebird::CheckStatusWrapper fb_status(fb_status_ptr);
                result_set_->close(&fb_status);
                result_set_ = nullptr;  // close() released the interface
                cursor_open_ = false;
                if (statusHasError(fb_status_ptr)) {
                    copyStatusToVector(fb_status_ptr, status_vector);
                    fb_status_ptr->dispose();
                    return false;
                }
                fb_status_ptr->dispose();
                return true;
            }
            // EOF path or no master: release only (server already closed cursor)
            result_set_->release();
#else
            // FB 3.0: release() only. close() segfaults on double-close after EOF
            // because FB 3.0 implicitly closes cursors server-side at EOF (#137).
            result_set_->release();
#endif
            result_set_ = nullptr;
            cursor_open_ = false;
            return true;

        } catch (...) {
            set_status_error(status_vector, isc_except2);
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
#if FB_API_VER >= 40
            // FB 4.0+: IStatement::free() (DSQL_drop equivalent) explicitly drops
            // the prepared statement server-side and releases the interface.
            // Fixes #135 server RAM accumulation for DML via fbird_query().
            if (master_) {
                Firebird::IStatus* fb_status_ptr = master_->getStatus();
                fb_status_ptr->init();
                Firebird::CheckStatusWrapper fb_status(fb_status_ptr);
                statement_->free(&fb_status);
                statement_ = nullptr;  // free() released the interface
                prepared_ = false;
                if (statusHasError(fb_status_ptr)) {
                    copyStatusToVector(fb_status_ptr, status_vector);
                    fb_status_ptr->dispose();
                    return false;
                }
                fb_status_ptr->dispose();
                return true;
            }
            // Fallback if no master: release only
            statement_->release();
#else
            // FB 3.0: release() only. free() segfaults when statement has an
            // uncommitted transaction, corrupting server connection state (#137).
            statement_->release();
#endif
            statement_ = nullptr;
            prepared_ = false;
            return true;

        } catch (...) {
            set_status_error(status_vector, isc_except2);
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
                fb_status->dispose();
                return nullptr;
            }

            fb_status->dispose();
            return meta;

        } catch (...) {
            set_status_error(status_vector, isc_except2);
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
                fb_status->dispose();
                return nullptr;
            }

            fb_status->dispose();
            return meta;

        } catch (...) {
            set_status_error(status_vector, isc_except2);
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
                fb_status->dispose();
                return 0;
            }

            fb_status->dispose();
            return stmt_type;

        } catch (...) {
            set_status_error(status_vector, isc_except2);
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
                fb_status->dispose();
                return 0;
            }

            fb_status->dispose();
            return count;

        } catch (...) {
            set_status_error(status_vector, isc_except2);
            return 0;
        }
    }

    // Accessors
    [[nodiscard]] Firebird::IStatement* getStatement() const noexcept { return statement_; }
    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }
    [[nodiscard]] bool isCursorOpen() const noexcept { return cursor_open_; }

private:
    Firebird::IStatement* statement_{nullptr};
    Firebird::IResultSet* result_set_{nullptr};
    Firebird::IMaster* master_{nullptr};  ///< Stored at prepare() for FB4+ status handling
    bool prepared_{false};
    bool cursor_open_{false};
};

} // namespace fb

#endif // FB_STATEMENT_HPP
