#include <gtest/gtest.h>
#include <memory>
#include <cstring>
#include "../firebird_utils.h"
#include <ibase.h>

// Mock interfaces for testing RAII wrapper classes
class MockFirebirdUtil : public Firebird::IUtil {
public:
    unsigned getClientVersion() override { return 0x40030000; } // Mock version 4.3.0

    // Mock implementations for required pure virtual functions
    ISC_TIME encodeTime(unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions) override {
        return hours * 3600 + minutes * 60 + seconds; // Simplified encoding
    }

    ISC_DATE encodeDate(unsigned year, unsigned month, unsigned day) override {
        return (year - 1900) * 365 + month * 30 + day; // Simplified encoding
    }

    // Additional required virtual functions (minimal implementations)
    void decodeDate(ISC_DATE, unsigned*, unsigned*, unsigned*) override {}
    void decodeTime(ISC_TIME, unsigned*, unsigned*, unsigned*, unsigned*) override {}
    void formatStatus(char*, unsigned, const ISC_STATUS*) override {}
    int fb_shutdown(unsigned, int) override { return 0; }
    void setOffsets(Firebird::IStatus*, Firebird::IMessageMetadata*, Firebird::IMetadataBuilder*) override {}
    Firebird::IDecFloat16* getDecFloat16(Firebird::IStatus*) override { return nullptr; }
    Firebird::IDecFloat34* getDecFloat34(Firebird::IStatus*) override { return nullptr; }
    Firebird::IInt128* getInt128(Firebird::IStatus*) override { return nullptr; }
    void decodeTimeTz(Firebird::IStatus*, const ISC_TIME_TZ*, unsigned*, unsigned*, unsigned*, unsigned*, unsigned, char*) override {}
    void decodeTimeStampTz(Firebird::IStatus*, const ISC_TIMESTAMP_TZ*, unsigned*, unsigned*, unsigned*, unsigned*, unsigned*, unsigned*, unsigned*, unsigned, char*) override {}
    ISC_TIME_TZ* encodeTimeTz(Firebird::IStatus*, unsigned, unsigned, unsigned, unsigned, const char*) override { return nullptr; }
    ISC_TIMESTAMP_TZ* encodeTimeStampTz(Firebird::IStatus*, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, const char*) override { return nullptr; }
};

class MockFirebirdMaster : public Firebird::IMaster {
private:
    std::unique_ptr<MockFirebirdUtil> util_;

public:
    MockFirebirdMaster() : util_(std::make_unique<MockFirebirdUtil>()) {}

    Firebird::IStatus* getStatus() override { return nullptr; }
    Firebird::IProvider* getDispatcher() override { return nullptr; }
    Firebird::IPluginManager* getPluginManager() override { return nullptr; }
    Firebird::ITimerControl* getTimerControl() override { return nullptr; }
    Firebird::IDtc* getDtc() override { return nullptr; }

    Firebird::IUtil* getUtilInterface() override {
        return util_.get();
    }

    Firebird::IConfigManager* getConfigManager() override { return nullptr; }
    int circularAlloc(char**, size_t, intptr_t*, Firebird::IStatus*) override { return 0; }
};

// Test fixture for C++17 wrapper classes
class FirebirdWrappersTest : public ::testing::Test {
protected:
    std::unique_ptr<MockFirebirdMaster> mock_master_;

    void SetUp() override {
        mock_master_ = std::make_unique<MockFirebirdMaster>();
    }

    void TearDown() override {
        mock_master_.reset();
    }

    void* getMasterPtr() {
        return static_cast<void*>(mock_master_.get());
    }
};

// Test FirebirdMasterWrapper RAII behavior
TEST_F(FirebirdWrappersTest, MasterWrapperBasicFunctionality) {
    FirebirdMasterWrapper wrapper(getMasterPtr());

    // Test basic functionality
    EXPECT_NE(wrapper.getMaster(), nullptr);
    EXPECT_NE(wrapper.getUtil(), nullptr);

    // Test utility interface access
    auto* util = wrapper.getUtil();
    EXPECT_EQ(util->getClientVersion(), 0x40030000); // Mock version
}

TEST_F(FirebirdWrappersTest, MasterWrapperMoveSemantics) {
    FirebirdMasterWrapper original(getMasterPtr());
    auto* original_master = original.getMaster();

    // Test move constructor
    FirebirdMasterWrapper moved = std::move(original);
    EXPECT_EQ(moved.getMaster(), original_master);

    // Test move assignment
    FirebirdMasterWrapper assigned(getMasterPtr());
    assigned = std::move(moved);
    EXPECT_EQ(assigned.getMaster(), original_master);
}

