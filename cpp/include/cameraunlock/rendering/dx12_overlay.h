#pragma once

// DX12 Overlay System
// Requires: Kiero, ImGui with DX12/Win32 backends, D3D12
//
// This is a header-only implementation template for DX12 game overlays.
// To use, include this header and provide the required dependencies.
//
// Required external dependencies:
// - kiero.h / kiero library
// - imgui.h, imgui_impl_dx12.h, imgui_impl_win32.h
// - d3d12.h, dxgi1_4.h
//
// Example usage:
//   DX12Overlay overlay;
//   overlay.SetRenderCallback([](float w, float h) {
//       ImDrawList* dl = ImGui::GetBackgroundDrawList();
//       dl->AddCircleFilled(ImVec2(w/2, h/2), 5, IM_COL32(255, 255, 255, 255));
//   });
//   overlay.Install();
//   // ...
//   overlay.Remove();

#include <functional>
#include <cstdint>

#ifdef CAMERAUNLOCK_DX12_OVERLAY_IMPLEMENTATION

#include <d3d12.h>
#include <dxgi1_4.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "kiero.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // CAMERAUNLOCK_DX12_OVERLAY_IMPLEMENTATION

namespace cameraunlock::rendering {

// Callback for custom rendering each frame
// Parameters: screenWidth, screenHeight
using RenderCallback = std::function<void(float, float)>;

// Callback for updating data each frame (before rendering)
using UpdateCallback = std::function<void()>;

// DX12 overlay configuration
struct DX12OverlayConfig {
    // Vtable indices for hooking
    int executeCommandListsIndex = 54;
    int presentIndex = 140;
    int present1Index = 154;
    int resizeBuffersIndex = 145;

    // Whether to hook Present1 (some games use Present, some use Present1)
    bool hookPresent1 = true;
};

#ifdef CAMERAUNLOCK_DX12_OVERLAY_IMPLEMENTATION

// DX12 hook function signatures
using ExecuteCommandLists_t = void(__stdcall*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
using Present_t = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using Present1_t = HRESULT(__stdcall*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using ResizeBuffers_t = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

// DX12 overlay implementation
class DX12Overlay {
public:
    DX12Overlay() = default;
    ~DX12Overlay() { Remove(); }

    // Non-copyable
    DX12Overlay(const DX12Overlay&) = delete;
    DX12Overlay& operator=(const DX12Overlay&) = delete;

    // Set callback for custom rendering
    void SetRenderCallback(RenderCallback callback) { m_renderCallback = std::move(callback); }

    // Set callback for updating data each frame
    void SetUpdateCallback(UpdateCallback callback) { m_updateCallback = std::move(callback); }

    // Install the DX12 hooks
    bool Install(const DX12OverlayConfig& config = {}) {
        if (m_hookInstalled) return true;

        if (kiero::init(kiero::RenderType::D3D12) != kiero::Status::Success) {
            return false;
        }

        m_config = config;

        // Store this pointer for static callbacks
        s_instance = this;

        bool hooked = false;

        if (kiero::bind(config.executeCommandListsIndex, (void**)&m_oExecuteCommandLists, HkExecuteCommandLists) == kiero::Status::Success) {
            hooked = true;
        }

        if (kiero::bind(config.presentIndex, (void**)&m_oPresent, HkPresent) == kiero::Status::Success) {
            hooked = true;
        }

        if (config.hookPresent1) {
            kiero::bind(config.present1Index, (void**)&m_oPresent1, HkPresent1);
        }

        kiero::bind(config.resizeBuffersIndex, (void**)&m_oResizeBuffers, HkResizeBuffers);

        if (!hooked) {
            kiero::shutdown();
            return false;
        }

        m_hookInstalled = true;
        return true;
    }

    // Remove the hooks and cleanup
    void Remove() {
        if (!m_hookInstalled) return;

        if (m_initialized) {
            ImGui_ImplDX12_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();

            if (m_oWndProc && m_hWindow) {
                SetWindowLongPtr(m_hWindow, GWLP_WNDPROC, (LONG_PTR)m_oWndProc);
            }

            CleanupResources();
            m_initialized = false;
        }

        kiero::unbind(m_config.executeCommandListsIndex);
        kiero::unbind(m_config.presentIndex);
        if (m_config.hookPresent1) {
            kiero::unbind(m_config.present1Index);
        }
        kiero::unbind(m_config.resizeBuffersIndex);
        kiero::shutdown();

        m_hookInstalled = false;
        s_instance = nullptr;
    }

    bool IsInstalled() const { return m_hookInstalled; }
    bool IsInitialized() const { return m_initialized; }

private:
    void CleanupResources() {
        if (m_pBackBuffers) {
            for (UINT i = 0; i < m_bufferCount; i++) {
                if (m_pBackBuffers[i]) m_pBackBuffers[i]->Release();
            }
            delete[] m_pBackBuffers;
            m_pBackBuffers = nullptr;
        }

        if (m_pCommandList) { m_pCommandList->Release(); m_pCommandList = nullptr; }
        if (m_pCommandAllocator) { m_pCommandAllocator->Release(); m_pCommandAllocator = nullptr; }
        if (m_pRtvDescHeap) { m_pRtvDescHeap->Release(); m_pRtvDescHeap = nullptr; }
        if (m_pSrvDescHeap) { m_pSrvDescHeap->Release(); m_pSrvDescHeap = nullptr; }
        if (m_pDevice) { m_pDevice->Release(); m_pDevice = nullptr; }
    }

    bool InitializeDX12(IDXGISwapChain* pSwapChain) {
        if (m_initialized) return true;
        if (!m_commandQueueReady || !m_pCommandQueue) return false;

        HRESULT hr = m_pCommandQueue->GetDevice(__uuidof(ID3D12Device), (void**)&m_pDevice);
        if (FAILED(hr) || !m_pDevice) return false;

        DXGI_SWAP_CHAIN_DESC desc;
        hr = pSwapChain->GetDesc(&desc);
        if (FAILED(hr)) return false;

        m_hWindow = desc.OutputWindow;
        m_bufferCount = desc.BufferCount;

        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.NumDescriptors = 1;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr = m_pDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_pSrvDescHeap));
        if (FAILED(hr)) return false;

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = m_bufferCount;
        hr = m_pDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_pRtvDescHeap));
        if (FAILED(hr)) return false;

