/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FB_METADATA_HPP
#define FB_METADATA_HPP

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
 * Field information structure.
 * Contains complete metadata about a result set field or input parameter.
 */
struct FieldInfo {
    std::string name;           // Field name
    std::string alias;          // Field alias (if different from name)
    std::string relation;       // Table/relation name
    std::string owner;          // Owner name (FB 3.0+)
    unsigned type = 0;          // SQL type (SQL_TEXT, SQL_VARYING, etc.)
    unsigned subType = 0;       // Sub-type (for BLOBs, NUMERICs)
    int length = 0;             // Field length in bytes
    int scale = 0;              // Numeric scale (negative for decimals)
    int precision = 0;          // Numeric precision
    unsigned charsetId = 0;     // Character set ID
    unsigned collationId = 0;   // Collation ID
    unsigned offset = 0;        // Offset in message buffer
    unsigned nullOffset = 0;    // Offset of null indicator
    bool nullable = false;      // Can be NULL

    /**
     * Check if the field is a text type.
     */
    [[nodiscard]] bool isTextType() const noexcept {
        return type == SQL_TEXT || type == SQL_VARYING;
    }

    /**
     * Check if the field is a BLOB.
     */
    [[nodiscard]] bool isBlobType() const noexcept {
        return type == SQL_BLOB;
    }

    /**
     * Check if the field is numeric (INT, DECIMAL, FLOAT, etc.).
     */
    [[nodiscard]] bool isNumericType() const noexcept {
        switch (type) {
            case SQL_SHORT:
            case SQL_LONG:
            case SQL_INT64:
            case SQL_FLOAT:
            case SQL_DOUBLE:
            case SQL_D_FLOAT:
#if FB_API_VER >= 40
            case SQL_INT128:
            case SQL_DEC16:
            case SQL_DEC34:
#endif
                return true;
            default:
                return false;
        }
    }

    /**
     * Check if the field is a date/time type.
     */
    [[nodiscard]] bool isDateTimeType() const noexcept {
        switch (type) {
            case SQL_DATE:       // Actually DATE in SQL
            case SQL_TYPE_DATE:  // DATE
            case SQL_TYPE_TIME:  // TIME
            case SQL_TIMESTAMP:
#if FB_API_VER >= 40
            case SQL_TIME_TZ:
            case SQL_TIMESTAMP_TZ:
#endif
                return true;
            default:
                return false;
        }
    }

    /**
     * Get a display-friendly type name.
     */
    [[nodiscard]] std::string typeName() const {
        switch (type & ~1) {  // Remove nullable flag bit
            case SQL_TEXT: return "CHAR";
            case SQL_VARYING: return "VARCHAR";
            case SQL_SHORT: return scale == 0 ? "SMALLINT" : "NUMERIC";
            case SQL_LONG: return scale == 0 ? "INTEGER" : "NUMERIC";
            case SQL_INT64: return scale == 0 ? "BIGINT" : "NUMERIC";
            case SQL_FLOAT: return "FLOAT";
            case SQL_DOUBLE: return "DOUBLE PRECISION";
            case SQL_D_FLOAT: return "DOUBLE";
            case SQL_DATE: return "DATE";
            case SQL_TYPE_DATE: return "DATE";
            case SQL_TYPE_TIME: return "TIME";
            case SQL_TIMESTAMP: return "TIMESTAMP";
            case SQL_BLOB: return subType == 1 ? "BLOB SUB_TYPE TEXT" : "BLOB";
            case SQL_ARRAY: return "ARRAY";
            case SQL_BOOLEAN: return "BOOLEAN";
#if FB_API_VER >= 40
            case SQL_INT128: return scale == 0 ? "INT128" : "NUMERIC";
            case SQL_DEC16: return "DECFLOAT(16)";
            case SQL_DEC34: return "DECFLOAT(34)";
            case SQL_TIME_TZ: return "TIME WITH TIME ZONE";
            case SQL_TIMESTAMP_TZ: return "TIMESTAMP WITH TIME ZONE";
#endif
            default: return "UNKNOWN";
        }
    }
};

