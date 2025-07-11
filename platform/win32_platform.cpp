// ────────────────────────────────────────────
//  File: win32_platform.cpp · Created by Yash Patel · 6-25-2025
// ────────────────────────────────────────────

#include "win32_platform.hpp"
#include "utils/logger.hpp"
#include "vulkan/vk_utils.hpp"

namespace keplar
{
    Win32Platform::Win32Platform()
        : m_hInstance(GetModuleHandle(nullptr))
        , m_hwnd(NULL)
        , m_windowPlacement{sizeof(WINDOWPLACEMENT)}
        , m_width(0)
        , m_height(0)
        , m_shouldClose(false)
        , m_isFullscreen(false)
    {
    }

    Win32Platform::~Win32Platform() 
    {
        shutdown();
    }

    bool Win32Platform::initialize(const std::string& title, int width, int height) 
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
        wndclass.lpfnWndProc    = WndProc;                                         
        wndclass.cbClsExtra     = 0;                                                
        wndclass.cbWndExtra     = 0;                                                
        wndclass.hInstance      = m_hInstance;                                   
        wndclass.hCursor        = LoadCursor((HINSTANCE)NULL, IDC_ARROW);           
        wndclass.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);              
        wndclass.lpszClassName  = appName;                                          
        wndclass.lpszMenuName   = NULL;              
        
        // TODO: add logo
        // wndclass.hIcon        = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON1));
        // wndclass.hIconSm      = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON1));

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
        ShowWindow(m_hwnd, SW_SHOWDEFAULT);
		SetForegroundWindow(m_hwnd);
		SetFocus(m_hwnd);
        return true;
    }

    void Win32Platform::pollEvents() 
    {
        MSG msg {};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) 
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    bool Win32Platform::shouldClose() 
    {
        return m_shouldClose;
    }

    void Win32Platform::shutdown() 
    {
        // send destroy window message
        if (m_hwnd) 
        {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    VkSurfaceKHR Win32Platform::createVulkanSurface(VkInstance vkInstance) const 
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

    std::vector<std::string_view> Win32Platform::getSurfaceInstanceExtensions() const 
    {
        return { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
    }

    void* Win32Platform::getNativeWindowHandle() const 
    {
        return reinterpret_cast<void*>(m_hwnd);
    }

    uint32_t Win32Platform::getWindowWidth() const 
    {
        return m_width;
    }

    uint32_t Win32Platform::getWindowHeight() const 
    {
        return m_height;
    }

    LRESULT CALLBACK Win32Platform::WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
    {
        Win32Platform* platform = reinterpret_cast<Win32Platform*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (!platform)
        {
            return DefWindowProc(hwnd, iMsg, wParam, lParam);
        }

        switch (iMsg)
        {
            case WM_KEYDOWN:
                switch (wParam)
                {
                    case VK_ESCAPE:
                        platform->m_shouldClose = true;
                        break;

                    case VK_F11:
                        platform->toggleFullscreen();
                        break;

                    default:
                        break;
                }
                break;

            case WM_CLOSE:
                DestroyWindow(hwnd);
                break;

            case WM_DESTROY:
                platform->m_shouldClose = true;
                PostQuitMessage(0);
                break;

            default:
                break;
        }

        return (DefWindowProc(hwnd, iMsg, wParam, lParam));
    }

    void Win32Platform::toggleFullscreen()
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
                    GetMonitorInfo(MonitorFromWindow(m_hwnd, MONITORINFOF_PRIMARY), &monitorInfo))
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
