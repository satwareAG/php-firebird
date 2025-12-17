/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

/**
 * @file fb_blob.hpp
 * @brief RAII wrapper for Firebird IBlob interface (Phase 6)
 *
 * Provides C++ wrapper for blob operations using OO API.
 * Replaces legacy isc_create_blob, isc_open_blob, isc_put_segment, etc.
 */

#ifndef FB_BLOB_HPP
#define FB_BLOB_HPP

#include <firebird/Interface.h>
#include "fb_status.hpp"
#include "fb_core.hpp"
#include <cstring>
#include <new>

#ifdef __cplusplus

namespace fb {

/**
 * Helper function to clear status vector to success state.
 */
inline void clearStatusVector(ISC_STATUS* status_vector) noexcept {
    if (status_vector) {
        status_vector[0] = 1;
        status_vector[1] = 0;  // Success
        status_vector[2] = isc_arg_end;
    }
}

/**
 * Helper function to copy status from CheckStatusWrapper to ISC_STATUS array.
 */
inline void copyStatusVector(Firebird::CheckStatusWrapper* status, ISC_STATUS* status_vector) noexcept {
    if (!status || !status_vector) return;
    const ISC_STATUS* errors = status->getErrors();
    if (errors) {
        for (int i = 0; i < ISC_STATUS_LENGTH && errors[i] != isc_arg_end; ++i) {
            status_vector[i] = errors[i];
        }
    }
}

/**
 * RAII wrapper for Firebird IBlob interface.
 *
 * Handles blob lifecycle: create/open, read/write segments, close/cancel.
 */
class BlobWrapper {
private:
    Firebird::IBlob* blob_ = nullptr;
    bool owns_blob_ = false;
    ISC_QUAD blob_id_{0, 0};

public:
    BlobWrapper() = default;

    ~BlobWrapper() {
        if (blob_ && owns_blob_) {
            // Silent close on destruction - errors ignored
            auto* master = getMaster();
            if (master) {
                Firebird::CheckStatusWrapper status(master->getStatus());
                blob_->close(&status);
            }
            blob_ = nullptr;
        }
    }

    // Non-copyable
    BlobWrapper(const BlobWrapper&) = delete;
    BlobWrapper& operator=(const BlobWrapper&) = delete;

    // Movable
    BlobWrapper(BlobWrapper&& other) noexcept
        : blob_(other.blob_), owns_blob_(other.owns_blob_), blob_id_(other.blob_id_) {
        other.blob_ = nullptr;
        other.owns_blob_ = false;
        other.blob_id_ = {0, 0};
    }

    BlobWrapper& operator=(BlobWrapper&& other) noexcept {
        if (this != &other) {
            if (blob_ && owns_blob_) {
                auto* master = getMaster();
                if (master) {
                    Firebird::CheckStatusWrapper status(master->getStatus());
                    blob_->close(&status);
                }
            }
            blob_ = other.blob_;
            owns_blob_ = other.owns_blob_;
            blob_id_ = other.blob_id_;
            other.blob_ = nullptr;
            other.owns_blob_ = false;
            other.blob_id_ = {0, 0};
        }
        return *this;
    }

    /**
     * Create a new blob for writing.
     *
     * @param master IMaster interface pointer
     * @param attachment IAttachment for the connection
     * @param transaction ITransaction for the operation
     * @param bpb_length BPB (Blob Parameter Block) length (0 for default)
     * @param bpb BPB data (nullptr for default)
     * @param status_vector Output status vector for error reporting
     * @return true on success, false on error
     */
    bool create(Firebird::IMaster* master,
                Firebird::IAttachment* attachment,
                Firebird::ITransaction* transaction,
                unsigned bpb_length,
                const unsigned char* bpb,
                ISC_STATUS* status_vector) {
        if (!master || !attachment || !transaction) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_bad_db_handle;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }

        // Close existing blob if any
        if (blob_ && owns_blob_) {
            Firebird::CheckStatusWrapper close_status(master->getStatus());
            blob_->close(&close_status);
            blob_ = nullptr;
        }

        Firebird::CheckStatusWrapper status(master->getStatus());