/**
 * RAII wrapper for Firebird IMessageMetadata interface.
 * Provides safe access to result set and parameter metadata.
 */
class MetadataWrapper {
public:
    /**
     * Construct from IMessageMetadata (takes ownership if owns=true).
     */
    explicit MetadataWrapper(Firebird::IMessageMetadata* metadata, bool owns = true) noexcept
        : metadata_(metadata), owns_(owns) {}

    ~MetadataWrapper() {
        if (owns_ && metadata_) {
            metadata_->release();
        }
    }

    // Non-copyable
    MetadataWrapper(const MetadataWrapper&) = delete;
    MetadataWrapper& operator=(const MetadataWrapper&) = delete;

    // Movable
    MetadataWrapper(MetadataWrapper&& other) noexcept
        : metadata_(other.metadata_), owns_(other.owns_) {
        other.metadata_ = nullptr;
        other.owns_ = false;
    }

    MetadataWrapper& operator=(MetadataWrapper&& other) noexcept {
        if (this != &other) {
            if (owns_ && metadata_) {
                metadata_->release();
            }
            metadata_ = other.metadata_;
            owns_ = other.owns_;
            other.metadata_ = nullptr;
            other.owns_ = false;
        }
        return *this;
    }

    /**
     * Get the raw metadata pointer.
     */
    [[nodiscard]] Firebird::IMessageMetadata* get() noexcept { return metadata_; }
    [[nodiscard]] const Firebird::IMessageMetadata* get() const noexcept { return metadata_; }

    /**
     * Check if metadata is valid.
     */
    [[nodiscard]] bool isValid() const noexcept { return metadata_ != nullptr; }

    /**
     * Get number of fields/parameters.
     */
    [[nodiscard]] unsigned getCount(Firebird::IStatus* status) const {
        return metadata_ ? metadata_->getCount(status) : 0;
    }

    /**
     * Get total message buffer length.
     */
    [[nodiscard]] unsigned getMessageLength(Firebird::IStatus* status) const {
        return metadata_ ? metadata_->getMessageLength(status) : 0;
    }

    /**
     * Get field name at index.
     */
    [[nodiscard]] std::string_view getFieldName(Firebird::IStatus* status, unsigned index) const {
        if (!metadata_) return {};
        const char* name = metadata_->getField(status, index);
        return name ? std::string_view(name) : std::string_view{};
    }

    /**
     * Get field alias at index.
     */
    [[nodiscard]] std::string_view getAlias(Firebird::IStatus* status, unsigned index) const {
        if (!metadata_) return {};
        const char* alias = metadata_->getAlias(status, index);
        return alias ? std::string_view(alias) : std::string_view{};
    }

    /**
     * Get relation (table) name at index.
     */
    [[nodiscard]] std::string_view getRelation(Firebird::IStatus* status, unsigned index) const {
        if (!metadata_) return {};
        const char* rel = metadata_->getRelation(status, index);
        return rel ? std::string_view(rel) : std::string_view{};
    }

    /**
     * Get owner name at index.
     */
    [[nodiscard]] std::string_view getOwner(Firebird::IStatus* status, unsigned index) const {
        if (!metadata_) return {};
        const char* owner = metadata_->getOwner(status, index);
        return owner ? std::string_view(owner) : std::string_view{};
    }

    /**
     * Get SQL type at index.
     */
    [[nodiscard]] unsigned getType(Firebird::IStatus* status, unsigned index) const {
        return metadata_ ? metadata_->getType(status, index) : 0;
    }

    /**
     * Get sub-type at index.
     */
    [[nodiscard]] unsigned getSubType(Firebird::IStatus* status, unsigned index) const {
        return metadata_ ? metadata_->getSubType(status, index) : 0;
    }

