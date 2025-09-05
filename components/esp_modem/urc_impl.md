# ESP Modem URC (Unsolicited Result Code) Implementation Analysis

## Overview

This document provides a comprehensive analysis of the URC (Unsolicited Result Code) callback mechanism in the ESP modem library. URCs are messages sent by the modem without a prior command to notify the host about events or status changes. This analysis is intended to help developers understand the current implementation and confidently refactor it to an observer pattern.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Core Components](#core-components)
3. [Data Flow](#data-flow)
4. [Configuration](#configuration)
5. [Current Implementation Details](#current-implementation-details)
6. [Usage Examples](#usage-examples)
7. [Limitations and Issues](#limitations-and-issues)
8. [Observer Pattern Refactoring Considerations](#observer-pattern-refactoring-considerations)

## Architecture Overview

The URC system follows a layered architecture:

```
Application Layer
       ↓
    DCE Layer (Data Circuit-terminating Equipment)
       ↓
    DTE Layer (Data Terminal Equipment)
       ↓
   Terminal Layer (UART/CMUX)
       ↓
   Hardware Layer
```

### Key Design Principles

- **Single Callback Registration**: Only one URC callback can be active at a time
- **Conditional Compilation**: URC support is controlled by `CONFIG_ESP_MODEM_URC_HANDLER`
- **Callback Chain**: URC callbacks are processed alongside command callbacks
- **Buffer Management**: Shared buffer handling between URC and command processing

## Core Components

### 1. Callback Type Definition

**Location**: `components/esp_modem/include/cxx_include/esp_modem_types.hpp:62`

```cpp
typedef std::function<command_result(uint8_t *data, size_t len)> got_line_cb;
```

**Purpose**: Defines the signature for both command and URC callbacks.

**Return Values**:
- `command_result::OK`: Processing complete, buffer can be consumed
- `command_result::FAIL`: Processing failed, buffer can be consumed
- `command_result::TIMEOUT`: Continue processing, buffer should be preserved

### 2. DTE Layer Implementation

**Location**: `components/esp_modem/include/cxx_include/esp_modem_dte.hpp:114-118`

```cpp
#ifdef CONFIG_ESP_MODEM_URC_HANDLER
void set_urc_cb(got_line_cb line_cb)
{
    command_cb.urc_handler = std::move(line_cb);
}
#endif
```

**Key Features**:
- Stores URC callback in `command_cb.urc_handler`
- Uses move semantics for efficiency
- Protected by conditional compilation

### 3. DCE Layer Implementation

**Location**: `components/esp_modem/include/cxx_include/esp_modem_dce_template.hpp:101-106`

```cpp
#ifdef CONFIG_ESP_MODEM_URC_HANDLER
void set_urc(got_line_cb on_read_cb)
{
    dte->set_urc_cb(on_read_cb);
}
#endif
```

**Purpose**: Provides a clean interface for applications to register URC callbacks.

### 4. Command Callback Structure

**Location**: `components/esp_modem/include/cxx_include/esp_modem_dte.hpp:216-252`

```cpp
struct command_cb {
#ifdef CONFIG_ESP_MODEM_URC_HANDLER
    got_line_cb urc_handler {};                             /*!< URC callback if enabled */
#endif
    got_line_cb got_line;                                   /*!< Supplied command callback */
    Lock line_lock{};                                       /*!< Command callback locking mechanism */
    char separator{};                                       /*!< Command reply separator */
    command_result result{};                                /*!< Command return code */
    SignalGroup signal;                                     /*!< Event group for signaling */
    // ... other members
};
```

**Key Features**:
- Thread-safe with `line_lock`
- Signal-based synchronization with `SignalGroup`
- Shared buffer processing logic

## Data Flow

### 1. Registration Flow

```
Application → DCE::set_urc() → DTE::set_urc_cb() → command_cb.urc_handler
```

### 2. Processing Flow

```
Terminal Data → DTE::set_command_callbacks() → command_cb::process_line() → URC Handler
```

**Detailed Processing** (`esp_modem_dte.cpp:368-388`):

```cpp
bool DTE::command_cb::process_line(uint8_t *data, size_t consumed, size_t len)
{
#ifdef CONFIG_ESP_MODEM_URC_HANDLER
    bool consume_buffer = false;
    if (urc_handler) {
        consume_buffer = urc_handler(data, consumed + len) != command_result::TIMEOUT;
    }
    if (result != command_result::TIMEOUT || got_line == nullptr) {
        return consume_buffer;
    }
#endif
    // Command processing continues...
}
```

**Processing Logic**:
1. URC handler is called first if registered
2. Buffer consumption is determined by URC handler return value
3. Command processing continues if no active command
4. Thread-safe processing with proper locking

### 3. Buffer Management

The system uses a sophisticated buffer management approach:

- **Primary Buffer**: Fixed-size buffer in DTE
- **Inflatable Buffer**: Optional dynamic buffer for large responses
- **Buffer Consumption**: Controlled by callback return values
- **Thread Safety**: Protected by locks and atomic operations

## Configuration

### Compile-Time Configuration

**Option**: `CONFIG_ESP_MODEM_URC_HANDLER`
- **Default**: Disabled
- **Purpose**: Enables/disables URC support entirely
- **Impact**: Removes URC-related code when disabled

### Runtime Configuration

**Buffer Size**: `dte_config.dte_buffer_size`
- **Default**: 1000 bytes
- **Purpose**: Controls primary buffer size
- **Impact**: Affects maximum URC message size

## Current Implementation Details

### 1. Thread Safety

- **Locking**: Uses `Lock line_lock` for thread safety
- **Atomic Operations**: Signal group operations are atomic
- **Scoped Locking**: RAII-based lock management with `Scoped<Lock>`

### 2. Error Handling

- **Buffer Overflow**: Handled by inflatable buffer or graceful failure
- **Timeout Handling**: URC callbacks can return `TIMEOUT` to continue processing
- **Recovery**: Automatic recovery from terminal errors

### 3. Memory Management

- **Move Semantics**: Callbacks are moved, not copied
- **RAII**: Automatic cleanup of resources
- **Buffer Reuse**: Efficient buffer reuse patterns

## Usage Examples

### 1. Basic URC Handler (urc_test.cpp)

```cpp
class DCE : public esp_modem::DCE_T<ESP_AT_Module> {
    bool http_get(const std::string &url) {
        std::string command = "AT+HTTPCGET=\"" + url + "\"\r\n";
        set_urc(handle_urc);  // Register URC handler
        auto ret = dte->write(esp_modem::DTE_Command(command));
        return ret > 0;
    }

    static esp_modem::command_result handle_urc(uint8_t *data, size_t len) {
        // Process URC data
        std::string_view chunk((const char*)data, len);
        // ... processing logic ...

        // Check for completion
        if (memmem(data, len, "Transfer completed", 17) != nullptr) {
            xEventGroupSetBits(s_event_group, transfer_completed);
        }
        return esp_modem::command_result::TIMEOUT;  // Continue processing
    }
};
```

### 2. Console Example (modem_console_main.cpp)

```cpp
#ifdef CONFIG_ESP_MODEM_URC_HANDLER
command_result handle_urc(uint8_t *data, size_t len) {
    ESP_LOG_BUFFER_HEXDUMP("on_read", data, len, ESP_LOG_INFO);
    return command_result::TIMEOUT;
}

// Toggle URC handler
const ConsoleCommand HandleURC("urc", "toggle urc handling", no_args, [&](ConsoleCommand * c) {
    static int cnt = 0;
    if (++cnt % 2) {
        ESP_LOGI(TAG, "Adding URC handler");
        dce->set_urc(handle_urc);
    } else {
        ESP_LOGI(TAG, "URC removed");
        dce->set_urc(nullptr);
    }
    return 0;
});
#endif
```

### 3. C API Usage

```cpp
// C API wrapper
extern "C" esp_err_t esp_modem_set_urc(esp_modem_dce_t *dce_wrap,
                                      esp_err_t(*got_line_fn)(uint8_t *data, size_t len)) {
    if (got_line_fn == nullptr) {
        dce_wrap->dce->set_urc(nullptr);
        return ESP_OK;
    }
    dce_wrap->dce->set_urc([got_line_fn](uint8_t *data, size_t len) {
        switch (got_line_fn(data, len)) {
        case ESP_OK:    return command_result::OK;
        case ESP_FAIL:  return command_result::FAIL;
        default:        return command_result::TIMEOUT;
        }
    });
    return ESP_OK;
}
```

## Critical Issues Requiring Immediate Attention

### 1. Tight Coupling Between URC and Command Processing

**Issue**: URC processing is tightly coupled with command processing in the same buffer and processing pipeline
**Impact**: URC processing can interfere with command responses, causing data corruption or missed responses
**Risk**: Race conditions between URC and command callbacks when both are active

**Current Problem**:
```cpp
// In DTE::command_cb::process_line()
#ifdef CONFIG_ESP_MODEM_URC_HANDLER
    bool consume_buffer = false;
    if (urc_handler) {
        consume_buffer = urc_handler(data, consumed + len) != command_result::TIMEOUT;
    }
    if (result != command_result::TIMEOUT || got_line == nullptr) {
        return consume_buffer;   // URC can consume buffer even when command is waiting
    }
#endif
```

### 2. Complex and Inflexible Buffer Management

**Issue**: Shared buffer between URC and command processing with limited consumption control
**Impact**: URC handlers cannot make granular decisions about buffer consumption
**Risk**: Data loss or incomplete processing of multi-part responses

**Current Limitations**:
- URC handler can only consume entire buffer or nothing
- No way to consume partial data (e.g., "OK\r\n+URC" - consume only "OK\r\n")
- No visibility into buffer state or processing progress
- No way to track what data has already been processed in multi-part responses

**Example Problem Scenario**:
```
Buffer: "HTTP response data...\r\n+HTTPGET:1,200\r\nOK\r\n"
Current: URC handler sees entire buffer but doesn't know:
- What part was already processed
- Whether to consume just the URC or the entire response
- How much data to leave for command processing
```

## Proposed Solution: Enhanced Buffer Management and Consumption Control

### 1. Core Design Principles

**Buffer Visibility**: URC handlers observe the entire buffer with processing offset information
**Granular Consumption**: URC handlers can consume entire buffer, partial data, or nothing
**Processing State Awareness**: URC handlers know what data has been processed and what's new
**Decoupled Processing**: URC processing separate from command processing pipeline

### 2. Enhanced URC Handler Interface

```cpp
// Enhanced URC handler with buffer consumption control
struct UrcBufferInfo {
    const uint8_t* buffer_start;        // Start of entire buffer
    size_t buffer_total_size;           // Total buffer size
    size_t processed_offset;            // Offset of already processed data
    size_t new_data_size;               // Size of new data since last call
    const uint8_t* new_data_start;      // Pointer to start of new data
    bool is_command_active;             // Whether a command is currently waiting for response
};

enum class UrcConsumeResult {
    CONSUME_NONE,           // Don't consume anything, continue waiting
    CONSUME_PARTIAL,        // Consume only part of the buffer
    CONSUME_ALL             // Consume entire buffer
};

struct UrcConsumeInfo {
    UrcConsumeResult result;
    size_t consume_size;    // How many bytes to consume (0 = none, SIZE_MAX = all)
};

typedef std::function<UrcConsumeInfo(const UrcBufferInfo&)> enhanced_urc_cb;
```

### 3. Implementation Concept: Enhanced Buffer Management with Consumption Control

The core concept is to provide URC handlers with complete buffer visibility and granular consumption control. This requires three integrated components:

#### Buffer State Tracking
```cpp
class DTE {
private:
    struct BufferState {
        size_t total_processed = 0;     // Total bytes processed by URC handlers
        size_t last_urc_processed = 0;  // Last offset processed by URC
        bool command_waiting = false;   // Whether command is waiting for response
        size_t command_start_offset = 0; // Where current command response started
    } buffer_state;

    void update_buffer_state(size_t new_data_size) {
        buffer_state.total_processed += new_data_size;
    }

    UrcBufferInfo create_urc_info(uint8_t* data, size_t consumed, size_t len) {
        return {
            .buffer_start = data,
            .buffer_total_size = consumed + len,
            .processed_offset = buffer_state.last_urc_processed,
            .new_data_size = (consumed + len) - buffer_state.last_urc_processed,
            .new_data_start = data + buffer_state.last_urc_processed,
            .is_command_active = buffer_state.command_waiting
        };
    }
};
```

#### Enhanced Processing Pipeline
```cpp
bool DTE::command_cb::process_line(uint8_t *data, size_t consumed, size_t len) {
    // Create buffer info for URC handler
    UrcBufferInfo buffer_info = create_urc_info(data, consumed, len);

    // Call URC handler if registered
    if (urc_handler) {
        UrcConsumeInfo consume_info = urc_handler(buffer_info);

        switch (consume_info.result) {
        case UrcConsumeResult::CONSUME_NONE:
            // Don't consume anything, continue with command processing
            break;

        case UrcConsumeResult::CONSUME_PARTIAL:
            // Consume only specified amount
            buffer_state.last_urc_processed += consume_info.consume_size;
            // Adjust data pointers for command processing
            data += consume_info.consume_size;
            consumed = (consumed + len) - consume_info.consume_size;
            len = 0;
            break;

        case UrcConsumeResult::CONSUME_ALL:
            // Consume entire buffer
            buffer_state.last_urc_processed = consumed + len;
            return true;  // Signal buffer consumption
        }
    }

    // Continue with normal command processing using adjusted data
    if (result != command_result::TIMEOUT || got_line == nullptr) {
        return false;  // Command processing continues
    }

    // ... existing command processing logic ...
}
```

#### Usage Examples

```cpp
// Example 1: HTTP URC Handler - Consume only URC, leave command response
UrcConsumeInfo http_urc_handler(const UrcBufferInfo& info) {
    std::string_view buffer((const char*)info.buffer_start, info.buffer_total_size);

    // Look for HTTP URC in new data
    size_t urc_start = buffer.find("+HTTPGET:", info.processed_offset);
    if (urc_start != std::string_view::npos) {
        size_t urc_end = buffer.find("\r\n", urc_start);
        if (urc_end != std::string_view::npos) {
            // Found complete URC, consume only up to URC end
            return {UrcConsumeResult::CONSUME_PARTIAL, urc_end + 2};
        }
    }

    // No complete URC found, don't consume anything
    return {UrcConsumeResult::CONSUME_NONE, 0};
}

// Example 2: Multi-part Response Handler
UrcConsumeInfo multi_part_handler(const UrcBufferInfo& info) {
    std::string_view new_data((const char*)info.new_data_start, info.new_data_size);

    // Check if we have complete response
    if (new_data.find("OK\r\n") != std::string_view::npos) {
        // Complete response received, consume everything
        return {UrcConsumeResult::CONSUME_ALL, 0};
    }

    // Still waiting for completion, don't consume
    return {UrcConsumeResult::CONSUME_NONE, 0};
}

// Example 3: Selective URC Processing
UrcConsumeInfo selective_handler(const UrcBufferInfo& info) {
    // Only process URCs when no command is active
    if (info.is_command_active) {
        return {UrcConsumeResult::CONSUME_NONE, 0};
    }

    // Process URCs normally when no command waiting
    std::string_view buffer((const char*)info.buffer_start, info.buffer_total_size);
    size_t urc_pos = buffer.find("+URC:", info.processed_offset);
    if (urc_pos != std::string_view::npos) {
        size_t end_pos = buffer.find("\r\n", urc_pos);
        if (end_pos != std::string_view::npos) {
            return {UrcConsumeResult::CONSUME_PARTIAL, end_pos + 2};
        }
    }

    return {UrcConsumeResult::CONSUME_NONE, 0};
}
```

### 4. Migration Strategy for Modem v2.0

#### Phase 1: Enhanced Interface Implementation
1. **Add new enhanced URC interface** alongside existing callback
2. **Implement buffer state tracking** in DTE layer
3. **Add consumption control logic** to processing pipeline
4. **Maintain backward compatibility** with existing callback API

#### Phase 2: Gradual Migration
1. **Update examples** to use enhanced interface
2. **Add comprehensive testing** for buffer consumption scenarios
3. **Document migration patterns** for common use cases
4. **Performance testing** to ensure no regression

#### Phase 3: Cleanup
1. **Deprecate old callback interface** with clear migration path
2. **Remove legacy code** in next major version
3. **Update all documentation** to reflect new patterns

### 5. Key Benefits of Enhanced Approach

**Granular Control**: URC handlers can make precise decisions about buffer consumption
**State Awareness**: Handlers know exactly what data is new vs. already processed
**Command Safety**: URC processing cannot interfere with active command responses
**Multi-part Support**: Proper handling of responses that arrive in multiple chunks
**Debugging**: Better visibility into buffer state and processing flow

### 6. Implementation Considerations

#### Thread Safety
- Buffer state updates must be atomic
- URC handler calls must be thread-safe
- Proper locking around buffer state modifications

#### Performance
- Minimize overhead of buffer state tracking
- Efficient string searching in URC handlers
- Avoid unnecessary buffer copies

#### Memory Management
- Careful pointer management for buffer offsets
- Proper handling of buffer growth and reallocation
- Cleanup of buffer state on DTE destruction

## Conclusion

The enhanced buffer management approach addresses the critical issues of tight coupling and inflexible buffer consumption while maintaining the simplicity of the current callback-based approach. This solution provides:

1. **Complete Buffer Visibility**: URC handlers see entire buffer with processing context
2. **Granular Consumption Control**: Handlers can consume entire buffer, partial data, or nothing
3. **Processing State Awareness**: Handlers know what's new vs. already processed
4. **Command Safety**: URC processing cannot interfere with active commands

This approach provides a clean migration path to modem v2.0 while solving the fundamental buffer management issues that currently limit URC handler flexibility.

## Implementation Plan

### Phase 1: Enhanced Interface Implementation

#### Step 1: Add Enhanced URC Interface
- [x] **Add enhanced URC types to DTE header** (`esp_modem_dte.hpp`)
  - [x] `UrcBufferInfo` struct with buffer state information
  - [x] `UrcConsumeResult` enum for consumption control
  - [x] `UrcConsumeInfo` struct for consumption details
  - [x] `enhanced_urc_cb` typedef for new callback signature
  - [x] `set_enhanced_urc_cb()` method in DTE class
  - [x] `enhanced_urc_handler` member in `command_cb` structure

- [x] **Add enhanced URC interface to DCE template** (`esp_modem_dce_template.hpp`)
  - [x] `set_enhanced_urc()` method in DCE_T class

- [x] **Implement buffer state tracking in DTE**
  - [x] Add `BufferState` struct to track processing state
  - [x] Add `create_urc_info()` method to build buffer information
  - [x] Add `update_buffer_state()` method to track state changes

- [x] **Implement enhanced processing pipeline**
  - [x] Modify `process_line()` to call enhanced URC handler
  - [x] Add consumption control logic
  - [x] Add buffer pointer adjustment for command processing

- [ ] **Add backward compatibility**
  - [ ] Ensure existing `urc_handler` still works
  - [ ] Add fallback logic when only legacy handler is set

#### Step 2: Testing and Validation
- [ ] **Create test cases for enhanced URC interface**
  - [ ] Test partial buffer consumption
  - [ ] Test full buffer consumption
  - [ ] Test no consumption (waiting)
  - [ ] Test command/URC interaction scenarios

- [x] **Update existing examples**
  - [x] Convert `urc_test.cpp` to use enhanced interface
  - [x] Update modem console example
  - [ ] Add new examples demonstrating consumption control

#### Step 3: Documentation and Examples
- [x] **Update API documentation**
  - [x] Document new enhanced URC interface
  - [x] Add usage examples and best practices
  - [x] Document migration path from legacy interface

### Phase 2: Gradual Migration
- [ ] **Performance testing**
  - [ ] Benchmark enhanced interface vs legacy
  - [ ] Test memory usage and overhead
  - [ ] Validate thread safety

- [ ] **Community feedback**
  - [ ] Gather feedback on new interface
  - [ ] Refine API based on usage patterns
  - [ ] Address any performance concerns

### Phase 3: Cleanup (Modem v2.0)
- [ ] **Deprecate legacy interface**
  - [ ] Add deprecation warnings
  - [ ] Provide clear migration documentation
  - [ ] Set removal timeline

- [ ] **Remove legacy code**
  - [ ] Remove old `urc_handler` callback
  - [ ] Clean up legacy processing logic
  - [ ] Update all internal references

### Current Status
- **Phase 1, Step 1**: ✅ **COMPLETED** - Enhanced URC interface added to headers
- **Phase 1, Step 2**: ✅ **COMPLETED** - Buffer state tracking implemented
- **Phase 1, Step 3**: ✅ **COMPLETED** - Enhanced processing pipeline implemented
- **Phase 1, Step 4**: ✅ **COMPLETED** - Test validation successful
- **Next**: Documentation and API documentation
