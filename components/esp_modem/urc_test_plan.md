# ESP Modem Enhanced URC Interface Test Plan

## Overview

This test plan covers comprehensive testing of the enhanced URC (Unsolicited Result Code) interface with buffer consumption control. The tests validate both the new enhanced interface and backward compatibility with the existing legacy interface.

## Test Environment Setup

### Prerequisites
- ESP-IDF development environment
- ESP32/ESP32-S3 development board
- UART connection to modem or ESP-AT device
- Test modem or ESP-AT firmware

### Configuration
- `CONFIG_ESP_MODEM_URC_HANDLER=y` (enabled)
- `CONFIG_ESP_MODEM_USE_INFLATABLE_BUFFER_IF_NEEDED=y` (if available)
- UART configuration: 115200 baud, 8N1

## Test Categories

### 1. Enhanced URC Interface Tests

#### Test 1.1: Basic Enhanced URC Registration
**Objective**: Verify enhanced URC handler can be registered and called
**Steps**:
1. Create DCE instance
2. Register enhanced URC handler using `set_enhanced_urc()`
3. Send AT command that generates URC
4. Verify handler is called with correct buffer information

**Expected Results**:
- Handler receives `UrcBufferInfo` with correct buffer pointers
- `buffer_start` points to beginning of buffer
- `new_data_start` points to new data
- `new_data_size` reflects actual new data size
- `is_command_active` reflects command state

**Test Data**:
```cpp
// Test handler
UrcConsumeInfo test_handler(const UrcBufferInfo& info) {
    // Verify buffer info is correct
    assert(info.buffer_start != nullptr);
    assert(info.new_data_size > 0);
    return {UrcConsumeResult::CONSUME_NONE, 0};
}
```

#### Test 1.2: CONSUME_NONE Behavior
**Objective**: Verify CONSUME_NONE doesn't consume buffer data
**Steps**:
1. Register enhanced URC handler that returns `CONSUME_NONE`
2. Send command that generates URC
3. Verify buffer data is preserved for command processing
4. Verify command response is still processed correctly

**Expected Results**:
- URC handler is called but no data is consumed
- Command processing continues normally
- No data loss or corruption

**Test Data**:
```cpp
UrcConsumeInfo consume_none_handler(const UrcBufferInfo& info) {
    // Process URC but don't consume
    return {UrcConsumeResult::CONSUME_NONE, 0};
}
```

#### Test 1.3: CONSUME_PARTIAL Behavior
**Objective**: Verify CONSUME_PARTIAL consumes only specified amount
**Steps**:
1. Register enhanced URC handler that returns `CONSUME_PARTIAL`
2. Send command that generates URC followed by command response
3. Verify only URC part is consumed
4. Verify remaining data is processed by command handler

**Expected Results**:
- Only specified amount of data is consumed
- Remaining data is available for command processing
- Buffer pointers are adjusted correctly

**Test Data**:
```cpp
UrcConsumeInfo consume_partial_handler(const UrcBufferInfo& info) {
    std::string_view buffer((const char*)info.buffer_start, info.buffer_total_size);
    size_t urc_end = buffer.find("\r\n");
    if (urc_end != std::string_view::npos) {
        return {UrcConsumeResult::CONSUME_PARTIAL, urc_end + 2};
    }
    return {UrcConsumeResult::CONSUME_NONE, 0};
}
```

#### Test 1.4: CONSUME_ALL Behavior
**Objective**: Verify CONSUME_ALL consumes entire buffer
**Steps**:
1. Register enhanced URC handler that returns `CONSUME_ALL`
2. Send command that generates URC
3. Verify entire buffer is consumed
4. Verify command processing is terminated

**Expected Results**:
- Entire buffer is consumed
- Command processing stops
- No further processing occurs

**Test Data**:
```cpp
UrcConsumeInfo consume_all_handler(const UrcBufferInfo& info) {
    // Consume everything
    return {UrcConsumeResult::CONSUME_ALL, 0};
}
```

#### Test 1.5: Multi-part Response Handling
**Objective**: Verify handling of responses that arrive in multiple chunks
**Steps**:
1. Register enhanced URC handler for multi-part responses
2. Send command that generates large response
3. Verify handler is called multiple times with correct data
4. Verify partial consumption works correctly

**Expected Results**:
- Handler called multiple times as data arrives
- `processed_offset` correctly tracks what's been processed
- `new_data_size` reflects only new data
- Partial consumption works across multiple calls

