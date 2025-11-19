# C++17 Implementation Roadmap - Step-by-Step Guide

## Implementation Order Strategy

The modernization follows a **bottom-up approach**: utility functions first, then higher-level APIs, maintaining strict backward compatibility at each step.

## Step 1: Foundation - RAII Wrapper Classes

### 1.1 FirebirdMasterWrapper Implementation

**File**: `firebird_utils.cpp` (add at top after includes)

```cpp
#if FB_API_VER >= 30

// Modern C++ RAII wrapper for Firebird IMaster interface
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
    
    template<typename T>
    T* getInterface() const noexcept {
        static_assert(std::is_pointer_v<T>, "T must be a pointer type");
        return static_cast<T>(master_);
    }
};

#endif // FB_API_VER >= 30
```

### 1.2 FirebirdStatusManager Implementation

```cpp
#if FB_API_VER >= 40

class FirebirdStatusManager {
private:
    ISC_STATUS* target_status_;
    std::unique_ptr<Firebird::ThrowStatusWrapper> status_wrapper_;
    
public:
    explicit FirebirdStatusManager(ISC_STATUS* target_status, Firebird::IMaster* master) 
        : target_status_(target_status),
          status_wrapper_(std::make_unique<Firebird::ThrowStatusWrapper>(master->getStatus())) {
    }
    
    ~FirebirdStatusManager() {
        if (status_wrapper_->hasData() && target_status_) {
            copy_status_safe(status_wrapper_->getErrors(), target_status_);
        }
    }
    
    Firebird::ThrowStatusWrapper* get() noexcept {
        return status_wrapper_.get();
    }
    
    bool hasError() const noexcept {
        return status_wrapper_->hasData();
    }

private:
    void copy_status_safe(const ISC_STATUS* from, ISC_STATUS* to) noexcept {
        constexpr size_t MAX_STATUS_LENGTH = 20;
        
        for (size_t i = 0; i < MAX_STATUS_LENGTH; ++i) {
            to[i] = from[i];
            if (from[i] == isc_arg_end) break;
        }
    }
};

#endif // FB_API_VER >= 40
```

## Step 2: Modernize Core Utility Functions

### 2.1 Replace fbu_copy_status with Modern Implementation

**Current Function:**
```cpp
static void fbu_copy_status(const ISC_STATUS* from, ISC_STATUS* to, size_t maxLength)
{
    for(size_t i=0; i < maxLength; ++i) {
        memcpy(to + i, from + i, sizeof(ISC_STATUS));
        if (from[i] == isc_arg_end) {
            break;
        }
    }
}
```

**Modernized Replacement:**
```cpp
namespace {
    constexpr size_t DEFAULT_STATUS_VECTOR_SIZE = 20;
    
    // C++17: constexpr + std::span for safe array operations
    void copy_status_vector(std::span<const ISC_STATUS> source, 
                           std::span<ISC_STATUS> destination) noexcept {
        const auto copy_count = std::min(source.size(), destination.size());
        
        for (size_t i = 0; i < copy_count; ++i) {
            destination[i] = source[i];
            if (source[i] == isc_arg_end) break;
        }
    }
    
    // Wrapper for legacy compatibility
    void copy_status_legacy(const ISC_STATUS* from, ISC_STATUS* to, size_t max_length) noexcept {
        copy_status_vector(
            std::span<const ISC_STATUS>(from, max_length),
            std::span<ISC_STATUS>(to, max_length)
        );
    }
}
```

### 2.2 Version-Aware Templates with if constexpr

```cpp
namespace detail {
    template<int ApiVersion = FB_API_VER>
    class FirebirdApiTraits {
    public:
        static constexpr bool has_timezone_support() noexcept {
            return ApiVersion >= 40;
        }
        
        static constexpr bool has_enhanced_metadata() noexcept {
            return ApiVersion >= 30;
        }
        
        template<typename Func, typename... Args>
        static auto call_if_supported(Func&& func, Args&&... args) noexcept {
            if constexpr (has_timezone_support()) {
                return func(std::forward<Args>(args)...);
            } else {
                return std::nullopt;
            }
        }
    };
}
```

## Step 3: Refactor Each Public Function

### 3.1 fbu_get_client_version Modernization

