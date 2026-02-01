#include <headcannon/input/hotkey_poller.h>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace headcannon::input {

HotkeyPoller::~HotkeyPoller() {
    Stop();
}

HotkeyPoller::HotkeyPoller(HotkeyPoller&& other) noexcept {
    // Stop the other's thread first
    other.Stop();

    m_toggleKey.store(other.m_toggleKey.load());
    m_recenterKey.store(other.m_recenterKey.load());
    m_pollInterval.store(other.m_pollInterval.load());
    m_toggleCallback = std::move(other.m_toggleCallback);
    m_recenterCallback = std::move(other.m_recenterCallback);

    std::lock_guard<std::mutex> lock(other.m_hotkeyMutex);
    m_hotkeys = std::move(other.m_hotkeys);
    m_nextHotkeyId = other.m_nextHotkeyId;
}

HotkeyPoller& HotkeyPoller::operator=(HotkeyPoller&& other) noexcept {
    if (this != &other) {
        Stop();
        other.Stop();

        m_toggleKey.store(other.m_toggleKey.load());
        m_recenterKey.store(other.m_recenterKey.load());
        m_pollInterval.store(other.m_pollInterval.load());
        m_toggleCallback = std::move(other.m_toggleCallback);
        m_recenterCallback = std::move(other.m_recenterCallback);

        std::lock_guard<std::mutex> lock(other.m_hotkeyMutex);
        m_hotkeys = std::move(other.m_hotkeys);
        m_nextHotkeyId = other.m_nextHotkeyId;
    }
    return *this;
}

void HotkeyPoller::SetToggleKey(int vkCode, HotkeyCallback callback) {
    m_toggleKey.store(vkCode);
    m_toggleCallback = std::move(callback);
}

void HotkeyPoller::SetRecenterKey(int vkCode, HotkeyCallback callback) {
    m_recenterKey.store(vkCode);
    m_recenterCallback = std::move(callback);
}

int HotkeyPoller::AddHotkey(int vkCode, HotkeyCallback callback) {
    std::lock_guard<std::mutex> lock(m_hotkeyMutex);
    int id = m_nextHotkeyId++;
    m_hotkeys.push_back({id, vkCode, false, std::move(callback)});
    return id;
}

void HotkeyPoller::RemoveHotkey(int id) {
    std::lock_guard<std::mutex> lock(m_hotkeyMutex);
    auto it = std::find_if(m_hotkeys.begin(), m_hotkeys.end(),
        [id](const HotkeyEntry& entry) { return entry.id == id; });
    if (it != m_hotkeys.end()) {
        m_hotkeys.erase(it);
    }
}

bool HotkeyPoller::Start(int pollIntervalMs) {
    if (m_running.load()) {
        return true;
    }

    m_pollInterval.store(pollIntervalMs);
    m_stopFlag.store(false);
    m_running.store(true);

    // Reset key states
    m_toggleKeyDown.store(false);
    m_recenterKeyDown.store(false);
    {
        std::lock_guard<std::mutex> lock(m_hotkeyMutex);
        for (auto& entry : m_hotkeys) {
            entry.keyDown = false;
        }
    }

    m_thread = std::thread(&HotkeyPoller::PollLoop, this);
    return true;
}

void HotkeyPoller::Stop() {
    if (!m_running.load()) {
        return;
    }

    m_stopFlag.store(true);

    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_running.store(false);
}

void HotkeyPoller::SetToggleKeyCode(int vkCode) {
    m_toggleKey.store(vkCode);
}

void HotkeyPoller::SetRecenterKeyCode(int vkCode) {
    m_recenterKey.store(vkCode);
}

void HotkeyPoller::CheckKey(int vkCode, std::atomic<bool>& keyDown, const HotkeyCallback& callback) {
    if (vkCode == 0 || !callback) return;

#ifdef _WIN32
    bool pressed = (GetAsyncKeyState(vkCode) & 0x8000) != 0;
    if (pressed && !keyDown.load()) {
        keyDown.store(true);
        callback();
    } else if (!pressed && keyDown.load()) {
        keyDown.store(false);
    }
#endif
}