**Test Data**:
```cpp
UrcConsumeInfo multi_part_handler(const UrcBufferInfo& info) {
    std::string_view new_data((const char*)info.new_data_start, info.new_data_size);

    // Look for complete response
    if (new_data.find("OK\r\n") != std::string_view::npos) {
        return {UrcConsumeResult::CONSUME_ALL, 0};
    }

    // Still waiting for completion
    return {UrcConsumeResult::CONSUME_NONE, 0};
}
```

#### Test 1.6: Command State Awareness
**Objective**: Verify URC handlers know when commands are active
**Steps**:
1. Register enhanced URC handler that checks `is_command_active`
2. Send command and verify handler knows command is active
3. Wait for command completion and verify handler knows command is inactive
4. Send URC without command and verify handler knows no command is active

**Expected Results**:
- `is_command_active` correctly reflects command state
- Handler can make decisions based on command state
- State transitions are accurate

**Test Data**:
```cpp
UrcConsumeInfo command_aware_handler(const UrcBufferInfo& info) {
    if (info.is_command_active) {
        // Don't process URCs during command processing
        return {UrcConsumeResult::CONSUME_NONE, 0};
    }

    // Process URCs when no command is active
    return {UrcConsumeResult::CONSUME_ALL, 0};
}
```

### 2. Backward Compatibility Tests

#### Test 2.1: Legacy URC Handler Still Works
**Objective**: Verify existing URC handlers continue to work
**Steps**:
1. Use existing `set_urc()` method with legacy handler
2. Send command that generates URC
3. Verify legacy handler is called correctly
4. Verify behavior matches previous implementation

**Expected Results**:
- Legacy handler is called with same signature
- Behavior is identical to previous implementation
- No regression in functionality

**Test Data**:
```cpp
// Legacy handler
command_result legacy_handler(uint8_t *data, size_t len) {
    // Process URC data
    return command_result::TIMEOUT;
}
```

#### Test 2.2: Legacy and Enhanced Handlers Coexistence
**Objective**: Verify both handlers can be used (enhanced takes precedence)
**Steps**:
1. Register both legacy and enhanced URC handlers
2. Send command that generates URC
3. Verify only enhanced handler is called
4. Remove enhanced handler and verify legacy handler is called

**Expected Results**:
- Enhanced handler takes precedence when both are registered
- Legacy handler is called when enhanced handler is removed
- No conflicts or errors

#### Test 2.3: Legacy Handler with Enhanced Interface Disabled
**Objective**: Verify legacy handler works when enhanced interface is disabled
**Steps**:
1. Compile with enhanced interface disabled (if possible)
2. Register legacy URC handler
3. Send command that generates URC
4. Verify legacy handler works normally

**Expected Results**:
- Legacy handler works when enhanced interface is disabled
- No compilation errors
- Behavior is identical to original implementation

### 3. Buffer Management Tests

#### Test 3.1: Buffer State Tracking
**Objective**: Verify buffer state is tracked correctly
**Steps**:
1. Register enhanced URC handler that logs buffer state
2. Send multiple commands and URCs
3. Verify buffer state tracking is accurate
4. Verify state is reset correctly

**Expected Results**:
- `total_processed` tracks total bytes processed
- `last_urc_processed` tracks URC processing offset
- `command_waiting` reflects command state
- State is consistent across operations

#### Test 3.2: Buffer Overflow Handling
**Objective**: Verify behavior with large responses
**Steps**:
1. Register enhanced URC handler
2. Send command that generates large response
3. Verify inflatable buffer is used if available
4. Verify no data loss or corruption

**Expected Results**:
- Large responses are handled correctly
- Inflatable buffer is used when needed
- No data loss or corruption
- Memory usage is reasonable

#### Test 3.3: Buffer Pointer Adjustment
**Objective**: Verify buffer pointers are adjusted correctly after partial consumption
**Steps**:
1. Register enhanced URC handler that consumes partial data
2. Send command that generates URC followed by response
3. Verify buffer pointers are adjusted correctly
4. Verify remaining data is processed correctly

**Expected Results**:
- Buffer pointers are adjusted after partial consumption
- Remaining data is processed correctly
- No pointer arithmetic errors
- Data integrity is maintained

### 4. Error Handling Tests

#### Test 4.1: Invalid Consumption Size
**Objective**: Verify handling of invalid consumption sizes
**Steps**:
1. Register enhanced URC handler that returns invalid consumption size
2. Send command that generates URC
3. Verify system handles invalid size gracefully
4. Verify no crashes or undefined behavior

