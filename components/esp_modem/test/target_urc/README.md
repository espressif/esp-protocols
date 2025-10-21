# ESP Modem Enhanced URC Test

## Overview

This test validates the enhanced URC (Unsolicited Result Code) interface with buffer consumption control. It demonstrates the new `set_enhanced_urc()` API that provides granular control over buffer consumption and complete buffer visibility.

## Test Configuration

- **Target**: ESP-AT device with HTTP server
- **UART**: 115200 baud, 8N1, TX=17, RX=18
- **Buffer Size**: 1024 bytes
- **Timeout**: 15 seconds

## Test Cases

### 1. Enhanced URC Handler Registration
- **Objective**: Verify enhanced URC handler can be registered
- **Method**: Call `set_enhanced_urc()` with custom handler
- **Expected**: Handler receives `UrcBufferInfo` with complete buffer context

### 2. Buffer Visibility
- **Objective**: Verify URC handler receives complete buffer information
- **Method**: Log buffer state information in handler
- **Expected**: Handler receives `buffer_start`, `buffer_total_size`, `processed_offset`, `new_data_size`, `is_command_active`

### 3. Granular Consumption Control
- **Objective**: Verify handler can consume partial buffer data
- **Method**: Process HTTP URCs line-by-line using `CONSUME_PARTIAL`
- **Expected**: Each complete line is consumed individually, remaining data preserved

### 4. Multi-part Response Handling
- **Objective**: Verify handling of chunked HTTP responses
- **Method**: Process 9 HTTP chunks from ESP-AT server
- **Expected**: All chunks processed correctly with proper offset tracking

### 5. Transfer Completion Detection
- **Objective**: Verify detection of transfer completion message
- **Method**: Search for "Transfer completed" in buffer
- **Expected**: Completion detected and event group signaled

### 6. Command State Awareness
- **Objective**: Verify handler knows command state
- **Method**: Check `is_command_active` flag during processing
- **Expected**: Flag correctly reflects command state

## Test Implementation

The test uses an ESP-AT device running an HTTP server that sends chunked responses. The enhanced URC handler processes each HTTP URC line individually and detects completion.

### Key Components

```cpp
// Enhanced URC handler registration
set_enhanced_urc(handle_enhanced_urc);

// Handler implementation
static esp_modem::DTE::UrcConsumeInfo handle_enhanced_urc(const esp_modem::DTE::UrcBufferInfo& info)
{
    // Process HTTP URCs with granular consumption control
    if (line.starts_with("+HTTPCGET:")) {
        // Consume this line only
        return {esp_modem::DTE::UrcConsumeResult::CONSUME_PARTIAL, line_end + 1};
    }

    // Check for completion
    if (buffer.find("Transfer completed") != std::string_view::npos) {
        xEventGroupSetBits(s_event_group, transfer_completed);
        return {esp_modem::DTE::UrcConsumeResult::CONSUME_ALL, 0};
    }

    return {esp_modem::DTE::UrcConsumeResult::CONSUME_NONE, 0};
}
```

## Example Output

### Successful Test Run
```
I (908) urc_test: Starting Enhanced URC Test
I (938) urc_test: Start HTTP server...(0)
I (948) urc_test: HTTP GET...(43)
I (1228) urc_test: HTTP URC: +HTTPCGET:27,=== Async Response #4 ===
I (2778) urc_test: HTTP URC: +HTTPCGET:61,[1/9] [633135 ms] This is a simulated slow server response.
I (4288) urc_test: HTTP URC: +HTTPCGET:71,[2/9] [634639 ms] Chunk 1: The ESP-AT HTTP server is demonstrating...
I (5788) urc_test: HTTP URC: +HTTPCGET:73,[3/9] [636143 ms] Chunk 2: ...asynchronous chunked transfer encoding...
I (7288) urc_test: HTTP URC: +HTTPCGET:72,[4/9] [637647 ms] Chunk 3: ...with artificial delays between chunks...
I (8788) urc_test: HTTP URC: +HTTPCGET:74,[5/9] [639151 ms] Chunk 4: ...to simulate real-world network conditions.
I (10288) urc_test: HTTP URC: +HTTPCGET:62,[6/9] [640655 ms] Chunk 5: Processing data... please wait...
I (11788) urc_test: HTTP URC: +HTTPCGET:63,[7/9] [642159 ms] Chunk 6: Still processing... almost done...
I (13288) urc_test: HTTP URC: +HTTPCGET:61,[8/9] [643663 ms] Chunk 7: Final chunk - transfer complete!
I (14758) urc_test: HTTP URC: +HTTPCGET:43,[9/9] [645168 ms] === END OF RESPONSE ===
I (15258) urc_test: Transfer completed detected in buffer!
I (15298) urc_test: Enhanced URC test completed successfully!
I (15308) urc_test: The enhanced URC handler successfully processed all HTTP chunks
I (15308) urc_test: with granular buffer consumption control
```

### Debug Output (with ESP_LOG_LEVEL_DEBUG)
```
D (958) urc_test: URC Buffer Info: total_size=43, processed_offset=0, new_data_size=43, command_active=false
D (958) urc_test: Buffer content (first 43 chars): AT+HTTPCGET="http://127.0.0.1:8080/async"
D (968) urc_test: Other data: AT+HTTPCGET="http://127.0.0.1:8080/async"
D (1218) urc_test: URC Buffer Info: total_size=85, processed_offset=43, new_data_size=42, command_active=false
D (1228) urc_test: Consuming 40 bytes (line_end=82, processed_offset=43)
D (2778) urc_test: Consuming 76 bytes (line_end=158, processed_offset=83)
```

### Failed Test (Timeout)
```
I (908) urc_test: Starting Enhanced URC Test
I (948) urc_test: HTTP GET...(43)
E (15385) urc_test: Enhanced URC test timed out
I (15385) urc_test: Enhanced URC test done
```

## Test Validation

### Success Criteria
- All 9 HTTP chunks processed successfully
- Transfer completion detected within 15 seconds
- No buffer arithmetic errors (no negative `new_data_size`)
- Proper consumption control (line-by-line processing)

### Failure Indicators
- Test timeout (15 seconds)
- Buffer arithmetic errors (negative `new_data_size`)
- Missing HTTP chunks
- Transfer completion not detected

## Dependencies

- ESP-AT device with HTTP server capability
- UART connection configured
- `CONFIG_ESP_MODEM_URC_HANDLER=y`
- FreeRTOS event groups

## Build and Run

```bash
idf.py build
idf.py flash monitor
```

## Comparison with Legacy URC

| Feature | Legacy URC | Enhanced URC |
|---------|------------|--------------|
| Buffer Consumption | All or nothing | Granular (partial) |
| Buffer Visibility | None | Complete context |
| Command State | Unknown | Known (`is_command_active`) |
| Processing State | Unknown | Tracked (`processed_offset`) |
| Multi-part Support | Limited | Full support |
