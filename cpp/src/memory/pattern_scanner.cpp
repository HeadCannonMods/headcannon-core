#include <cameraunlock/memory/pattern_scanner.h>

#include <vector>
#include <cctype>
#include <cstring>

#ifdef _WIN32
#include <Psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace cameraunlock::memory {

namespace {

// Parse hex string pattern into bytes and mask
// Pattern format: "48 8B 05 ?? ?? ?? ??" where ?? is wildcard
// Using char for mask because std::vector<bool> is a special case that doesn't have .data()
bool ParsePattern(std::string_view pattern, std::vector<uint8_t>& bytes, std::vector<char>& mask) {
    bytes.clear();
    mask.clear();

    size_t i = 0;
    while (i < pattern.size()) {
        // Skip whitespace
        while (i < pattern.size() && std::isspace(static_cast<unsigned char>(pattern[i]))) {
            ++i;
        }
        if (i >= pattern.size()) break;

        // Check for wildcard
        if (pattern[i] == '?') {
            bytes.push_back(0);
            mask.push_back('?');  // wildcard
            ++i;
            // Skip second ? if present (for ??)
            if (i < pattern.size() && pattern[i] == '?') {
                ++i;
            }
        } else {
            // Parse hex byte
            if (i + 1 >= pattern.size()) return false;

            char hex[3] = { pattern[i], pattern[i + 1], 0 };
            char* end = nullptr;
            long value = std::strtol(hex, &end, 16);
            if (end != hex + 2) return false;

            bytes.push_back(static_cast<uint8_t>(value));
            mask.push_back('x');  // match
            i += 2;
        }
    }

    return !bytes.empty();
}

// Match pattern at a specific address
bool MatchPattern(const uint8_t* data, const uint8_t* pattern, const char* mask, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (mask[i] == 'x' && data[i] != pattern[i]) {
            return false;
        }
    }
    return true;
}

} // anonymous namespace

bool GetModuleRange(void* module, uintptr_t& base, size_t& size) {
    if (!module) return false;

#ifdef _WIN32
    MODULEINFO modInfo = {};
    if (!GetModuleInformation(GetCurrentProcess(), static_cast<HMODULE>(module), &modInfo, sizeof(modInfo))) {
        return false;
    }
    base = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
    size = modInfo.SizeOfImage;
    return true;
#else
    // Non-Windows platforms would need different implementation
    (void)base;
    (void)size;
    return false;
#endif
}

void* ScanPatternInRange(uintptr_t base, size_t size, std::string_view pattern) {
    std::vector<uint8_t> patternBytes;
    std::vector<char> patternMask;

    if (!ParsePattern(pattern, patternBytes, patternMask)) {
        return nullptr;
    }

    if (patternBytes.size() > size) {
        return nullptr;
    }

    const uint8_t* start = reinterpret_cast<const uint8_t*>(base);
    const size_t searchSize = size - patternBytes.size();

    for (size_t i = 0; i <= searchSize; ++i) {
        if (MatchPattern(start + i, patternBytes.data(), patternMask.data(), patternBytes.size())) {
            return const_cast<uint8_t*>(start + i);
        }
    }

    return nullptr;
}

void* ScanPatternMaskInRange(uintptr_t base, size_t size, const uint8_t* pattern, const char* mask, size_t length) {
    if (length > size) {
        return nullptr;
    }

    const uint8_t* start = reinterpret_cast<const uint8_t*>(base);
    const size_t searchSize = size - length;

    for (size_t i = 0; i <= searchSize; ++i) {
        if (MatchPattern(start + i, pattern, mask, length)) {
            return const_cast<uint8_t*>(start + i);
        }
    }

    return nullptr;
}

void* ScanPattern(void* module, std::string_view pattern) {
    uintptr_t base = 0;
    size_t size = 0;

    if (!GetModuleRange(module, base, size)) {
        return nullptr;
    }

    return ScanPatternInRange(base, size, pattern);
}

void* ScanPatternMask(void* module, const uint8_t* pattern, const char* mask, size_t length) {
    uintptr_t base = 0;
    size_t size = 0;

    if (!GetModuleRange(module, base, size)) {
        return nullptr;
    }

    return ScanPatternMaskInRange(base, size, pattern, mask, length);
}

void* ResolveRIPRelative(void* instruction, int offset_position, int instruction_length) {
    if (!instruction) return nullptr;

    auto* inst = static_cast<uint8_t*>(instruction);
    int32_t displacement = 0;
    std::memcpy(&displacement, inst + offset_position, sizeof(int32_t));

    // RIP-relative addressing: target = instruction_end + displacement
    return inst + instruction_length + displacement;
}

void* FindRTTIDescriptor(void* module, std::string_view class_name) {
    uintptr_t base = 0;
    size_t size = 0;

    if (!GetModuleRange(module, base, size)) {
        return nullptr;
    }

    // Search for the class name string in the module
    // RTTI type descriptor starts with vtable pointer followed by spare data, then name
    const uint8_t* start = reinterpret_cast<const uint8_t*>(base);

    for (size_t i = 0; i <= size - class_name.size(); ++i) {
        if (std::memcmp(start + i, class_name.data(), class_name.size()) == 0) {
            // Found the name string - this is inside the type_info structure
            // The structure layout is:
            // - vtable pointer (8 bytes on x64)
            // - spare data pointer (8 bytes on x64)
            // - name string (variable length, null terminated)
            // We found the name, so go back to find the start of type_info
            const size_t type_info_offset = sizeof(void*) * 2;  // 16 bytes on x64
            if (i >= type_info_offset) {
                return const_cast<uint8_t*>(start + i - type_info_offset);
            }
        }
    }

    return nullptr;
}

} // namespace cameraunlock::memory
