# ESP Modem Enhanced URC Documentation Plan

## Current Documentation Analysis

### Existing Documentation Structure
- **Location**: `/docs/esp_modem/`
- **Format**: Sphinx/RST with Doxygen integration
- **Structure**:
  - `index.rst` - Main documentation index
  - `cxx_api_docs.rst` - C++ API documentation
  - `advanced_api.rst` - Advanced use cases
  - `internal_docs.rst` - Internal implementation details

### Current URC Documentation Status
- **URC mentioned**: Only in `advanced_api.rst` line 39: "implement URC processing in user space"
- **No dedicated URC section**: URC functionality is not documented
- **Missing**: Enhanced URC interface documentation
- **Missing**: URC usage examples and best practices

## Documentation Plan

### Phase 1: Add URC Documentation to Existing Structure

#### 1.1 Update C++ API Documentation (`cxx_api_docs.rst`)
**Location**: `/docs/esp_modem/en/cxx_api_docs.rst`

**Add new section**:
```rst
.. _cpp_urc_handling:

URC (Unsolicited Result Code) Handling
---------------------------------------

The ESP modem library provides two interfaces for handling URCs:

- **Legacy URC Interface**: Simple callback-based approach
- **Enhanced URC Interface**: Advanced interface with buffer consumption control

.. _cpp_legacy_urc:

Legacy URC Interface
^^^^^^^^^^^^^^^^^^^^

The legacy interface provides basic URC handling with a simple callback:

.. code-block:: cpp

    // Register legacy URC handler
    dce->set_urc([](uint8_t *data, size_t len) -> esp_modem::command_result {
        // Process URC data
        return esp_modem::command_result::TIMEOUT;
    });

.. _cpp_enhanced_urc:

Enhanced URC Interface
^^^^^^^^^^^^^^^^^^^^^^

The enhanced interface provides granular buffer consumption control and complete buffer visibility:

.. code-block:: cpp

    // Register enhanced URC handler
    dce->set_enhanced_urc([](const esp_modem::DTE::UrcBufferInfo& info) -> esp_modem::DTE::UrcConsumeInfo {
        // Process URC with complete buffer context
        if (info.buffer_total_size > 0) {
            // Consume partial data
            return {esp_modem::DTE::UrcConsumeResult::CONSUME_PARTIAL, 100};
        }
        return {esp_modem::DTE::UrcConsumeResult::CONSUME_NONE, 0};
    });

**Key Features**:
- Complete buffer visibility with processing context
- Granular consumption control (none, partial, all)
- Processing state awareness (what's new vs. already processed)
- Command state awareness (whether a command is active)

.. doxygenstruct:: esp_modem::DTE::UrcBufferInfo
   :members:

.. doxygenenum:: esp_modem::DTE::UrcConsumeResult

.. doxygenstruct:: esp_modem::DTE::UrcConsumeInfo
   :members:

.. doxygenfunction:: esp_modem::DCE_T::set_enhanced_urc
```

#### 1.2 Update Advanced API Documentation (`advanced_api.rst`)
**Location**: `/docs/esp_modem/en/advanced_api.rst`

**Add new section**:
```rst
.. _urc_advanced:

Advanced URC Handling
---------------------

This section covers advanced URC handling scenarios and best practices.

.. _urc_migration:

Migration from Legacy to Enhanced URC
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The enhanced URC interface provides backward compatibility with the legacy interface.
Applications can migrate gradually:

1. **Immediate Migration**: Replace `set_urc()` with `set_enhanced_urc()`
2. **Gradual Migration**: Use both interfaces during transition
3. **Full Migration**: Remove legacy interface usage

.. _urc_best_practices:

URC Handling Best Practices
^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Buffer Consumption Strategies**:

- **CONSUME_NONE**: Use when waiting for complete data
- **CONSUME_PARTIAL**: Use for line-by-line processing
- **CONSUME_ALL**: Use when processing is complete

**Multi-part Response Handling**:

.. code-block:: cpp

    esp_modem::DTE::UrcConsumeInfo handle_multi_part(const esp_modem::DTE::UrcBufferInfo& info) {
        std::string_view buffer((const char*)info.buffer_start, info.buffer_total_size);

        // Look for complete response
        if (buffer.find("OK\r\n") != std::string_view::npos) {
            return {esp_modem::DTE::UrcConsumeResult::CONSUME_ALL, 0};
        }

        // Still waiting for completion
        return {esp_modem::DTE::UrcConsumeResult::CONSUME_NONE, 0};
    }

**Command State Awareness**:

.. code-block:: cpp

    esp_modem::DTE::UrcConsumeInfo handle_with_command_awareness(const esp_modem::DTE::UrcBufferInfo& info) {
        // Only process URCs when no command is active
        if (info.is_command_active) {
            return {esp_modem::DTE::UrcConsumeResult::CONSUME_NONE, 0};
        }

        // Process URCs normally
        return {esp_modem::DTE::UrcConsumeResult::CONSUME_ALL, 0};
    }
```