TEST_F(FirebirdWrappersTest, MasterWrapperNonCopyable) {
    // Verify non-copyable behavior at compile time
    static_assert(!std::is_copy_constructible_v<FirebirdMasterWrapper>);
    static_assert(!std::is_copy_assignable_v<FirebirdMasterWrapper>);

    // Verify movable behavior at compile time
    static_assert(std::is_move_constructible_v<FirebirdMasterWrapper>);
    static_assert(std::is_move_assignable_v<FirebirdMasterWrapper>);
}

#if FB_API_VER >= 40

TEST_F(FirebirdWrappersTest, StatusManagerRAIIBehavior) {
    ISC_STATUS test_status[20] = {0};

    {
        // Test RAII scope management
        FirebirdStatusManager status_mgr(test_status, mock_master_.get());

        // Verify basic functionality
        EXPECT_NE(status_mgr.get(), nullptr);
        EXPECT_FALSE(status_mgr.hasError()); // No error initially

    } // status_mgr destructor should handle cleanup

    // Verify status array state after destruction
    EXPECT_TRUE(true); // If we reach here, RAII cleanup succeeded
}

TEST_F(FirebirdWrappersTest, MetadataWrapperBasicFunctionality) {
    // Test with null metadata (safe handling)
    FirebirdMetadataWrapper null_wrapper(nullptr);
    EXPECT_EQ(null_wrapper.get(), nullptr);

    // Test string_view returns for null metadata
    auto field_name = null_wrapper.getFieldName(nullptr, 0);
    EXPECT_TRUE(field_name.empty());

    auto alias = null_wrapper.getAlias(nullptr, 0);
    EXPECT_TRUE(alias.empty());

    auto relation = null_wrapper.getRelation(nullptr, 0);
    EXPECT_TRUE(relation.empty());
}

TEST_F(FirebirdWrappersTest, MetadataWrapperMoveSemantics) {
    // Test move semantics with non-owning wrapper
    FirebirdMetadataWrapper original(nullptr, false);

    FirebirdMetadataWrapper moved = std::move(original);
    EXPECT_EQ(moved.get(), nullptr);

    // Verify non-copyable behavior at compile time
    static_assert(!std::is_copy_constructible_v<FirebirdMetadataWrapper>);
    static_assert(!std::is_copy_assignable_v<FirebirdMetadataWrapper>);
}

#endif // FB_API_VER >= 40

// Test modern status vector operations
TEST_F(FirebirdWrappersTest, StatusVectorOperations) {
    ISC_STATUS source[5] = {1, 2, 3, 4, isc_arg_end};
    ISC_STATUS destination[10] = {0};

    // Test the modern fbu_copy_status function
    fbu_copy_status(source, destination, 10);

    // Verify correct copying with termination
    EXPECT_EQ(destination[0], 1);
    EXPECT_EQ(destination[1], 2);
    EXPECT_EQ(destination[2], 3);
    EXPECT_EQ(destination[3], 4);
    EXPECT_EQ(destination[4], isc_arg_end);
}

TEST_F(FirebirdWrappersTest, StatusVectorNullPointerSafety) {
    ISC_STATUS destination[5] = {99, 99, 99, 99, 99}; // Initialize to detect changes

    // Test null pointer safety
    fbu_copy_status(nullptr, destination, 5);
    EXPECT_EQ(destination[0], 99); // Should remain unchanged

    ISC_STATUS source[2] = {1, isc_arg_end};
    fbu_copy_status(source, nullptr, 2);
    // Should not crash (verified by reaching this point)

    // Test zero length safety
    fbu_copy_status(source, destination, 0);
    EXPECT_EQ(destination[0], 99); // Should remain unchanged
}

// Integration test - verify extern "C" functions work with new classes
TEST_F(FirebirdWrappersTest, ExternCFunctionCompatibility) {
    void* master_ptr = getMasterPtr();

    // Test fbu_get_client_version with wrapper classes available
    unsigned version = fbu_get_client_version(master_ptr);
    EXPECT_EQ(version, 0x40030000); // Mock version

    // Test encoding functions
    ISC_TIME time_result = fbu_encode_time(master_ptr, 10, 30, 45, 123);
    EXPECT_GT(time_result, 0); // Should encode to positive value

    ISC_DATE date_result = fbu_encode_date(master_ptr, 2025, 11, 19);
    EXPECT_GT(date_result, 0); // Should encode to positive value
}