**Target Function:**
```cpp
extern "C" unsigned fbu_get_client_version(void *master_ptr)
```

**Implementation Strategy:**
1. Create internal C++ function with std::optional
2. Keep extern "C" wrapper for compatibility
3. Add error handling and validation

**Complete Implementation:**
```cpp
namespace {
    // Internal C++17 implementation
    std::optional<unsigned> get_client_version_impl(void* master_ptr) noexcept {
        if (!master_ptr) {
            return std::nullopt;
        }
        
        try {
            FirebirdMasterWrapper master(master_ptr);
            auto* util = master.getUtil();
            
            if (!util) {
                return std::nullopt;
            }
            
            return util->getClientVersion();
            
        } catch (...) {
            return std::nullopt;
        }
    }
}

// Updated extern "C" function
extern "C" unsigned fbu_get_client_version(void *master_ptr)
{
    auto version = get_client_version_impl(master_ptr);
    return version.value_or(0);
}
```

### 3.2 fbu_encode_time/date Modernization

**Target Functions:**
```cpp
extern "C" ISC_TIME fbu_encode_time(void *master_ptr, unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions)
extern "C" ISC_DATE fbu_encode_date(void *master_ptr, unsigned year, unsigned month, unsigned day)
```

**Modernized Implementation:**
```cpp
namespace {
    struct TimeComponents {
        unsigned hours, minutes, seconds, fractions;
        
        bool is_valid() const noexcept {
            return hours <= 23 && minutes <= 59 && seconds <= 59 && fractions <= 9999;
        }
    };
    
    struct DateComponents {
        unsigned year, month, day;
        
        bool is_valid() const noexcept {
            return year >= 1 && year <= 9999 &&
                   month >= 1 && month <= 12 &&
                   day >= 1 && day <= 31;
        }
    };
    
    // Internal C++17 implementations
    std::optional<ISC_TIME> encode_time_impl(void* master_ptr, const TimeComponents& components) noexcept {
        if (!master_ptr || !components.is_valid()) {
            return std::nullopt;
        }
        
        try {
            FirebirdMasterWrapper master(master_ptr);
            auto* util = master.getUtil();
            
            return util->encodeTime(components.hours, components.minutes, 
                                  components.seconds, components.fractions);
        } catch (...) {
            return std::nullopt;
        }
    }
    
    std::optional<ISC_DATE> encode_date_impl(void* master_ptr, const DateComponents& components) noexcept {
        if (!master_ptr || !components.is_valid()) {
            return std::nullopt;
        }
        
        try {
            FirebirdMasterWrapper master(master_ptr);
            auto* util = master.getUtil();
            
            return util->encodeDate(components.year, components.month, components.day);
        } catch (...) {
            return std::nullopt;
        }
    }
}

// Updated extern "C" functions
extern "C" ISC_TIME fbu_encode_time(void *master_ptr, unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions)
{
    TimeComponents components{hours, minutes, seconds, fractions};
    auto result = encode_time_impl(master_ptr, components);
    return result.value_or(0);
}

extern "C" ISC_DATE fbu_encode_date(void *master_ptr, unsigned year, unsigned month, unsigned day)
{
    DateComponents components{year, month, day};
    auto result = encode_date_impl(master_ptr, components);
    return result.value_or(0);
}
```

### 3.3 Complex Functions with Structured Bindings

**Target Function:**
```cpp
extern "C" void fbu_decode_timestamp_tz(void *master_ptr, const ISC_TIMESTAMP_TZ* timestampTz,
    unsigned* year, unsigned* month, unsigned* day,
    unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions,
    unsigned timeZoneBufferLength, char* timeZoneBuffer)
```