        try {
            blob_ = attachment->createBlob(&status, transaction, &blob_id_, bpb_length, bpb);

            // Check both for NULL blob AND error status
            if (!blob_ || statusHasError(&status)) {
                copyStatusVector(&status, status_vector);
                if (!blob_ && status_vector && status_vector[1] == 0) {
                    // createBlob returned NULL without setting error
                    status_vector[1] = isc_bad_segstr_handle;
                }
                blob_ = nullptr;
                return false;
            }

            owns_blob_ = true;
            clearStatusVector(status_vector);
            return true;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_random;
                status_vector[2] = isc_arg_end;
            }
            blob_ = nullptr;
            return false;
        }
    }

    /**
     * Open an existing blob for reading.
     *
     * @param master IMaster interface pointer
     * @param attachment IAttachment for the connection
     * @param transaction ITransaction for the operation
     * @param blob_id Blob ID (ISC_QUAD) to open
     * @param bpb_length BPB length (0 for default)
     * @param bpb BPB data (nullptr for default)
     * @param status_vector Output status vector for error reporting
     * @return true on success, false on error
     */
    bool open(Firebird::IMaster* master,
              Firebird::IAttachment* attachment,
              Firebird::ITransaction* transaction,
              const ISC_QUAD* blob_id,
              unsigned bpb_length,
              const unsigned char* bpb,
              ISC_STATUS* status_vector) {
        if (!master || !attachment || !transaction || !blob_id) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_bad_db_handle;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }

        // Close existing blob if any
        if (blob_ && owns_blob_) {
            Firebird::CheckStatusWrapper close_status(master->getStatus());
            blob_->close(&close_status);
            blob_ = nullptr;
        }

        blob_id_ = *blob_id;
        Firebird::CheckStatusWrapper status(master->getStatus());

        try {
            blob_ = attachment->openBlob(&status, transaction, &blob_id_, bpb_length, bpb);

            // Check both for NULL blob AND error status
            if (!blob_ || statusHasError(&status)) {
                copyStatusVector(&status, status_vector);
                if (!blob_ && status_vector && status_vector[1] == 0) {
                    // openBlob returned NULL without setting error
                    status_vector[1] = isc_bad_segstr_handle;
                }
                blob_ = nullptr;
                return false;
            }

            owns_blob_ = true;
            clearStatusVector(status_vector);
            return true;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_random;
                status_vector[2] = isc_arg_end;
            }
            blob_ = nullptr;
            return false;
        }
    }

    /**
     * Write a segment to the blob.
     *
     * @param master IMaster interface pointer
     * @param length Segment length
     * @param buffer Data to write
     * @param status_vector Output status vector
     * @return true on success, false on error
     */
    bool putSegment(Firebird::IMaster* master,
                    unsigned length,
                    const void* buffer,
                    ISC_STATUS* status_vector) {
        if (!blob_ || !master) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_bad_segstr_handle;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }

        Firebird::CheckStatusWrapper status(master->getStatus());

        try {
            blob_->putSegment(&status, length, buffer);

            if (statusHasError(&status)) {
                copyStatusVector(&status, status_vector);
                return false;
            }

            clearStatusVector(status_vector);
            return true;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_random;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }
    }

    /**
     * Read a segment from the blob.
     *
     * @param master IMaster interface pointer
     * @param buffer_length Buffer size
     * @param buffer Output buffer
     * @param actual_length Output: actual bytes read
     * @param status_vector Output status vector
     * @return 0 on success with more data, 1 on EOF, 2 on segment (partial read), -1 on error
     */
    int getSegment(Firebird::IMaster* master,
                   unsigned buffer_length,
                   void* buffer,
                   unsigned* actual_length,
                   ISC_STATUS* status_vector) {
        if (!blob_ || !master) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_bad_segstr_handle;
                status_vector[2] = isc_arg_end;
            }
            if (actual_length) *actual_length = 0;
            return -1;
        }

        Firebird::CheckStatusWrapper status(master->getStatus());

        try {
            int result = blob_->getSegment(&status, buffer_length, buffer, actual_length);

            if (statusHasError(&status)) {
                // Check for special conditions
                unsigned state = status.getState();
                if (state & Firebird::IStatus::STATE_WARNINGS) {
                    // EOF or segment - these are not errors
                    ISC_STATUS err = status.getErrors()[1];
                    if (err == isc_segstr_eof) {
                        clearStatusVector(status_vector);
                        return 1; // EOF
                    }
                    if (err == isc_segment) {
                        clearStatusVector(status_vector);
                        return 2; // Segment (partial read)
                    }
                }
                copyStatusVector(&status, status_vector);
                return -1;
            }

            // Check Firebird result code
            if (result == Firebird::IStatus::RESULT_NO_DATA) {
                clearStatusVector(status_vector);
                return 1; // EOF
            }
            if (result == Firebird::IStatus::RESULT_SEGMENT) {
                clearStatusVector(status_vector);
                return 2; // Partial segment
            }

            clearStatusVector(status_vector);
            return 0; // Success, more data available

        } catch (...) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_random;
                status_vector[2] = isc_arg_end;
            }
            if (actual_length) *actual_length = 0;
            return -1;
        }
    }

    /**
     * Close the blob (commit writes).
     *
     * @param master IMaster interface pointer
     * @param status_vector Output status vector
     * @return true on success, false on error
     */
    bool close(Firebird::IMaster* master, ISC_STATUS* status_vector) {
        if (!blob_) {
            clearStatusVector(status_vector);
            return true; // Already closed
        }

        if (!master) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_bad_db_handle;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }

        Firebird::CheckStatusWrapper status(master->getStatus());

        try {
            blob_->close(&status);

            if (statusHasError(&status)) {
                copyStatusVector(&status, status_vector);
                // Still mark as closed to avoid double-close
                blob_ = nullptr;
                owns_blob_ = false;
                return false;
            }

            blob_ = nullptr;
            owns_blob_ = false;
            clearStatusVector(status_vector);
            return true;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_random;
                status_vector[2] = isc_arg_end;
            }
            blob_ = nullptr;
            owns_blob_ = false;
            return false;
        }
    }

    /**
     * Cancel the blob (discard writes).
     *
     * @param master IMaster interface pointer
     * @param status_vector Output status vector
     * @return true on success, false on error
     */
    bool cancel(Firebird::IMaster* master, ISC_STATUS* status_vector) {
        if (!blob_) {
            clearStatusVector(status_vector);
            return true; // Already closed/cancelled
        }

        if (!master) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_bad_db_handle;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }

        Firebird::CheckStatusWrapper status(master->getStatus());

        try {
            blob_->cancel(&status);

            if (statusHasError(&status)) {
                // Ignore "invalid blob handle" error during cancel
                if (status.getErrors()[1] != isc_bad_segstr_handle) {
                    copyStatusVector(&status, status_vector);
                    blob_ = nullptr;
                    owns_blob_ = false;
                    return false;
                }
            }

            blob_ = nullptr;
            owns_blob_ = false;
            clearStatusVector(status_vector);
            return true;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_random;
                status_vector[2] = isc_arg_end;
            }
            blob_ = nullptr;
            owns_blob_ = false;
            return false;
        }
    }

    /**
     * Get blob info.
     *
     * @param master IMaster interface pointer
     * @param items_length Info items length
     * @param items Info items to request
     * @param buffer_length Output buffer length
     * @param buffer Output buffer
     * @param status_vector Output status vector
     * @return true on success, false on error
     */
    bool getInfo(Firebird::IMaster* master,
                 unsigned items_length,
                 const unsigned char* items,
                 unsigned buffer_length,
                 unsigned char* buffer,
                 ISC_STATUS* status_vector) {
        if (!blob_ || !master) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_bad_segstr_handle;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }

        Firebird::CheckStatusWrapper status(master->getStatus());

        try {
            blob_->getInfo(&status, items_length, items, buffer_length, buffer);

            if (statusHasError(&status)) {
                copyStatusVector(&status, status_vector);
                return false;
            }

            clearStatusVector(status_vector);
            return true;

        } catch (...) {
            if (status_vector) {
                status_vector[0] = 1;
                status_vector[1] = isc_random;
                status_vector[2] = isc_arg_end;
            }
            return false;
        }
    }

    /**
     * Get the blob ID (valid after create).
     */
    [[nodiscard]] const ISC_QUAD& getBlobId() const noexcept {
        return blob_id_;
    }

    /**
     * Set blob ID (for opening existing blob).
     */
    void setBlobId(const ISC_QUAD& id) noexcept {
        blob_id_ = id;
    }

    /**
     * Get raw IBlob pointer.
     */
    [[nodiscard]] Firebird::IBlob* getBlob() const noexcept {
        return blob_;
    }

    /**
     * Check if blob is open.
     */
    [[nodiscard]] bool isOpen() const noexcept {
        return blob_ != nullptr;
    }

    /**
     * Release ownership of blob handle (for transfer to legacy code).
     */
    Firebird::IBlob* release() noexcept {
        Firebird::IBlob* result = blob_;
        blob_ = nullptr;
        owns_blob_ = false;
        return result;
    }
};

} // namespace fb