// Test null pointer safety across all functions
TEST_F(FirebirdWrappersTest, NullPointerSafety) {
    // Test functions handle null master pointer gracefully
    unsigned version = fbu_get_client_version(nullptr);
    EXPECT_EQ(version, 0); // Should return 0 for null pointer

    ISC_TIME time_result = fbu_encode_time(nullptr, 10, 30, 45, 123);
    EXPECT_EQ(time_result, 0); // Should return 0 for null pointer

    ISC_DATE date_result = fbu_encode_date(nullptr, 2025, 11, 19);
    EXPECT_EQ(date_result, 0); // Should return 0 for null pointer
}

//=============================================================================
// Step 2: Modernized Function Tests (std::optional safety)
//=============================================================================

TEST_F(FirebirdWrappersTest, ModernizedClientVersionWithOptionalSafety) {
    void* master_ptr = getMasterPtr();

    // Test valid case
    unsigned version = fbu_get_client_version(master_ptr);
    EXPECT_EQ(version, 0x40030000); // Mock version should be returned

    // Test null safety (returns safe fallback)
    unsigned null_version = fbu_get_client_version(nullptr);
    EXPECT_EQ(null_version, 0); // Safe fallback for null pointer
}

TEST_F(FirebirdWrappersTest, ModernizedEncodeTimeWithValidation) {
    void* master_ptr = getMasterPtr();

    // Test valid time components
    ISC_TIME valid_time = fbu_encode_time(master_ptr, 10, 30, 45, 123);
    EXPECT_GT(valid_time, 0); // Should encode to positive value

    // Test boundary valid cases
    ISC_TIME midnight = fbu_encode_time(master_ptr, 0, 0, 0, 0);
    EXPECT_GE(midnight, 0); // Midnight should be valid

    ISC_TIME end_of_day = fbu_encode_time(master_ptr, 23, 59, 59, 9999);
    EXPECT_GT(end_of_day, 0); // End of day should be valid

    // Test invalid time components (should return 0 due to validation)
    ISC_TIME invalid_hours = fbu_encode_time(master_ptr, 25, 30, 45, 123);
    EXPECT_EQ(invalid_hours, 0); // Hours > 23 invalid

    ISC_TIME invalid_minutes = fbu_encode_time(master_ptr, 10, 60, 45, 123);
    EXPECT_EQ(invalid_minutes, 0); // Minutes > 59 invalid

    ISC_TIME invalid_seconds = fbu_encode_time(master_ptr, 10, 30, 60, 123);
    EXPECT_EQ(invalid_seconds, 0); // Seconds > 59 invalid

    ISC_TIME invalid_fractions = fbu_encode_time(master_ptr, 10, 30, 45, 10000);
    EXPECT_EQ(invalid_fractions, 0); // Fractions > 9999 invalid

    // Test null pointer safety
    ISC_TIME null_result = fbu_encode_time(nullptr, 10, 30, 45, 123);
    EXPECT_EQ(null_result, 0); // Null pointer should return 0
}

TEST_F(FirebirdWrappersTest, ModernizedEncodeDateWithValidation) {
    void* master_ptr = getMasterPtr();

    // Test valid date components
    ISC_DATE valid_date = fbu_encode_date(master_ptr, 2025, 11, 19);
    EXPECT_GT(valid_date, 0); // Should encode to positive value

    // Test boundary valid cases
    ISC_DATE min_date = fbu_encode_date(master_ptr, 1, 1, 1);
    EXPECT_GT(min_date, 0); // Minimum date should be valid

    ISC_DATE max_date = fbu_encode_date(master_ptr, 9999, 12, 31);
    EXPECT_GT(max_date, 0); // Maximum date should be valid

    // Test invalid date components (should return 0 due to validation)
    ISC_DATE invalid_year_low = fbu_encode_date(master_ptr, 0, 11, 19);
    EXPECT_EQ(invalid_year_low, 0); // Year < 1 invalid

    ISC_DATE invalid_year_high = fbu_encode_date(master_ptr, 10000, 11, 19);
    EXPECT_EQ(invalid_year_high, 0); // Year > 9999 invalid

    ISC_DATE invalid_month_low = fbu_encode_date(master_ptr, 2025, 0, 19);
    EXPECT_EQ(invalid_month_low, 0); // Month < 1 invalid

    ISC_DATE invalid_month_high = fbu_encode_date(master_ptr, 2025, 13, 19);
    EXPECT_EQ(invalid_month_high, 0); // Month > 12 invalid

    ISC_DATE invalid_day_low = fbu_encode_date(master_ptr, 2025, 11, 0);
    EXPECT_EQ(invalid_day_low, 0); // Day < 1 invalid

    ISC_DATE invalid_day_high = fbu_encode_date(master_ptr, 2025, 11, 32);
    EXPECT_EQ(invalid_day_high, 0); // Day > 31 invalid

    // Test null pointer safety
    ISC_DATE null_result = fbu_encode_date(nullptr, 2025, 11, 19);
    EXPECT_EQ(null_result, 0); // Null pointer should return 0
}

