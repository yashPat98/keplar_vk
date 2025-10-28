// ────────────────────────────────────────────
//  File: win32_platform.cpp · Created by Yash Patel · 6-25-2025
// ────────────────────────────────────────────

#include "win32_platform.hpp"
#include "resource.hpp"
#include "utils/logger.hpp"
#include "vulkan/vulkan_utils.hpp"

// forward declaration of ImGui message handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace 
{
    static constexpr int kMinWindowWidth  = 1280;
    static constexpr int kMinWindowHeight = 720;
}

namespace keplar
{
    Win32Platform::Win32Platform() noexcept
        : m_hInstance(GetModuleHandle(nullptr))
        , m_hwnd(NULL)
        , m_windowPlacement{sizeof(WINDOWPLACEMENT)}
        , m_width(0)
        , m_height(0)
        , m_shouldClose(false)
        , m_isFullscreen(false)
        , m_imguiEvents(false)
    {
    }

    Win32Platform::~Win32Platform() 
    {
        shutdown();
    }

    bool Win32Platform::initialize(const std::string& title, int width, int height, bool maximized) noexcept 
    {
        // convert title to wide string
        TCHAR appName[256];   
        wsprintf(appName, TEXT("%s"), title.c_str());
        m_width = width;
        m_height = height;

        // initialize the window class
		WNDCLASSEX wndclass {};
        wndclass.cbSize         = sizeof(WNDCLASSEX);                               
        wndclass.style          = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;              
        wndclass.lpfnWndProc    = Win32Platform::WndProc;                                         
        wndclass.cbClsExtra     = 0;                                                
        wndclass.cbWndExtra     = 0;                                                
        wndclass.hInstance      = m_hInstance;  
        wndclass.hIcon          = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON1));  
        wndclass.hIconSm        = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON1));                               
        wndclass.hCursor        = LoadCursor((HINSTANCE)NULL, IDC_ARROW);           
        wndclass.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);              
        wndclass.lpszClassName  = appName;                                          
        wndclass.lpszMenuName   = NULL;              

		// register the window class
		RegisterClassEx(&wndclass);

		// find x, y for centered window
		int cxWindow = (GetSystemMetrics(SM_CXSCREEN) - m_width) / 2;
		int cyWindow = (GetSystemMetrics(SM_CYSCREEN) - m_height) / 2;

		// create window
		m_hwnd = CreateWindowEx(WS_EX_APPWINDOW,
			appName,
			appName,
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			cxWindow,
			cyWindow,
			m_width,
			m_height,
			(HWND)NULL,
			(HMENU)NULL,
			m_hInstance,
			(LPVOID)NULL);

		if (!m_hwnd)
		{
            VK_LOG_FATAL("failed to create window (GetLastError: %lu)", GetLastError());
			return false;
		}

        // store this pointer for message routing
        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        FreeConsole();
        ShowWindow(m_hwnd, maximized ? SW_MAXIMIZE : SW_SHOWDEFAULT);
		SetForegroundWindow(m_hwnd);
		SetFocus(m_hwnd);
        VK_LOG_INFO("window is created successfully.");
        return true;
    }

    void Win32Platform::pollEvents() noexcept 
    {
        MSG msg {};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) 
        {
            // signal render loop to exit
            if (msg.message == WM_QUIT)
            {
                m_shouldClose = true;
                break;
            }
            
            // translate and dispatch to wndProc
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
 
    bool Win32Platform::shouldClose() noexcept
    {
        return m_shouldClose;
    }

    void Win32Platform::shutdown() noexcept
    {
        // remove all listeners on shutdown
        m_eventManager.removeAllListeners();

        // destroy window 
        if (m_hwnd) 
        {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
            VK_LOG_INFO("window is destroyed successfully");
        }
    }

    void* Win32Platform::getWindowHandle() const noexcept
    {
        return reinterpret_cast<void*>(m_hwnd);
    }

    uint32_t Win32Platform::getWindowWidth() const noexcept
    {
        return m_width;
    }

    uint32_t Win32Platform::getWindowHeight() const noexcept
    {
        return m_height;
    }

    void Win32Platform::addListener(const std::shared_ptr<EventListener>& listener) noexcept 
    {
        m_eventManager.addListener(listener);
    }

    void Win32Platform::removeListener(const std::shared_ptr<EventListener>& listener) noexcept 
    {
        m_eventManager.removeListener(listener);
    }

    void Win32Platform::enableImGuiEvents(bool enabled) noexcept 
    {
        m_imguiEvents = enabled;
    }

    VkSurfaceKHR Win32Platform::createSurface(VkInstance vkInstance) const noexcept
    {
        VkSurfaceKHR vkSurfaceKHR = VK_NULL_HANDLE;  
        if (vkInstance == VK_NULL_HANDLE) 
        {
            VK_LOG_FATAL("vkInstance is null, cannot create Win32 surface");
            return vkSurfaceKHR;
        }

        VkWin32SurfaceCreateInfoKHR vkWin32SurfaceCreateInfoKHR {};
        vkWin32SurfaceCreateInfoKHR.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;        
        vkWin32SurfaceCreateInfoKHR.pNext = nullptr;                                                   
        vkWin32SurfaceCreateInfoKHR.flags = 0;                                                      
        vkWin32SurfaceCreateInfoKHR.hinstance = m_hInstance; 
        vkWin32SurfaceCreateInfoKHR.hwnd = m_hwnd;                                                   

        VK_CHECK(vkCreateWin32SurfaceKHR(vkInstance, &vkWin32SurfaceCreateInfoKHR, nullptr, &vkSurfaceKHR));
        return vkSurfaceKHR;
    }

    std::vector<std::string_view> Win32Platform::getSurfaceExtensions() const noexcept
    {
        return { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
    }

    LRESULT CALLBACK Win32Platform::WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
    {
        Win32Platform* platform = reinterpret_cast<Win32Platform*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (!platform)
        {
            return DefWindowProc(hwnd, iMsg, wParam, lParam);
        }

        if (platform->m_imguiEvents)
        {
            ImGui_ImplWin32_WndProcHandler(hwnd, iMsg, wParam, lParam);
        }

        switch (iMsg)
        {
            case WM_CLOSE:
                // prevent default window destruction by DefWindowProc 
                // to ensure window and vulkan surface remains valid until 
                // render loop is exited gracefully
                platform->m_eventManager.onWindowClose();
                PostQuitMessage(0);
                return 0;

            case WM_GETMINMAXINFO:
                reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize.x = kMinWindowWidth;
                reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize.y = kMinWindowHeight;
                return 0;

            case WM_SIZE:
                platform->m_width = LOWORD(lParam);
                platform->m_height = HIWORD(lParam);
                platform->m_eventManager.onWindowResize(platform->m_width, platform->m_height);
                break;

            case WM_KEYDOWN:
                platform->m_eventManager.onKeyPressed(static_cast<uint32_t>(wParam));
                switch (wParam)
                {
                    case VK_F11:
                        platform->toggleFullscreen();
                        break;

                    case VK_ESCAPE:
                        PostQuitMessage(0);
                        break;

                    default:
                        break;
                }
                break;

            case WM_KEYUP:
                platform->m_eventManager.onKeyReleased(static_cast<uint32_t>(wParam));
                break;

            case WM_MOUSEMOVE:
                platform->m_eventManager.onMouseMove(static_cast<uint32_t>(LOWORD(lParam)), static_cast<uint32_t>(HIWORD(lParam)));
                break;

            case WM_MOUSEWHEEL:
                platform->m_eventManager.onMouseScroll(static_cast<double>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
                break;

            case WM_SETFOCUS:    platform->m_eventManager.onWindowFocus(true);      break;
            case WM_KILLFOCUS:   platform->m_eventManager.onWindowFocus(false);     break;
            case WM_LBUTTONDOWN: platform->m_eventManager.onMouseButtonPressed(0);  break;
            case WM_LBUTTONUP:   platform->m_eventManager.onMouseButtonReleased(0); break;
            case WM_RBUTTONDOWN: platform->m_eventManager.onMouseButtonPressed(1);  break;
            case WM_RBUTTONUP:   platform->m_eventManager.onMouseButtonReleased(1); break;
            case WM_MBUTTONDOWN: platform->m_eventManager.onMouseButtonPressed(2);  break;
            case WM_MBUTTONUP:   platform->m_eventManager.onMouseButtonReleased(2); break;

            default:
                break;
        }

        return (DefWindowProc(hwnd, iMsg, wParam, lParam));
    }

    void Win32Platform::toggleFullscreen() noexcept
    {
        MONITORINFO monitorInfo {};                                            
        monitorInfo.cbSize = sizeof(MONITORINFO);                    
        if (!m_isFullscreen)                             
        {
            // save current window placement and style
            m_windowStyle = GetWindowLong(m_hwnd, GWL_STYLE);       
            if (m_windowStyle & WS_OVERLAPPEDWINDOW)               
            {
                if (GetWindowPlacement(m_hwnd, &m_windowPlacement) && 
                    GetMonitorInfo(MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
                {
                    SetWindowLong(m_hwnd, GWL_STYLE, (m_windowStyle & ~WS_OVERLAPPEDWINDOW));
                    SetWindowPos(m_hwnd, 
                        HWND_TOP, 
                        monitorInfo.rcMonitor.left, 
                        monitorInfo.rcMonitor.top, 
                        monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, 
                        monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,             
                        SWP_NOZORDER | SWP_FRAMECHANGED);                   
                }                                                          
            }
            ShowCursor(FALSE);                                 
            m_isFullscreen = true;                          
        }
        else                                                   
        { 
            // restore window style and placement
            SetWindowLong(m_hwnd, GWL_STYLE, (m_windowStyle | WS_OVERLAPPEDWINDOW));
            SetWindowPlacement(m_hwnd, &m_windowPlacement);
            SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
            ShowCursor(TRUE);            
            m_isFullscreen = false;
        }
    }
}   // namespace keplar
