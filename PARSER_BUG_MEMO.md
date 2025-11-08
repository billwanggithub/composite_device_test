# Command Parser Bug Memo - indexOf Anti-Pattern

**Date:** 2025-11-09
**Severity:** Critical
**Status:** Fixed in v2.5.0
**Commits:** `06a45ba`, `6f969ed`

---

## 🐛 Bug Description

**The indexOf Anti-Pattern:**

Using `indexOf(char, fromIndex)` where `fromIndex` points to the position AFTER a known prefix causes the search to skip the intended delimiter and find the NEXT occurrence, resulting in incorrect parameter extraction.

---

## 💥 Real-World Impact

### Affected Commands (All Fixed)
1. UART1 PWM
2. BUZZER
3. LED_PWM
4. UART1 CONFIG
5. UART1 WRITE
6. UART2 CONFIG
7. UART2 WRITE

### Example Failure
```cpp
// BUGGY CODE:
void handleUART1PWM(const String& cmd, ICommandResponse* response) {
    int idx1 = cmd.indexOf(' ', 10);  // ❌ BUG!
    // ...
}

// Command: "UART1 PWM 1000 50 ON"
// Position:  0123456789012345678901
//                   ^    ^  ^
//                   10   14 17
//
// indexOf(' ', 10) searches FROM position 10
// Finds space at position 14 (after "1000")
// Skips the space at position 9 (after "PWM")
//
// Result: Extracts "50" as frequency instead of "1000" ❌
```

---

## ✅ The Fix

### Pattern 1: Fixed Position (Best for Known Prefixes)

```cpp
// CORRECT CODE:
void handleUART1PWM(const String& cmd, ICommandResponse* response) {
    // "UART1 PWM " is exactly 10 characters (positions 0-9)
    const int idx1 = 9;  // ✅ Fixed position of space after "PWM"

    int idx2 = cmd.indexOf(' ', idx1 + 1);  // Search from position 10
    uint32_t freq = cmd.substring(idx1 + 1, idx2).toInt();
    // ...
}
```

### Pattern 2: Substring First (Best for Flexibility)

```cpp
// CORRECT CODE:
void handleCommand(const String& cmd, ICommandResponse* response) {
    // Extract parameters substring first
    const int PREFIX_LENGTH = 13;  // Length of "UART1 CONFIG "
    String params = cmd.substring(PREFIX_LENGTH);
    params.trim();

    // Now search within params (position 0 is start of params)
    int idx = params.indexOf(' ');  // ✅ Search from beginning of params
    if (idx != -1) {
        String param1 = params.substring(0, idx);
        String param2 = params.substring(idx + 1);
    }
}
```

---

## 🚨 Why This Bug is Dangerous

### 1. Silent Failure
- No compiler errors or warnings
- Code looks correct at first glance
- Only fails with actual input data

### 2. Data-Dependent
- Different commands have different lengths
- Easy to test with one command, break another
- Hard to catch without comprehensive testing

### 3. Subtle Logic Error
```cpp
// Both look similar, but behave very differently:
int idx = cmd.indexOf(' ', 10);   // ❌ Searches FROM 10
int idx = cmd.indexOf(' ');        // ✅ Searches from beginning
```

---

## 📋 Code Review Checklist

When reviewing command parsers, check for:

- [ ] ❌ `cmd.indexOf(' ', N)` where N > 0
- [ ] ❌ Searching in full command string instead of param substring
- [ ] ❌ Hardcoded position numbers without comments
- [ ] ✅ Substring extraction first, then parse
- [ ] ✅ Fixed position with clear comments explaining why
- [ ] ✅ Unit tests with actual command strings

---

## 🎯 Best Practices Going Forward

### DO ✅

```cpp
// 1. Use fixed positions with documentation
const int PREFIX_LEN = 10;  // "UART1 PWM " = 10 chars
int spacePos = 9;           // Position of space after "PWM"

// 2. Extract substring first
String params = cmd.substring(PREFIX_LEN);
int idx = params.indexOf(' ');  // Search in params, not cmd

// 3. Add validation
if (params.length() == 0) {
    response->println("ERROR: No parameters");
    return;
}

// 4. Document the command format
// Command format: "UART1 PWM <freq> <duty> [ON|OFF]"
//                  ^        ^
//                  0        10 (start of params)
```

### DON'T ❌

```cpp
// 1. Don't search from arbitrary position
int idx = cmd.indexOf(' ', 10);  // ❌ What is 10? Why 10?

// 2. Don't use magic numbers
int idx = cmd.indexOf(' ', 10);  // ❌ No explanation

// 3. Don't skip validation
String param = cmd.substring(idx + 1);  // ❌ What if idx == -1?

// 4. Don't assume input format
uint32_t value = cmd.substring(10).toInt();  // ❌ No error checking
```

---

## 🧪 Testing Requirements

### For Every New Command Parser

```cpp
// Minimum test cases:
1. Valid command with all parameters
2. Valid command with minimum parameters
3. Invalid: missing parameters
4. Invalid: extra parameters
5. Edge case: parameter at boundary values
6. Text parameter with spaces (if applicable)
```

### Example Test
```cpp
void test_UART1_PWM_Parser() {
    // Valid full format
    assert(parse("UART1 PWM 1000 50 ON") == {freq:1000, duty:50.0, enabled:true});

    // Valid minimal format
    assert(parse("UART1 PWM 1000 50") == {freq:1000, duty:50.0, enabled:true});

    // Invalid formats
    assert(parse("UART1 PWM 1000") == ERROR);
    assert(parse("UART1 PWM") == ERROR);

    // Edge cases
    assert(parse("UART1 PWM 1 0 OFF") == {freq:1, duty:0.0, enabled:false});
    assert(parse("UART1 PWM 500000 100 ON") == {freq:500000, duty:100.0, enabled:true});
}
```

---

## 📚 Related Documentation

- Technical details: [CLAUDE.md § Command Parser indexOf Bug](CLAUDE.md#command-parser-indexof-bug-fixed-in-v250)
- User notice: [README.md § 重要修正](README.md#-重要修正v250)
- Fixed commits:
  - `06a45ba` - UART1 PWM fix
  - `6f969ed` - 6 additional commands fixed

---

## 💡 Key Takeaway

**"Never use `indexOf(char, N)` where N > 0 on the full command string. Always extract the parameter substring first, then search from position 0."**

This simple rule prevents an entire class of parsing bugs.

---

## ⚡ Quick Reference

```cpp
// ❌ WRONG - Anti-pattern that caused the bug
int idx = cmd.indexOf(' ', PREFIX_LENGTH);

// ✅ CORRECT - Option 1: Fixed position
const int SPACE_POS = PREFIX_LENGTH - 1;
int idx2 = cmd.indexOf(' ', SPACE_POS + 1);

// ✅ CORRECT - Option 2: Substring first (RECOMMENDED)
String params = cmd.substring(PREFIX_LENGTH);
int idx = params.indexOf(' ');
```

---

**REMEMBER:** This bug affected 7 production commands and was only caught through careful debugging with user input. Don't let it happen again!