#endif // __cplusplus

/*
 * C interface for blob operations (fbb_* functions)
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a new blob for writing.
 *
 * @param master IMaster interface pointer
 * @param attachment IAttachment pointer
 * @param transaction ITransaction pointer
 * @param blob_id Output: blob ID after creation
 * @param bpb_length BPB length
 * @param bpb BPB data
 * @param status_vector Output status vector
 * @return Opaque blob wrapper pointer, or NULL on error
 */
void* fbb_create(void* master,
                 void* attachment,
                 void* transaction,
                 ISC_QUAD* blob_id,
                 unsigned bpb_length,
                 const unsigned char* bpb,
                 ISC_STATUS* status_vector);

/**
 * Open an existing blob for reading.
 *
 * @param master IMaster interface pointer
 * @param attachment IAttachment pointer
 * @param transaction ITransaction pointer
 * @param blob_id Blob ID to open
 * @param bpb_length BPB length
 * @param bpb BPB data
 * @param status_vector Output status vector
 * @return Opaque blob wrapper pointer, or NULL on error
 */
void* fbb_open(void* master,
               void* attachment,
               void* transaction,
               const ISC_QUAD* blob_id,
               unsigned bpb_length,
               const unsigned char* bpb,
               ISC_STATUS* status_vector);

/**
 * Write a segment to the blob.
 *
 * @param master IMaster interface pointer
 * @param blob_wrapper Blob wrapper pointer
 * @param length Segment length
 * @param buffer Data to write
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbb_put_segment(void* master,
                    void* blob_wrapper,
                    unsigned length,
                    const void* buffer,
                    ISC_STATUS* status_vector);

/**
 * Read a segment from the blob.
 *
 * @param master IMaster interface pointer
 * @param blob_wrapper Blob wrapper pointer
 * @param buffer_length Buffer size
 * @param buffer Output buffer
 * @param actual_length Output: actual bytes read
 * @param status_vector Output status vector
 * @return 0 on success with more data, 1 on EOF, 2 on segment, -1 on error
 */
