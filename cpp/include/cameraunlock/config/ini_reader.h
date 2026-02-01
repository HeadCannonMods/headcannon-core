#pragma once

#include <string>
#include <cstdint>
#include <functional>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace cameraunlock {

/// Generic INI file reader with type-safe reading methods.
/// Supports hot-reload detection via file modification time.
class IniReader {
public:
    /// Error callback type for logging.
    using ErrorCallback = std::function<void(const char*)>;

    IniReader() = default;
    ~IniReader() = default;

    // Non-copyable but movable
    IniReader(const IniReader&) = delete;
    IniReader& operator=(const IniReader&) = delete;
    IniReader(IniReader&&) = default;
    IniReader& operator=(IniReader&&) = default;

    /// Opens an INI file for reading.
    /// @param path Path to the INI file.
    /// @return True if the file exists and was opened successfully.
    bool Open(const std::string& path);

    /// Closes the current file.
    void Close();

    /// Gets the current file path.
    const std::string& GetPath() const { return m_path; }

    /// Returns true if a file is currently open.
    bool IsOpen() const { return !m_path.empty(); }

    /// Checks if the file has been modified since last Open() or RefreshModTime().
    bool HasChanged() const;

    /// Updates the stored modification time to the current file time.
    void RefreshModTime();

    /// Sets an error callback for logging.
    void SetErrorCallback(ErrorCallback callback) { m_errorCallback = std::move(callback); }

    // ========== Reading methods ==========

    /// Reads a string value.
    /// @param section INI section name.
    /// @param key Key name within the section.
    /// @param defaultValue Value to return if key is not found.
    /// @return The value, or defaultValue if not found.
    std::string ReadString(const char* section, const char* key, const char* defaultValue = "") const;

    /// Reads an integer value.
    int ReadInt(const char* section, const char* key, int defaultValue = 0) const;

    /// Reads an unsigned integer value.
    unsigned int ReadUInt(const char* section, const char* key, unsigned int defaultValue = 0) const;

    /// Reads a 64-bit integer value.
    int64_t ReadInt64(const char* section, const char* key, int64_t defaultValue = 0) const;

    /// Reads a double-precision floating point value.
    double ReadDouble(const char* section, const char* key, double defaultValue = 0.0) const;

    /// Reads a single-precision floating point value.
    float ReadFloat(const char* section, const char* key, float defaultValue = 0.0f) const;

    /// Reads a boolean value (0/1, true/false, yes/no).
    bool ReadBool(const char* section, const char* key, bool defaultValue = false) const;

    /// Reads a hexadecimal value (e.g., "0x77" or "77").
    int ReadHex(const char* section, const char* key, int defaultValue = 0) const;

    // ========== Validation helpers ==========

    /// Reads an int and validates it's within a range.
    /// @return True if the value was read and is valid.
    bool ReadIntInRange(const char* section, const char* key, int& outValue,
                        int minValue, int maxValue, int defaultValue = 0) const;

    /// Reads a double and validates it's within a range.
    bool ReadDoubleInRange(const char* section, const char* key, double& outValue,
                           double minValue, double maxValue, double defaultValue = 0.0) const;

    /// Reads a float and validates it's within a range.
    bool ReadFloatInRange(const char* section, const char* key, float& outValue,
                          float minValue, float maxValue, float defaultValue = 0.0f) const;

private:
    void LogError(const char* message) const;

    std::string m_path;
    ErrorCallback m_errorCallback;

#ifdef _WIN32
    FILETIME m_lastModTime = {};
#else
    time_t m_lastModTime = 0;
#endif
};

/// Helper class for creating default INI files.
class IniWriter {
public:
    IniWriter() = default;
    ~IniWriter();

    /// Opens a file for writing. Creates parent directories if needed.
    bool Open(const std::string& path);

    /// Closes the file.
    void Close();

    /// Writes a comment line (prefixed with ;).
    void WriteComment(const char* comment);

    /// Writes a blank line.
    void WriteBlankLine();

    /// Writes a section header [SectionName].
    void WriteSection(const char* section);

    /// Writes a key=value pair.
    void WriteString(const char* key, const char* value);
    void WriteInt(const char* key, int value);
    void WriteDouble(const char* key, double value);
    void WriteBool(const char* key, bool value);
    void WriteHex(const char* key, int value);

private:
    void* m_file = nullptr;  // FILE* stored as void* for header-only
};

}  // namespace cameraunlock