    /**
     * Get field length at index.
     */
    [[nodiscard]] unsigned getLength(Firebird::IStatus* status, unsigned index) const {
        return metadata_ ? metadata_->getLength(status, index) : 0;
    }

    /**
     * Get numeric scale at index.
     */
    [[nodiscard]] int getScale(Firebird::IStatus* status, unsigned index) const {
        return metadata_ ? metadata_->getScale(status, index) : 0;
    }

    /**
     * Get character set ID at index.
     */
    [[nodiscard]] unsigned getCharSet(Firebird::IStatus* status, unsigned index) const {
        return metadata_ ? metadata_->getCharSet(status, index) : 0;
    }

    /**
     * Get offset in message buffer at index.
     */
    [[nodiscard]] unsigned getOffset(Firebird::IStatus* status, unsigned index) const {
        return metadata_ ? metadata_->getOffset(status, index) : 0;
    }

    /**
     * Get null indicator offset at index.
     */
    [[nodiscard]] unsigned getNullOffset(Firebird::IStatus* status, unsigned index) const {
        return metadata_ ? metadata_->getNullOffset(status, index) : 0;
    }

    /**
     * Check if field is nullable at index.
     */
    [[nodiscard]] bool isNullable(Firebird::IStatus* status, unsigned index) const {
        return metadata_ ? metadata_->isNullable(status, index) : false;
    }

    /**
     * Extract complete field information for all fields.
     * @param status Status wrapper for error handling
     * @return Vector of FieldInfo structures
     */
    [[nodiscard]] std::vector<FieldInfo> extractAllFields(Firebird::IStatus* status) const {
        std::vector<FieldInfo> fields;
        if (!metadata_) return fields;

        unsigned count = metadata_->getCount(status);
        fields.reserve(count);

        for (unsigned i = 0; i < count; ++i) {
            fields.push_back(extractField(status, i));
        }

        return fields;
    }

    /**
     * Extract field information at specific index.
     */
    [[nodiscard]] FieldInfo extractField(Firebird::IStatus* status, unsigned index) const {
        FieldInfo info;
        if (!metadata_) return info;

        // Names
        if (auto name = getFieldName(status, index); !name.empty()) {
            info.name = std::string(name);
        }
        if (auto alias = getAlias(status, index); !alias.empty()) {
            info.alias = std::string(alias);
        }
        if (auto rel = getRelation(status, index); !rel.empty()) {
            info.relation = std::string(rel);
        }
        if (auto owner = getOwner(status, index); !owner.empty()) {
            info.owner = std::string(owner);
        }

        // Type information
        info.type = getType(status, index);
        info.subType = getSubType(status, index);
        info.length = static_cast<int>(getLength(status, index));
        info.scale = getScale(status, index);
        info.charsetId = getCharSet(status, index);

        // Calculate precision for numeric types
        if (info.scale != 0) {
            switch (info.type & ~1) {
                case SQL_SHORT: info.precision = 4; break;
                case SQL_LONG: info.precision = 9; break;
                case SQL_INT64: info.precision = 18; break;
#if FB_API_VER >= 40
                case SQL_INT128: info.precision = 38; break;
#endif
                default: info.precision = 0;
            }
        }

        // Buffer offsets
        info.offset = getOffset(status, index);
        info.nullOffset = getNullOffset(status, index);
        info.nullable = isNullable(status, index);

        return info;
    }

private:
    Firebird::IMessageMetadata* metadata_;
    bool owns_;
};

/**
 * Helper to create a MetadataWrapper from statement output metadata.
 */
[[nodiscard]] inline std::optional<MetadataWrapper> getOutputMetadata(
    Firebird::IStatement* statement,
    Firebird::IStatus* status) {
    if (!statement) return std::nullopt;

    auto* metadata = statement->getOutputMetadata(status);
    // FB 4.0 compatible: use statusHasError() instead of hasData()
    if (!metadata || statusHasError(status)) {
        return std::nullopt;
    }

    return MetadataWrapper(metadata, true);
}