int fbb_get_segment(void* master,
                    void* blob_wrapper,
                    unsigned buffer_length,
                    void* buffer,
                    unsigned* actual_length,
                    ISC_STATUS* status_vector);

/**
 * Close the blob (commit writes).
 *
 * @param master IMaster interface pointer
 * @param blob_wrapper Blob wrapper pointer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbb_close(void* master, void* blob_wrapper, ISC_STATUS* status_vector);

/**
 * Cancel the blob (discard writes).
 *
 * @param master IMaster interface pointer
 * @param blob_wrapper Blob wrapper pointer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbb_cancel(void* master, void* blob_wrapper, ISC_STATUS* status_vector);

/**
 * Get blob info.
 *
 * @param master IMaster interface pointer
 * @param blob_wrapper Blob wrapper pointer
 * @param items_length Info items length
 * @param items Info items to request
 * @param buffer_length Output buffer length
 * @param buffer Output buffer
 * @param status_vector Output status vector
 * @return 1 on success, 0 on error
 */
int fbb_get_info(void* master,
                 void* blob_wrapper,
                 unsigned items_length,
                 const unsigned char* items,
                 unsigned buffer_length,
                 unsigned char* buffer,
                 ISC_STATUS* status_vector);

/**
 * Get blob ID from wrapper.
 *
 * @param blob_wrapper Blob wrapper pointer
 * @param blob_id Output: blob ID
 */
void fbb_get_blob_id(void* blob_wrapper, ISC_QUAD* blob_id);

/**
 * Check if blob is open.
 *
 * @param blob_wrapper Blob wrapper pointer
 * @return 1 if open, 0 if closed or invalid
 */
int fbb_is_open(void* blob_wrapper);

/**
 * Get raw IBlob handle from wrapper.
 *
 * @param blob_wrapper Blob wrapper pointer
 * @return Raw IBlob pointer, or NULL if invalid
 */
void* fbb_get_handle(void* blob_wrapper);

/**
 * Free blob wrapper (without closing - blob must be closed first).
 *
 * @param blob_wrapper Blob wrapper pointer
 */
void fbb_free(void* blob_wrapper);

#ifdef __cplusplus
} // extern "C"
#endif

/*
 * C++ implementation of C interface functions
 * Define FBB_NO_INLINE_IMPL before including this header to provide
 * your own implementations in a .cpp file (avoids multiple definition errors).
 */
#if defined(__cplusplus) && !defined(FBB_NO_INLINE_IMPL)