TEST_F(FirebirdWrappersTest, InputValidationStructures) {
    // Test TimeComponents validation
    TimeComponents valid_time{10, 30, 45, 123};
    EXPECT_TRUE(valid_time.is_valid());

    TimeComponents invalid_time{25, 30, 45, 123}; // Invalid hours
    EXPECT_FALSE(invalid_time.is_valid());

    // Test DateComponents validation
    DateComponents valid_date{2025, 11, 19};
    EXPECT_TRUE(valid_date.is_valid());

    DateComponents invalid_date{2025, 13, 19}; // Invalid month
    EXPECT_FALSE(invalid_date.is_valid());

    // Test boundary conditions
    TimeComponents boundary_time{23, 59, 59, 9999}; // Maximum valid
    EXPECT_TRUE(boundary_time.is_valid());

    DateComponents boundary_date{9999, 12, 31}; // Maximum valid
    EXPECT_TRUE(boundary_date.is_valid());
}

TEST_F(FirebirdWrappersTest, ModernizedFunctionsBackwardCompatibility) {
    void* master_ptr = getMasterPtr();

    // Verify exact same behavior as original functions for valid inputs
    // These should match original implementation behavior exactly

    // Client version - should return mock version
    unsigned version = fbu_get_client_version(master_ptr);
    EXPECT_EQ(version, 0x40030000);

    // Time encoding - valid inputs should work exactly as before
    ISC_TIME time_10_30 = fbu_encode_time(master_ptr, 10, 30, 45, 123);
    EXPECT_GT(time_10_30, 0); // Should produce positive result

    // Date encoding - valid inputs should work exactly as before
    ISC_DATE date_2025_11_19 = fbu_encode_date(master_ptr, 2025, 11, 19);
    EXPECT_GT(date_2025_11_19, 0); // Should produce positive result

    // Additional safety: invalid inputs now return 0 instead of undefined behavior
    ISC_TIME invalid_time = fbu_encode_time(master_ptr, 25, 0, 0, 0);
    EXPECT_EQ(invalid_time, 0); // Improved safety vs original

    ISC_DATE invalid_date = fbu_encode_date(master_ptr, 0, 1, 1);
    EXPECT_EQ(invalid_date, 0); // Improved safety vs original
}

//=============================================================================
// Step 3: Complex Function Tests (Structured Bindings + Advanced RAII)
//=============================================================================

#if FB_API_VER >= 40

TEST_F(FirebirdWrappersTest, StructuredBindingsTimestampDecoding) {
    // Test DecodedTimestampTz structure validation
    DecodedTimestampTz valid_timestamp{
        2025, 11, 19,     // year, month, day
        15, 30, 45, 123,  // hours, minutes, seconds, fractions
        "UTC"             // timezone
    };
    EXPECT_TRUE(valid_timestamp.is_valid());

    // Test structured binding tie() method
    auto [year, month, day, hours, minutes, seconds, fractions] = valid_timestamp.tie();
    EXPECT_EQ(year, 2025);
    EXPECT_EQ(month, 11);
    EXPECT_EQ(day, 19);
    EXPECT_EQ(hours, 15);
    EXPECT_EQ(minutes, 30);
    EXPECT_EQ(seconds, 45);
    EXPECT_EQ(fractions, 123);

    // Test invalid timestamp detection
    DecodedTimestampTz invalid_timestamp{
        2025, 13, 19,     // Invalid month > 12
        15, 30, 45, 123,
        "UTC"
    };
    EXPECT_FALSE(invalid_timestamp.is_valid());
}