#### 1.3 Update Internal Documentation (`internal_docs.rst`)
**Location**: `/docs/esp_modem/en/internal_docs.rst`

**Add to DTE section**:
```rst
.. _dte_urc_impl:

URC Processing Implementation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The DTE layer implements both legacy and enhanced URC processing:

**Legacy URC Processing**:
- Simple callback-based approach
- All-or-nothing buffer consumption
- Limited buffer visibility

**Enhanced URC Processing**:
- Advanced callback with buffer context
- Granular consumption control
- Complete buffer state tracking
- Command state awareness

**Buffer State Tracking**:

.. code-block:: cpp

    struct BufferState {
        size_t total_processed = 0;         // Total bytes processed by URC handlers
        size_t last_urc_processed = 0;      // Last offset processed by URC
        bool command_waiting = false;       // Whether command is waiting for response
        size_t command_start_offset = 0;    // Where current command response started
    };

**Processing Pipeline**:

1. **Buffer Update**: New data arrives, buffer state updated
2. **URC Handler Call**: Enhanced URC handler called with complete buffer context
3. **Consumption Decision**: Handler returns consumption information
4. **Buffer Adjustment**: Buffer pointers adjusted based on consumption
5. **Command Processing**: Remaining data processed by command handler
```

### Phase 2: Create Dedicated URC Documentation

#### 2.1 Create URC Guide (`urc_guide.rst`)
**Location**: `/docs/esp_modem/en/urc_guide.rst`

**Content**:
- Complete URC handling guide
- Detailed examples
- Troubleshooting section
- Performance considerations

#### 2.2 Create URC Examples (`urc_examples.rst`)
**Location**: `/docs/esp_modem/en/urc_examples.rst`

**Content**:
- HTTP URC handling example
- SMS URC handling example
- Custom URC handling example
- Multi-part response handling example

### Phase 3: Update Documentation Index

#### 3.1 Update Main Index (`index.rst`)
**Location**: `/docs/esp_modem/en/index.rst`

**Add to toctree**:
```rst
.. toctree::

   Brief intro <README>
   C interface <api_docs>
   C++ interface <cxx_api_docs>
   URC Handling Guide <urc_guide>
   Advanced use cases <advanced_api>
   Internal design <internal_design>
   Internal implementation <internal_docs>
```

## Implementation Steps

### Step 1: Update C++ API Documentation
- [ ] Add URC section to `cxx_api_docs.rst`
- [ ] Document enhanced URC interface
- [ ] Add code examples
- [ ] Include Doxygen references

### Step 2: Update Advanced API Documentation
- [ ] Add advanced URC section to `advanced_api.rst`
- [ ] Document migration path
- [ ] Add best practices
- [ ] Include advanced examples

### Step 3: Update Internal Documentation
- [ ] Add URC implementation details to `internal_docs.rst`
- [ ] Document buffer state tracking
- [ ] Explain processing pipeline
- [ ] Include implementation details

### Step 4: Create Dedicated URC Documentation
- [ ] Create `urc_guide.rst`
- [ ] Create `urc_examples.rst`
- [ ] Add comprehensive examples
- [ ] Include troubleshooting guide

### Step 5: Update Documentation Index
- [ ] Update main index to include URC documentation
- [ ] Ensure proper navigation
- [ ] Test documentation build

## Documentation Standards

### Code Examples
- Use complete, compilable examples
- Include error handling
- Add comments explaining key concepts
- Use consistent formatting

### API Documentation
- Use Doxygen integration for API references
- Include parameter descriptions
- Add return value documentation
- Include usage notes

### Examples
- Provide real-world scenarios
- Include expected output
- Add troubleshooting tips
- Show common pitfalls

## Success Criteria

### Functional Requirements
- ✅ Complete URC interface documentation
- ✅ Migration guide from legacy to enhanced
- ✅ Best practices and examples
- ✅ Troubleshooting section

### Quality Requirements
- ✅ Clear, concise documentation
- ✅ Complete code examples
- ✅ Proper Doxygen integration
- ✅ Consistent formatting

### User Experience
- ✅ Easy to find URC documentation
- ✅ Clear navigation structure
- ✅ Practical examples
- ✅ Comprehensive coverage

## Next Steps

1. **Start with C++ API documentation** - Add URC section to existing structure
2. **Update advanced API** - Add migration and best practices
3. **Create dedicated guide** - Comprehensive URC handling guide
4. **Test documentation build** - Ensure all changes work correctly
5. **Review and refine** - Get feedback and improve documentation