/**
 * Helper to create a MetadataWrapper from statement input metadata.
 */
[[nodiscard]] inline std::optional<MetadataWrapper> getInputMetadata(
    Firebird::IStatement* statement,
    Firebird::IStatus* status) {
    if (!statement) return std::nullopt;

    auto* metadata = statement->getInputMetadata(status);
    // FB 4.0 compatible: use statusHasError() instead of hasData()
    if (!metadata || statusHasError(status)) {
        return std::nullopt;
    }

    return MetadataWrapper(metadata, true);
}

/**
 * RAII wrapper for IMetadataBuilder (used to create custom metadata).
 */
class MetadataBuilder {
public:
    explicit MetadataBuilder(Firebird::IMaster* master, unsigned fieldCount)
        : master_(master), builder_(nullptr) {
        if (master_) {
            StatusWrapper status(master_);
            builder_ = master_->getMetadataBuilder(status.get(), fieldCount);
        }
    }

    ~MetadataBuilder() {
        if (builder_) {
            builder_->release();
        }
    }

    // Non-copyable
    MetadataBuilder(const MetadataBuilder&) = delete;
    MetadataBuilder& operator=(const MetadataBuilder&) = delete;

    // Movable
    MetadataBuilder(MetadataBuilder&& other) noexcept
        : master_(other.master_), builder_(other.builder_) {
        other.builder_ = nullptr;
    }

    MetadataBuilder& operator=(MetadataBuilder&& other) noexcept {
        if (this != &other) {
            if (builder_) builder_->release();
            master_ = other.master_;
            builder_ = other.builder_;
            other.builder_ = nullptr;
        }
        return *this;
    }

    /**
     * Set field type at index.
     */
    MetadataBuilder& setType(unsigned index, unsigned type) {
        if (builder_ && master_) {
            StatusWrapper status(master_);
            builder_->setType(status.get(), index, type);
        }
        return *this;
    }

    /**
     * Set field length at index.
     */
    MetadataBuilder& setLength(unsigned index, unsigned length) {
        if (builder_ && master_) {
            StatusWrapper status(master_);
            builder_->setLength(status.get(), index, length);
        }
        return *this;
    }

    /**
     * Set numeric scale at index.
     */
    MetadataBuilder& setScale(unsigned index, int scale) {
        if (builder_ && master_) {
            StatusWrapper status(master_);
            builder_->setScale(status.get(), index, scale);
        }
        return *this;
    }

    /**
     * Set character set at index.
     */
    MetadataBuilder& setCharSet(unsigned index, unsigned charSet) {
        if (builder_ && master_) {
            StatusWrapper status(master_);
            builder_->setCharSet(status.get(), index, charSet);
        }
        return *this;
    }

    /**
     * Set sub-type at index.
     */
    MetadataBuilder& setSubType(unsigned index, unsigned subType) {
        if (builder_ && master_) {
            StatusWrapper status(master_);
            builder_->setSubType(status.get(), index, subType);
        }
        return *this;
    }

    /**
     * Build the final metadata.
     * @return MetadataWrapper owning the built metadata, or nullopt on error
     */
    [[nodiscard]] std::optional<MetadataWrapper> build() {
        if (!builder_ || !master_) return std::nullopt;

        StatusWrapper status(master_);
        auto* metadata = builder_->getMetadata(status.get());

        if (!metadata || status.hasError()) {
            return std::nullopt;
        }

        return MetadataWrapper(metadata, true);
    }

    /**
     * Check if builder is valid.
     */
    [[nodiscard]] bool isValid() const noexcept { return builder_ != nullptr; }

private:
    Firebird::IMaster* master_;
    Firebird::IMetadataBuilder* builder_;
};

} // namespace fb

#endif // FB_METADATA_HPP