**Modernized Implementation with Structured Bindings:**
```cpp
#if FB_API_VER >= 40

namespace {
    struct DecodedTimestampTz {
        unsigned year, month, day;
        unsigned hours, minutes, seconds, fractions;
        std::string timeZone;
        
        // C++17: Structured binding support
        auto tie() const noexcept {
            return std::tie(year, month, day, hours, minutes, seconds, fractions);
        }
    };
    
    // Internal C++17 implementation using structured bindings
    std::optional<DecodedTimestampTz> decode_timestamp_tz_impl(
        void* master_ptr, const ISC_TIMESTAMP_TZ* timestampTz) noexcept {
        
        if (!master_ptr || !timestampTz) {
            return std::nullopt;
        }
        
        try {
            FirebirdMasterWrapper master(master_ptr);
            FirebirdStatusManager status_mgr(nullptr, master.getMaster());
            
            auto* util = master.getUtil();
            
            // Temporary storage for decode operation
            unsigned year, month, day, hours, minutes, seconds, fractions;
            constexpr size_t TZ_BUFFER_SIZE = 64;
            std::array<char, TZ_BUFFER_SIZE> tz_buffer{};
            
            util->decodeTimeStampTz(status_mgr.get(), timestampTz,
                                   &year, &month, &day,
                                   &hours, &minutes, &seconds, &fractions,
                                   TZ_BUFFER_SIZE, tz_buffer.data());
            
            if (status_mgr.hasError()) {
                return std::nullopt;
            }
            
            // C++17: Structured binding return
            return DecodedTimestampTz{
                .year = year, .month = month, .day = day,
                .hours = hours, .minutes = minutes, .seconds = seconds, .fractions = fractions,
                .timeZone = std::string(tz_buffer.data())
            };
            
        } catch (...) {
            return std::nullopt;
        }
    }
}

// Updated extern "C" function - maintains exact signature
extern "C" void fbu_decode_timestamp_tz(void *master_ptr, const ISC_TIMESTAMP_TZ* timestampTz,
    unsigned* year, unsigned* month, unsigned* day,
    unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions,
    unsigned timeZoneBufferLength, char* timeZoneBuffer)
{
    auto decoded = decode_timestamp_tz_impl(master_ptr, timestampTz);
    
    if (decoded.has_value()) {
        // C++17: Structured binding assignment
        auto [y, m, d, h, min, s, f] = decoded->tie();
        
        if (year) *year = y;
        if (month) *month = m;
        if (day) *day = d;
        if (hours) *hours = h;
        if (minutes) *minutes = min;
        if (seconds) *seconds = s;
        if (fractions) *fractions = f;
        
        if (timeZoneBuffer && timeZoneBufferLength > 0) {
            const auto& tz = decoded->timeZone;
            const size_t copy_size = std::min(static_cast<size_t>(timeZoneBufferLength - 1), tz.size());
            std::copy_n(tz.begin(), copy_size, timeZoneBuffer);
            timeZoneBuffer[copy_size] = '\0';
        }
    } else {
        // Error case - set all outputs to zero/empty
        if (year) *year = 0;
        if (month) *month = 0;
        if (day) *day = 0;
        if (hours) *hours = 0;
        if (minutes) *minutes = 0;
        if (seconds) *seconds = 0;
        if (fractions) *fractions = 0;
        if (timeZoneBuffer && timeZoneBufferLength > 0) {
            timeZoneBuffer[0] = '\0';
        }
    }
}

#endif // FB_API_VER >= 40
```

## Step 4: Advanced Pattern - Field Info Modernization

### 4.1 Complete fbu_insert_field_info Refactor

**Target Function:**
```cpp
extern "C" int fbu_insert_field_info(void *master_ptr, ISC_STATUS* st, int is_outvar, int num,
    zval *into_array, void *statement_ptr)
```