inline void* fbb_create(void* master,
                        void* attachment,
                        void* transaction,
                        ISC_QUAD* blob_id,
                        unsigned bpb_length,
                        const unsigned char* bpb,
                        ISC_STATUS* status_vector) {
    if (!master || !attachment || !transaction) {
        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = isc_bad_db_handle;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    auto* wrapper = new (std::nothrow) fb::BlobWrapper();
    if (!wrapper) {
        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = isc_virmemexh;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    bool success = wrapper->create(
        static_cast<Firebird::IMaster*>(master),
        static_cast<Firebird::IAttachment*>(attachment),
        static_cast<Firebird::ITransaction*>(transaction),
        bpb_length, bpb, status_vector
    );

    if (!success) {
        delete wrapper;
        return nullptr;
    }

    if (blob_id) {
        *blob_id = wrapper->getBlobId();
    }

    return wrapper;
}

inline void* fbb_open(void* master,
                      void* attachment,
                      void* transaction,
                      const ISC_QUAD* blob_id,
                      unsigned bpb_length,
                      const unsigned char* bpb,
                      ISC_STATUS* status_vector) {
    if (!master || !attachment || !transaction || !blob_id) {
        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = isc_bad_db_handle;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    auto* wrapper = new (std::nothrow) fb::BlobWrapper();
    if (!wrapper) {
        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = isc_virmemexh;
            status_vector[2] = isc_arg_end;
        }
        return nullptr;
    }

    bool success = wrapper->open(
        static_cast<Firebird::IMaster*>(master),
        static_cast<Firebird::IAttachment*>(attachment),
        static_cast<Firebird::ITransaction*>(transaction),
        blob_id, bpb_length, bpb, status_vector
    );

    if (!success) {
        delete wrapper;
        return nullptr;
    }

    return wrapper;
}

inline int fbb_put_segment(void* master,
                           void* blob_wrapper,
                           unsigned length,
                           const void* buffer,
                           ISC_STATUS* status_vector) {
    if (!blob_wrapper) {
        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = isc_bad_segstr_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);
    return wrapper->putSegment(
        static_cast<Firebird::IMaster*>(master),
        length, buffer, status_vector
    ) ? 1 : 0;
}

inline int fbb_get_segment(void* master,
                           void* blob_wrapper,
                           unsigned buffer_length,
                           void* buffer,
                           unsigned* actual_length,
                           ISC_STATUS* status_vector) {
    if (!blob_wrapper) {
        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = isc_bad_segstr_handle;
            status_vector[2] = isc_arg_end;
        }
        if (actual_length) *actual_length = 0;
        return -1;
    }

    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);
    return wrapper->getSegment(
        static_cast<Firebird::IMaster*>(master),
        buffer_length, buffer, actual_length, status_vector
    );
}

inline int fbb_close(void* master, void* blob_wrapper, ISC_STATUS* status_vector) {
    if (!blob_wrapper) {
        fb::clearStatusVector(status_vector);
        return 1; // Already closed
    }

    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);
    return wrapper->close(
        static_cast<Firebird::IMaster*>(master),
        status_vector
    ) ? 1 : 0;
}

inline int fbb_cancel(void* master, void* blob_wrapper, ISC_STATUS* status_vector) {
    if (!blob_wrapper) {
        fb::clearStatusVector(status_vector);
        return 1; // Already cancelled
    }

    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);
    return wrapper->cancel(
        static_cast<Firebird::IMaster*>(master),
        status_vector
    ) ? 1 : 0;
}

inline int fbb_get_info(void* master,
                        void* blob_wrapper,
                        unsigned items_length,
                        const unsigned char* items,
                        unsigned buffer_length,
                        unsigned char* buffer,
                        ISC_STATUS* status_vector) {
    if (!blob_wrapper) {
        if (status_vector) {
            status_vector[0] = 1;
            status_vector[1] = isc_bad_segstr_handle;
            status_vector[2] = isc_arg_end;
        }
        return 0;
    }

    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);
    return wrapper->getInfo(
        static_cast<Firebird::IMaster*>(master),
        items_length, items, buffer_length, buffer, status_vector
    ) ? 1 : 0;
}

inline void fbb_get_blob_id(void* blob_wrapper, ISC_QUAD* blob_id) {
    if (blob_wrapper && blob_id) {
        auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);
        *blob_id = wrapper->getBlobId();
    }
}

inline int fbb_is_open(void* blob_wrapper) {
    if (!blob_wrapper) return 0;
    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);
    return wrapper->isOpen() ? 1 : 0;
}

inline void* fbb_get_handle(void* blob_wrapper) {
    if (!blob_wrapper) return nullptr;
    auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);
    return wrapper->getBlob();
}

inline void fbb_free(void* blob_wrapper) {
    if (blob_wrapper) {
        auto* wrapper = static_cast<fb::BlobWrapper*>(blob_wrapper);
        delete wrapper;
    }
}

#endif // __cplusplus

#endif // FB_BLOB_HPP