        m_rtvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator));
        if (FAILED(hr)) return false;

        hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator, nullptr, IID_PPV_ARGS(&m_pCommandList));
        if (FAILED(hr)) return false;
        m_pCommandList->Close();

        m_pBackBuffers = new ID3D12Resource*[m_bufferCount];
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < m_bufferCount; i++) {
            hr = pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pBackBuffers[i]));
            if (FAILED(hr)) return false;
            m_pDevice->CreateRenderTargetView(m_pBackBuffers[i], nullptr, rtvHandle);
            rtvHandle.ptr += m_rtvDescriptorSize;
        }

        m_oWndProc = (WNDPROC)SetWindowLongPtr(m_hWindow, GWLP_WNDPROC, (LONG_PTR)WndProc);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.IniFilename = nullptr;

        ImGui_ImplWin32_Init(m_hWindow);

        ImGui_ImplDX12_InitInfo initInfo = {};
        initInfo.Device = m_pDevice;
        initInfo.CommandQueue = m_pCommandQueue;
        initInfo.NumFramesInFlight = m_bufferCount;
        initInfo.RTVFormat = desc.BufferDesc.Format;
        initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
        initInfo.SrvDescriptorHeap = m_pSrvDescHeap;
        initInfo.LegacySingleSrvCpuDescriptor = m_pSrvDescHeap->GetCPUDescriptorHandleForHeapStart();
        initInfo.LegacySingleSrvGpuDescriptor = m_pSrvDescHeap->GetGPUDescriptorHandleForHeapStart();

        if (!ImGui_ImplDX12_Init(&initInfo)) return false;

        m_initialized = true;
        return true;
    }

    void RenderImGui(IDXGISwapChain* pSwapChain) {
        if (!m_initialized) return;

        IDXGISwapChain3* pSwapChain3 = nullptr;
        UINT bufferIndex = 0;
        if (SUCCEEDED(pSwapChain->QueryInterface(IID_PPV_ARGS(&pSwapChain3)))) {
            bufferIndex = pSwapChain3->GetCurrentBackBufferIndex();
            pSwapChain3->Release();
        }

        m_pCommandAllocator->Reset();
        m_pCommandList->Reset(m_pCommandAllocator, nullptr);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = m_pBackBuffers[bufferIndex];
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        m_pCommandList->ResourceBarrier(1, &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += bufferIndex * m_rtvDescriptorSize;
        m_pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        ID3D12DescriptorHeap* heaps[] = { m_pSrvDescHeap };
        m_pCommandList->SetDescriptorHeaps(1, heaps);

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (m_renderCallback) {
            ImGuiIO& io = ImGui::GetIO();
            m_renderCallback(io.DisplaySize.x, io.DisplaySize.y);
        }

        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pCommandList);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        m_pCommandList->ResourceBarrier(1, &barrier);

        m_pCommandList->Close();
        ID3D12CommandList* ppCommandLists[] = { m_pCommandList };
        m_pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
    }

    // Static callbacks for hooks
    static DX12Overlay* s_instance;

    static void __stdcall HkExecuteCommandLists(ID3D12CommandQueue* pQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists) {
        if (s_instance && !s_instance->m_commandQueueReady && pQueue) {
            D3D12_COMMAND_QUEUE_DESC desc = pQueue->GetDesc();
            if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
                s_instance->m_pCommandQueue = pQueue;
                s_instance->m_commandQueueReady = true;
            }
        }
        if (s_instance) {
            s_instance->m_oExecuteCommandLists(pQueue, NumCommandLists, ppCommandLists);
        }
    }

    static HRESULT __stdcall HkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
        if (s_instance) {
            if (s_instance->m_updateCallback) s_instance->m_updateCallback();

            if (!s_instance->m_initialized && s_instance->m_commandQueueReady) {
                s_instance->InitializeDX12(pSwapChain);
            }

            if (s_instance->m_initialized) {
                s_instance->RenderImGui(pSwapChain);
            }

            return s_instance->m_oPresent(pSwapChain, SyncInterval, Flags);
        }
        return S_OK;
    }

    static HRESULT __stdcall HkPresent1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
        if (s_instance) {
            if (s_instance->m_updateCallback) s_instance->m_updateCallback();

            if (!s_instance->m_initialized && s_instance->m_commandQueueReady) {
                s_instance->InitializeDX12(pSwapChain);
            }

            if (s_instance->m_initialized) {
                s_instance->RenderImGui(pSwapChain);
            }

            return s_instance->m_oPresent1(pSwapChain, SyncInterval, Flags, pPresentParameters);
        }
        return S_OK;
    }

    static HRESULT __stdcall HkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
        if (s_instance) {
            if (s_instance->m_pBackBuffers) {
                for (UINT i = 0; i < s_instance->m_bufferCount; i++) {
                    if (s_instance->m_pBackBuffers[i]) {
                        s_instance->m_pBackBuffers[i]->Release();
                        s_instance->m_pBackBuffers[i] = nullptr;
                    }
                }
            }

            HRESULT hr = s_instance->m_oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

            if (SUCCEEDED(hr) && s_instance->m_initialized && s_instance->m_pBackBuffers) {
                s_instance->m_bufferCount = BufferCount;
                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = s_instance->m_pRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
                for (UINT i = 0; i < s_instance->m_bufferCount; i++) {
                    pSwapChain->GetBuffer(i, IID_PPV_ARGS(&s_instance->m_pBackBuffers[i]));
                    s_instance->m_pDevice->CreateRenderTargetView(s_instance->m_pBackBuffers[i], nullptr, rtvHandle);
                    rtvHandle.ptr += s_instance->m_rtvDescriptorSize;
                }
            }

            return hr;
        }
        return S_OK;
    }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (s_instance && s_instance->m_oWndProc) {
            return CallWindowProc(s_instance->m_oWndProc, hWnd, msg, wParam, lParam);
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    // State
    bool m_hookInstalled = false;
    bool m_initialized = false;
    bool m_commandQueueReady = false;
    DX12OverlayConfig m_config;

    // Callbacks
    RenderCallback m_renderCallback;
    UpdateCallback m_updateCallback;

    // Original functions
    ExecuteCommandLists_t m_oExecuteCommandLists = nullptr;
    Present_t m_oPresent = nullptr;
    Present1_t m_oPresent1 = nullptr;
    ResizeBuffers_t m_oResizeBuffers = nullptr;

    // DX12 objects
    ID3D12Device* m_pDevice = nullptr;
    ID3D12CommandQueue* m_pCommandQueue = nullptr;
    ID3D12DescriptorHeap* m_pSrvDescHeap = nullptr;
    ID3D12CommandAllocator* m_pCommandAllocator = nullptr;
    ID3D12GraphicsCommandList* m_pCommandList = nullptr;
    ID3D12Resource** m_pBackBuffers = nullptr;
    ID3D12DescriptorHeap* m_pRtvDescHeap = nullptr;
    UINT m_rtvDescriptorSize = 0;
    UINT m_bufferCount = 0;

    // Window
    HWND m_hWindow = nullptr;
    WNDPROC m_oWndProc = nullptr;
};

// Static instance pointer definition
inline DX12Overlay* DX12Overlay::s_instance = nullptr;

#else // !CAMERAUNLOCK_DX12_OVERLAY_IMPLEMENTATION

// Forward declaration for non-implementation builds
class DX12Overlay;

#endif // CAMERAUNLOCK_DX12_OVERLAY_IMPLEMENTATION

} // namespace cameraunlock::rendering