void HotkeyPoller::PollLoop() {
    while (!m_stopFlag.load()) {
        Poll();

        int interval = m_pollInterval.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }
}

void HotkeyPoller::Poll() {
    // Check toggle key
    CheckKey(m_toggleKey.load(), m_toggleKeyDown, m_toggleCallback);

    // Check recenter key
    CheckKey(m_recenterKey.load(), m_recenterKeyDown, m_recenterCallback);

    // Check generic hotkeys
    {
        std::lock_guard<std::mutex> lock(m_hotkeyMutex);
        for (auto& entry : m_hotkeys) {
            if (entry.vkCode == 0 || !entry.callback) continue;

#ifdef _WIN32
            bool pressed = (GetAsyncKeyState(entry.vkCode) & 0x8000) != 0;
            if (pressed && !entry.keyDown) {
                entry.keyDown = true;
                entry.callback();
            } else if (!pressed && entry.keyDown) {
                entry.keyDown = false;
            }
#endif
        }
    }
}

const char* VirtualKeyToString(int vkCode) {
    switch (vkCode) {
        case 0x70: return "F1";
        case 0x71: return "F2";
        case 0x72: return "F3";
        case 0x73: return "F4";
        case 0x74: return "F5";
        case 0x75: return "F6";
        case 0x76: return "F7";
        case 0x77: return "F8";
        case 0x78: return "F9";
        case 0x79: return "F10";
        case 0x7A: return "F11";
        case 0x7B: return "F12";
        case 0x1B: return "Escape";
        case 0x20: return "Space";
        case 0x24: return "Home";
        case 0x23: return "End";
        case 0x2D: return "Insert";
        case 0x2E: return "Delete";
        case 0x60: return "NumPad0";
        case 0x61: return "NumPad1";
        case 0x62: return "NumPad2";
        case 0x63: return "NumPad3";
        case 0x64: return "NumPad4";
        case 0x65: return "NumPad5";
        case 0x66: return "NumPad6";
        case 0x67: return "NumPad7";
        case 0x68: return "NumPad8";
        case 0x69: return "NumPad9";
        case 0x6A: return "NumPad*";
        case 0x6B: return "NumPad+";
        case 0x6D: return "NumPad-";
        case 0x6E: return "NumPad.";
        case 0x6F: return "NumPad/";
        case 0x90: return "NumLock";
        case 0x91: return "ScrollLock";
        case 0x13: return "Pause";
        case 0x2C: return "PrintScreen";
        default:
            if (vkCode >= 0x30 && vkCode <= 0x39) {
                static char numStr[2] = "0";
                numStr[0] = static_cast<char>(vkCode);
                return numStr;
            }
            if (vkCode >= 0x41 && vkCode <= 0x5A) {
                static char letterStr[2] = "A";
                letterStr[0] = static_cast<char>(vkCode);
                return letterStr;
            }
            return "Unknown";
    }
}

bool IsValidHotkeyCode(int vkCode) {
    // Function keys F1-F12
    if (vkCode >= 0x70 && vkCode <= 0x7B) return true;

    // NumPad keys
    if (vkCode >= 0x60 && vkCode <= 0x6F) return true;

    // Special keys
    if (vkCode == 0x13) return true;  // Pause
    if (vkCode == 0x2C) return true;  // PrintScreen
    if (vkCode == 0x90) return true;  // NumLock
    if (vkCode == 0x91) return true;  // ScrollLock
    if (vkCode == 0x24) return true;  // Home
    if (vkCode == 0x23) return true;  // End
    if (vkCode == 0x2D) return true;  // Insert
    if (vkCode == 0x2E) return true;  // Delete

    return false;
}

} // namespace headcannon::input