**Full Modern Implementation:**
```cpp
#if FB_API_VER >= 40

namespace {
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
        
        Firebird::IMessageMetadata* get() const noexcept { return metadata_; }
        
        std::string_view getFieldName(Firebird::IStatus* status, unsigned index) const {
            if (!metadata_) return {};
            
            const char* name = metadata_->getField(status, index);
            return name ? std::string_view(name) : std::string_view{};
        }
        
        std::string_view getAlias(Firebird::IStatus* status, unsigned index) const {
            if (!metadata_) return {};
            
            const char* alias = metadata_->getAlias(status, index);
            return alias ? std::string_view(alias) : std::string_view{};
        }
        
        std::string_view getRelation(Firebird::IStatus* status, unsigned index) const {
            if (!metadata_) return {};
            
            const char* relation = metadata_->getRelation(status, index);
            return relation ? std::string_view(relation) : std::string_view{};
        }
    };
    
    // Internal C++17 implementation
    int insert_field_info_modern(FirebirdMasterWrapper& master, ISC_STATUS* status_vec,
                                bool is_output_var, int field_num, zval* target_array,
                                Firebird::IStatement* statement) noexcept {
        try {
            FirebirdStatusManager status_mgr(status_vec, master.getMaster());
            
            // Get appropriate metadata
            auto* metadata = is_output_var 
                ? statement->getOutputMetadata(status_mgr.get())
                : statement->getInputMetadata(status_mgr.get());
                
            if (!metadata) {
                return -1;
            }
            
            FirebirdMetadataWrapper meta_wrapper(metadata, true);
            
            // C++17: Use string_view for efficient string handling
            const auto field_name = meta_wrapper.getFieldName(status_mgr.get(), field_num);
            const auto alias_name = meta_wrapper.getAlias(status_mgr.get(), field_num);
            const auto relation_name = meta_wrapper.getRelation(status_mgr.get(), field_num);
            
            if (status_mgr.hasError()) {
                return -1;
            }
            
            // Add to PHP array (maintaining exact original behavior)
            add_index_stringl(target_array, 0, field_name.data(), field_name.size());
            add_assoc_stringl(target_array, "name", field_name.data(), field_name.size());
            
            add_index_stringl(target_array, 1, alias_name.data(), alias_name.size());
            add_assoc_stringl(target_array, "alias", alias_name.data(), alias_name.size());
            
            add_index_stringl(target_array, 2, relation_name.data(), relation_name.size());
            add_assoc_stringl(target_array, "relation", relation_name.data(), relation_name.size());
            
            return 0;
            
        } catch (...) {
            return -1;
        }
    }
}

// Updated extern "C" function maintains exact signature
extern "C" int fbu_insert_field_info(void *master_ptr, ISC_STATUS* st, int is_outvar, int num,
    zval *into_array, void *statement_ptr)
{
    if (!master_ptr || !statement_ptr || !into_array) {
        return -1;
    }
    
    FirebirdMasterWrapper master(master_ptr);
    auto* statement = static_cast<Firebird::IStatement*>(statement_ptr);
    
    return insert_field_info_modern(master, st, is_outvar != 0, num, into_array, statement);
}

#endif // FB_API_VER >= 40
```

## Step 5: Performance Optimizations

### 5.1 String View Optimizations

```cpp
namespace string_utils {
    // C++17: Efficient string processing with string_view
    void add_php_string_from_view(zval* array, const char* key, std::string_view value) noexcept {
        add_assoc_stringl(array, key, value.data(), value.size());
    }
    
    void add_php_string_indexed(zval* array, int index, std::string_view value) noexcept {
        add_index_stringl(array, index, value.data(), value.size());
    }
    
    // C++17: Compile-time string validation
    template<size_t N>
    constexpr bool validate_string_literal(const char (&str)[N]) noexcept {
        return N > 1; // At least one character + null terminator
    }
}
```

### 5.2 Move Semantics for Resource Classes

```cpp
class FirebirdResourceBundle {
private:
    std::unique_ptr<FirebirdMasterWrapper> master_;
    std::unique_ptr<FirebirdStatusManager> status_;
    
public:
    FirebirdResourceBundle(void* master_ptr, ISC_STATUS* status_vec)
        : master_(std::make_unique<FirebirdMasterWrapper>(master_ptr)),
          status_(std::make_unique<FirebirdStatusManager>(status_vec, master_->getMaster())) {}
    
    // C++17: Move semantics
    FirebirdResourceBundle(FirebirdResourceBundle&&) = default;
    FirebirdResourceBundle& operator=(FirebirdResourceBundle&&) = default;
    
    // Non-copyable
    FirebirdResourceBundle(const FirebirdResourceBundle&) = delete;
    FirebirdResourceBundle& operator=(const FirebirdResourceBundle&) = delete;
    
    auto& getMaster() noexcept { return *master_; }
    auto& getStatus() noexcept { return *status_; }
};
```

## Testing Strategy

### Unit Tests for Modern Functions

**File**: `tests/cpp17_modernization_test.cpp` (new file)