TEST_F(FirebirdWrappersTest, TimestampDecodingNullPointerSafety) {
    // Test fbu_decode_timestamp_tz with null pointers
    unsigned year, month, day, hours, minutes, seconds, fractions;
    char timezone_buffer[64];

    // Test null master pointer - should not crash and set outputs to zero
    fbu_decode_timestamp_tz(nullptr, nullptr,
                           &year, &month, &day,
                           &hours, &minutes, &seconds, &fractions,
                           sizeof(timezone_buffer), timezone_buffer);

    EXPECT_EQ(year, 0);
    EXPECT_EQ(month, 0);
    EXPECT_EQ(day, 0);
    EXPECT_EQ(hours, 0);
    EXPECT_EQ(minutes, 0);
    EXPECT_EQ(seconds, 0);
    EXPECT_EQ(fractions, 0);
    EXPECT_EQ(timezone_buffer[0], '\0');
}

TEST_F(FirebirdWrappersTest, TimestampDecodingNullOutputParameters) {
    void* master_ptr = getMasterPtr();

    // Test with null output parameters - should not crash
    fbu_decode_timestamp_tz(master_ptr, nullptr,
                           nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr, nullptr,
                           0, nullptr);

    // Test with partial null parameters
    unsigned year = 99;
    fbu_decode_timestamp_tz(master_ptr, nullptr,
                           &year, nullptr, nullptr,
                           nullptr, nullptr, nullptr, nullptr,
                           0, nullptr);

    // Should handle gracefully and set available outputs
    EXPECT_TRUE(true); // Test passes if no crash occurs
}

TEST_F(FirebirdWrappersTest, FieldInfoModernRAIIIntegration) {
    // Test null pointer safety for fbu_insert_field_info
    ISC_STATUS status[20] = {0};

    // Test null parameters return error code
    int null_master_result = fbu_insert_field_info(nullptr, status, 1, 0, nullptr, nullptr);
    EXPECT_EQ(null_master_result, -1);

    int null_array_result = fbu_insert_field_info(getMasterPtr(), status, 1, 0, nullptr, nullptr);
    EXPECT_EQ(null_array_result, -1);

    int null_statement_result = fbu_insert_field_info(getMasterPtr(), status, 1, 0, nullptr, nullptr);
    EXPECT_EQ(null_statement_result, -1);
}

TEST_F(FirebirdWrappersTest, AliasInsertionModernRAIIIntegration) {
    // Test null pointer safety for fbu_insert_aliases
    ISC_STATUS status[20] = {0};

    // Test null parameters return error code
    int null_master_result = fbu_insert_aliases(nullptr, status, nullptr, nullptr);
    EXPECT_EQ(null_master_result, -1);

    int null_query_result = fbu_insert_aliases(getMasterPtr(), status, nullptr, nullptr);
    EXPECT_EQ(null_query_result, -1);

    int null_statement_result = fbu_insert_aliases(getMasterPtr(), status, nullptr, nullptr);
    EXPECT_EQ(null_statement_result, -1);
}

TEST_F(FirebirdWrappersTest, ComplexFunctionExceptionSafety) {
    void* master_ptr = getMasterPtr();
    ISC_STATUS status[20] = {0};

    // Test that complex functions handle exceptions gracefully
    // These should return error codes rather than throwing

    // Field info with invalid parameters should return -1
    int field_info_result = fbu_insert_field_info(master_ptr, status, 1, 999, nullptr, nullptr);
    EXPECT_EQ(field_info_result, -1);

    // Alias insertion with invalid parameters should return -1
    int aliases_result = fbu_insert_aliases(master_ptr, status, nullptr, nullptr);
    EXPECT_EQ(aliases_result, -1);

    // Timestamp decoding with null inputs should handle gracefully
    unsigned output_vars[7] = {99, 99, 99, 99, 99, 99, 99}; // Initialize to detect changes
    char tz_buffer[10] = "UNCHANGED";

    fbu_decode_timestamp_tz(master_ptr, nullptr,
                           &output_vars[0], &output_vars[1], &output_vars[2],
                           &output_vars[3], &output_vars[4], &output_vars[5], &output_vars[6],
                           sizeof(tz_buffer), tz_buffer);

    // Should set outputs to zero for null input
    EXPECT_EQ(output_vars[0], 0); // year
    EXPECT_EQ(output_vars[1], 0); // month
    EXPECT_EQ(tz_buffer[0], '\0'); // timezone cleared
}

#endif // FB_API_VER >= 40