**Expected Results**:
- Invalid consumption sizes are handled gracefully
- No crashes or undefined behavior
- System continues to function normally

#### Test 4.2: Handler Exception Handling
**Objective**: Verify system handles URC handler exceptions
**Steps**:
1. Register enhanced URC handler that throws exception
2. Send command that generates URC
3. Verify system handles exception gracefully
4. Verify system continues to function

**Expected Results**:
- Exceptions are caught and handled
- System continues to function normally
- No crashes or undefined behavior

#### Test 4.3: Null Handler Handling
**Objective**: Verify system handles null handlers correctly
**Steps**:
1. Register null enhanced URC handler
2. Send command that generates URC
3. Verify system handles null handler gracefully
4. Verify no crashes or undefined behavior

**Expected Results**:
- Null handlers are handled gracefully
- No crashes or undefined behavior
- System continues to function normally

### 5. Performance Tests

#### Test 5.1: Enhanced vs Legacy Performance
**Objective**: Compare performance of enhanced vs legacy interface
**Steps**:
1. Measure performance of legacy URC handler
2. Measure performance of enhanced URC handler
3. Compare overhead and processing time
4. Verify performance is acceptable

**Expected Results**:
- Enhanced interface has minimal overhead
- Performance is acceptable for real-time applications
- No significant performance regression

#### Test 5.2: Memory Usage
**Objective**: Verify memory usage is reasonable
**Steps**:
1. Measure memory usage with legacy interface
2. Measure memory usage with enhanced interface
3. Compare memory usage
4. Verify memory usage is reasonable

**Expected Results**:
- Memory usage is reasonable
- No significant memory overhead
- Memory usage is predictable

### 6. Integration Tests

#### Test 6.1: Real Modem Integration
**Objective**: Verify enhanced interface works with real modem
**Steps**:
1. Connect to real modem (SIM800, SIM7600, etc.)
2. Register enhanced URC handler
3. Send real AT commands that generate URCs
4. Verify URCs are handled correctly

**Expected Results**:
- Real URCs are handled correctly
- No data loss or corruption
- System works with real hardware

#### Test 6.2: ESP-AT Integration
**Objective**: Verify enhanced interface works with ESP-AT
**Steps**:
1. Connect to ESP-AT device
2. Register enhanced URC handler
3. Send HTTP commands that generate URCs
4. Verify URCs are handled correctly

**Expected Results**:
- ESP-AT URCs are handled correctly
- HTTP responses are processed correctly
- System works with ESP-AT

#### Test 6.3: Multiple URC Types
**Objective**: Verify handling of multiple URC types
**Steps**:
1. Register enhanced URC handler for multiple URC types
2. Send commands that generate different URC types
3. Verify all URC types are handled correctly
4. Verify no conflicts between URC types

**Expected Results**:
- All URC types are handled correctly
- No conflicts between URC types
- System is robust and reliable

## Test Implementation

### Test Framework
- Use ESP-IDF test framework
- Create test cases for each test scenario
- Use mock data for predictable testing
- Include real hardware tests where possible

### Test Data
- Create test URCs for various scenarios
- Use real modem responses where possible
- Include edge cases and error conditions
- Test with different buffer sizes

### Test Execution
- Run tests in isolated environment
- Verify each test passes independently
- Run tests multiple times for reliability
- Document any failures or issues

## Success Criteria

### Functional Requirements
- ✅ Enhanced URC interface works correctly
- ✅ All consumption modes work as expected
- ✅ Buffer state tracking is accurate
- ✅ Command state awareness works correctly
- ✅ Backward compatibility is maintained
- ✅ Error handling is robust

### Performance Requirements
- ✅ Performance overhead is minimal (< 5%)
- ✅ Memory usage is reasonable
- ✅ Real-time performance is maintained
- ✅ No memory leaks or corruption

### Reliability Requirements
- ✅ System handles errors gracefully
- ✅ No crashes or undefined behavior
- ✅ System is stable under stress
- ✅ Works with real hardware

## Test Results Documentation

### Test Report Template
For each test:
- Test ID and name
- Test objective
- Test steps executed
- Expected results
- Actual results
- Pass/Fail status
- Issues found (if any)
- Recommendations

### Issue Tracking
- Document all issues found
- Prioritize issues by severity
- Track issue resolution
- Verify fixes with regression testing

## Conclusion

This comprehensive test plan ensures the enhanced URC interface is thoroughly tested and validated before deployment. The tests cover functionality, performance, reliability, and backward compatibility to ensure a robust and reliable implementation.