```cpp
#include <gtest/gtest.h>
#include "../firebird_utils.h"

class FirebirdUtilsModernTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Mock Firebird master interface setup
    }
    
    void TearDown() override {
        // Cleanup
    }
};

TEST_F(FirebirdUtilsModernTest, ClientVersionReturnsValidValue) {
    void* mock_master = create_mock_master();
    unsigned version = fbu_get_client_version(mock_master);
    
    EXPECT_GT(version, 0);
    EXPECT_LT(version, 0xFFFFFFFF);
}

TEST_F(FirebirdUtilsModernTest, ClientVersionHandlesNullPointer) {
    unsigned version = fbu_get_client_version(nullptr);
    EXPECT_EQ(version, 0);
}

TEST_F(FirebirdUtilsModernTest, EncodeTimeValidatesInput) {
    void* mock_master = create_mock_master();
    
    // Valid time
    ISC_TIME valid_time = fbu_encode_time(mock_master, 10, 30, 45, 123);
    EXPECT_NE(valid_time, 0);
    
    // Invalid time (25 hours)
    ISC_TIME invalid_time = fbu_encode_time(mock_master, 25, 30, 45, 123);
    EXPECT_EQ(invalid_time, 0);
}
```

### Integration Tests

**File**: `tests/integration/firebird_compatibility_test.php`

```php
<?php
class FirebirdCompatibilityTest extends PHPUnit\Framework\TestCase
{
    public function testModernizedFunctionsPreserveCompatibility()
    {
        $connection = ibase_connect('test.fdb', 'sysdba', 'masterkey');
        $this->assertNotFalse($connection, 'Could not connect to test database');
        
        // Test client version function
        $version = fbu_get_client_version($connection);
        $this->assertGreaterThan(0, $version, 'Client version should be positive');
        
        // Test encode/decode functions
        $encoded_time = fbu_encode_time($connection, 10, 30, 45, 123);
        $this->assertNotEquals(0, $encoded_time, 'Encoded time should not be zero');
    }
}
```

## Validation Checklist

### Performance Validation:
- [ ] Benchmark all modernized functions vs original
- [ ] Memory usage profiling (no regressions)
- [ ] Compilation time impact assessment
- [ ] Runtime performance with realistic workloads

### Compatibility Validation:
- [ ] All existing PHP tests pass without modification
- [ ] Binary compatibility maintained (same function signatures)
- [ ] Cross-platform compilation (Linux, Windows, macOS)
- [ ] Firebird 2.5-5.0 compatibility confirmed

### Code Quality Validation:
- [ ] Static analysis tools show improved scores
- [ ] Memory safety tools (Valgrind) show no issues
- [ ] Address sanitizer reports clean
- [ ] Thread safety characteristics preserved

## Implementation Timeline

**Week 1: Foundation Classes**
- Create RAII wrapper classes
- Implement basic exception safety
- Unit tests for wrapper classes

**Week 2: Core Function Modernization**
- Modernize fbu_copy_status and utilities
- Update fbu_get_client_version
- Update fbu_encode_time/date functions

**Week 3: Complex Function Refactoring**
- Modernize fbu_decode_timestamp_tz with structured bindings
- Modernize fbu_insert_field_info with RAII
- Update fbu_insert_aliases

**Week 4: Testing and Validation**
- Comprehensive testing suite
- Performance benchmarking
- Cross-platform validation
- Static analysis integration

## Success Metrics

### Quality Improvements:
- **Memory safety**: Zero memory leaks detected by Valgrind
- **Exception safety**: Strong exception guarantees implemented
- **Type safety**: Compile-time error detection improved
- **Code clarity**: Reduced cyclomatic complexity

### Performance Metrics:
- **No performance regression**: <1% deviation from baseline
- **Memory usage**: Equal or improved memory patterns  
- **Compilation time**: <10% increase acceptable for C++17 features
- **Binary size**: Monitor but secondary to safety/maintainability

### Compatibility Validation:
- **PHP tests**: 100% existing test suite passes
- **API compatibility**: All function signatures preserved
- **Firebird compatibility**: All supported versions work correctly
- **Cross-platform**: Linux, Windows, macOS compilation success

This roadmap provides specific, actionable steps for the C++17 modernization while ensuring full backward compatibility and improved code quality.
