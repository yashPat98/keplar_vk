// ────────────────────────────────────────────
//  File: x11_platform.cpp · Created by Yash Patel · 5-12-2026
// ────────────────────────────────────────────

#include "x11_Platform.hpp"
#include "utils/logger.hpp"
#include "vulkan/vulkan_utils.hpp"

namespace
{
    static constexpr int kMinWindowWidth  = 1280;
    static constexpr int kMinWindowHeight = 720;

    // X11 mouse button indices
    static constexpr unsigned int kX11ButtonLeft   = Button1;  // 1
    static constexpr unsigned int kX11ButtonMiddle = Button2;  // 2
    static constexpr unsigned int kX11ButtonRight  = Button3;  // 3
    static constexpr unsigned int kX11ButtonWheelU = Button4;  // 4  (scroll up)
    static constexpr unsigned int kX11ButtonWheelD = Button5;  // 5  (scroll down)

    // _NET_WM_STATE client-message action codes (EWMH spec)
    static constexpr long kNetWmStateRemove = 0;
    static constexpr long kNetWmStateAdd    = 1;
    static constexpr long kNetWmStateToggle = 2;
}

namespace keplar
{
    X11Platform::X11Platform() noexcept
        : m_display(nullptr)
        , m_window(0)
        , m_screen(0)
        , m_wmDeleteWindow(0)
        , m_wmState(0)
        , m_wmStateFullscreen(0)
        , m_width(0)
        , m_height(0)
        , m_shouldClose(false)
        , m_isFullscreen(false)
        , m_imguiEvents(false)
    {
    }

    X11Platform::~X11Platform()
    {
        shutdown();
    }

    // ────────────────────────────────────────────
    //  lifecycle
    // ────────────────────────────────────────────

    bool X11Platform::initialize(const std::string& title, int width, int height, bool maximized) noexcept
    {
        // open connection to the X server (uses $DISPLAY env var)
        m_display = XOpenDisplay(nullptr);
        if (!m_display)
        {
            VK_LOG_FATAL("failed to open X11 display — is $DISPLAY set?");
            return false;
        }

        m_screen = DefaultScreen(m_display);
        m_width  = static_cast<uint32_t>(width);
        m_height = static_cast<uint32_t>(height);

        // center the window on the root display
        const int screenWidth  = DisplayWidth(m_display, m_screen);
        const int screenHeight = DisplayHeight(m_display, m_screen);
        const int xPos = (screenWidth  - width)  / 2;
        const int yPos = (screenHeight - height) / 2;

        // XSetWindowAttributes for background + border
        XSetWindowAttributes swa {};
        swa.background_pixel  = BlackPixel(m_display, m_screen);
        swa.border_pixel      = BlackPixel(m_display, m_screen);
        swa.event_mask        = KeyPressMask     | KeyReleaseMask   |
                                ButtonPressMask  | ButtonReleaseMask |
                                PointerMotionMask|
                                StructureNotifyMask |    // ConfigureNotify, DestroyNotify
                                FocusChangeMask;

        m_window = XCreateWindow(
            m_display,
            RootWindow(m_display, m_screen),
            xPos, yPos,
            m_width, m_height,
            0,                                          // border width
            DefaultDepth(m_display, m_screen),
            InputOutput,
            DefaultVisual(m_display, m_screen),
            CWBackPixel | CWBorderPixel | CWEventMask,
            &swa);

        if (!m_window)
        {
            VK_LOG_FATAL("failed to create X11 window");
            XCloseDisplay(m_display);
            m_display = nullptr;
            return false;
        }

        // set window title (WM_NAME and _NET_WM_NAME)
        XStoreName(m_display, m_window, title.c_str());

        // enforce minimum window size via WM size hints
        XSizeHints* sizeHints = XAllocSizeHints();
        if (sizeHints)
        {
            sizeHints->flags     = PMinSize;
            sizeHints->min_width  = kMinWindowWidth;
            sizeHints->min_height = kMinWindowHeight;
            XSetWMNormalHints(m_display, m_window, sizeHints);
            XFree(sizeHints);
        }

        // register the WM_DELETE_WINDOW protocol so the close button sends a
        // ClientMessage instead of brutally killing the connection
        m_wmDeleteWindow = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(m_display, m_window, &m_wmDeleteWindow, 1);

        // intern EWMH atoms needed for fullscreen toggling
        m_wmState           = XInternAtom(m_display, "_NET_WM_STATE",            False);
        m_wmStateFullscreen = XInternAtom(m_display, "_NET_WM_STATE_FULLSCREEN", False);

        // show the window
        if (maximized)
        {
            // request maximized state before mapping so the WM honors it
            Atom wmStateMaxH = XInternAtom(m_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
            Atom wmStateMaxV = XInternAtom(m_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
            Atom atoms[2]   = { wmStateMaxH, wmStateMaxV };
            XChangeProperty(m_display, m_window, m_wmState,
                            XA_ATOM, 32, PropModeReplace,
                            reinterpret_cast<unsigned char*>(atoms), 2);
        }

        XMapWindow(m_display, m_window);
        XRaiseWindow(m_display, m_window);
        XFlush(m_display);

        VK_LOG_INFO("X11 window created successfully (%dx%d).", m_width, m_height);
        return true;
    }

    void X11Platform::pollEvents() noexcept
    {
        // drain all pending events without blocking
        while (XPending(m_display) > 0)
        {
            XEvent event {};
            XNextEvent(m_display, &event);
            handleEvent(event);

            if (m_shouldClose)
            {
                break;
            }
        }
    }

    bool X11Platform::shouldClose() noexcept
    {
        return m_shouldClose;
    }

    void X11Platform::shutdown() noexcept
    {
        m_eventManager.removeAllListeners();

        if (m_display)
        {
            if (m_window)
            {
                XDestroyWindow(m_display, m_window);
                m_window = 0;
            }
            XCloseDisplay(m_display);
            m_display = nullptr;
            VK_LOG_INFO("X11 window destroyed successfully.");
        }
    }

    // ────────────────────────────────────────────
    //  window queries
    // ────────────────────────────────────────────

    void* X11Platform::getWindowHandle() const noexcept
    {
        // callers that need a Display* can recover it; here we return the Window XID
        // wrapped as a pointer for compatibility with the Platform interface
        return reinterpret_cast<void*>(m_window);
    }

    uint32_t X11Platform::getWindowWidth() const noexcept
    {
        return m_width;
    }

    uint32_t X11Platform::getWindowHeight() const noexcept
    {
        return m_height;
    }

    // ────────────────────────────────────────────
    //  event listeners
    // ────────────────────────────────────────────

    void X11Platform::addListener(const std::shared_ptr<EventListener>& listener) noexcept
    {
        m_eventManager.addListener(listener);
    }

    void X11Platform::removeListener(const std::shared_ptr<EventListener>& listener) noexcept
    {
        m_eventManager.removeListener(listener);
    }

    void X11Platform::enableImGuiEvents(bool enabled) noexcept
    {
        m_imguiEvents = enabled;
    }

    // ────────────────────────────────────────────
    //  vulkan surface
    // ────────────────────────────────────────────

    VkSurfaceKHR X11Platform::createSurface(VkInstance vkInstance) const noexcept
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (vkInstance == VK_NULL_HANDLE)
        {
            VK_LOG_FATAL("vkInstance is null, cannot create Xlib surface");
            return surface;
        }

        VkXlibSurfaceCreateInfoKHR createInfo {};
        createInfo.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext  = nullptr;
        createInfo.flags  = 0;
        createInfo.dpy    = m_display;
        createInfo.window = m_window;

        VK_CHECK(vkCreateXlibSurfaceKHR(vkInstance, &createInfo, nullptr, &surface));
        return surface;
    }

    std::vector<std::string_view> X11Platform::getSurfaceExtensions() const noexcept
    {
        return { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME };
    }

    // ────────────────────────────────────────────
    //  private — event dispatch
    // ────────────────────────────────────────────

    void X11Platform::handleEvent(XEvent& event) noexcept
    {
        switch (event.type)
        {
            // ── close button ──────────────────────────────────────────────
            case ClientMessage:
            {
                if (static_cast<Atom>(event.xclient.data.l[0]) == m_wmDeleteWindow)
                {
                    m_eventManager.onWindowClose();
                    m_shouldClose = true;
                }
                break;
            }

            // ── window resized / moved ────────────────────────────────────
            case ConfigureNotify:
            {
                const uint32_t newWidth  = static_cast<uint32_t>(event.xconfigure.width);
                const uint32_t newHeight = static_cast<uint32_t>(event.xconfigure.height);
                if (newWidth != m_width || newHeight != m_height)
                {
                    m_width  = newWidth;
                    m_height = newHeight;
                    m_eventManager.onWindowResize(m_width, m_height);
                }
                break;
            }

            // ── keyboard ──────────────────────────────────────────────────
            case KeyPress:
            {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                const uint32_t key = translateKeysym(keysym);

                m_eventManager.onKeyPressed(key);

                if (keysym == XK_F11)
                {
                    toggleFullscreen();
                }
                else if (keysym == XK_Escape)
                {
                    m_eventManager.onWindowClose();
                    m_shouldClose = true;
                }
                break;
            }

            case KeyRelease:
            {
                // X11 auto-repeats by firing a KeyRelease immediately followed by
                // a KeyPress — filter those out so listeners see clean transitions
                if (XPending(m_display) > 0)
                {
                    XEvent next {};
                    XPeekEvent(m_display, &next);
                    if (next.type == KeyPress &&
                        next.xkey.keycode == event.xkey.keycode &&
                        next.xkey.time    == event.xkey.time)
                    {
                        break; // swallow the synthetic release
                    }
                }
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                m_eventManager.onKeyReleased(translateKeysym(keysym));
                break;
            }

            // ── mouse buttons ─────────────────────────────────────────────
            case ButtonPress:
            {
                const unsigned int btn = event.xbutton.button;
                const int xpos = event.xbutton.x;
                const int ypos = event.xbutton.y;

                if (btn == kX11ButtonWheelU)
                {
                    m_eventManager.onMouseScroll(+1.0);
                }
                else if (btn == kX11ButtonWheelD)
                {
                    m_eventManager.onMouseScroll(-1.0);
                }
                else
                {
                    // map X11 button indices to the same convention as Win32:
                    //   0 = left, 1 = right, 2 = middle
                    uint32_t mapped = 0;
                    if      (btn == kX11ButtonLeft)   mapped = 0;
                    else if (btn == kX11ButtonRight)  mapped = 1;
                    else if (btn == kX11ButtonMiddle) mapped = 2;
                    m_eventManager.onMouseButtonPressed(mapped, xpos, ypos);
                }
                break;
            }

            case ButtonRelease:
            {
                const unsigned int btn = event.xbutton.button;
                const int xpos = event.xbutton.x;
                const int ypos = event.xbutton.y;

                // ignore scroll-wheel pseudo-releases
                if (btn == kX11ButtonWheelU || btn == kX11ButtonWheelD)
                {
                    break;
                }

                uint32_t mapped = 0;
                if      (btn == kX11ButtonLeft)   mapped = 0;
                else if (btn == kX11ButtonRight)  mapped = 1;
                else if (btn == kX11ButtonMiddle) mapped = 2;
                m_eventManager.onMouseButtonReleased(mapped, xpos, ypos);
                break;
            }

            // ── mouse motion ──────────────────────────────────────────────
            case MotionNotify:
            {
                m_eventManager.onMouseMove(
                    static_cast<double>(event.xmotion.x),
                    static_cast<double>(event.xmotion.y));
                break;
            }

            // ── focus ─────────────────────────────────────────────────────
            case FocusIn:
            {
                m_eventManager.onWindowFocus(true);
                break;
            }

            case FocusOut:
            {
                m_eventManager.onWindowFocus(false);
                break;
            }

            default:
                break;
        }
    }

    // ────────────────────────────────────────────
    //  private — fullscreen toggle (EWMH)
    // ────────────────────────────────────────────

    void X11Platform::toggleFullscreen() noexcept
    {
        XEvent xev {};
        xev.type                 = ClientMessage;
        xev.xclient.window       = m_window;
        xev.xclient.message_type = m_wmState;
        xev.xclient.format       = 32;
        xev.xclient.data.l[0]   = kNetWmStateToggle;
        xev.xclient.data.l[1]   = static_cast<long>(m_wmStateFullscreen);
        xev.xclient.data.l[2]   = 0;
        xev.xclient.data.l[3]   = 1; // source: application
        xev.xclient.data.l[4]   = 0;

        XSendEvent(
            m_display,
            DefaultRootWindow(m_display),
            False,
            SubstructureNotifyMask | SubstructureRedirectMask,
            &xev);

        XFlush(m_display);
        m_isFullscreen = !m_isFullscreen;
    }

    // ────────────────────────────────────────────
    //  private — keysym translation
    //  Maps X11 KeySyms to a stable uint32_t key code.
    //  Extended mappings can be added here as needed.
    // ────────────────────────────────────────────

    uint32_t X11Platform::translateKeysym(KeySym keysym) const noexcept
    {
        // function keys
        if (keysym >= XK_F1 && keysym <= XK_F12)
        {
            return static_cast<uint32_t>(0x70 + (keysym - XK_F1)); // F1=0x70 … F12=0x7B
        }

        switch (keysym)
        {
            // navigation
            case XK_Escape:         return 0x1B;
            case XK_Return:         return 0x0D;
            case XK_BackSpace:      return 0x08;
            case XK_Tab:            return 0x09;
            case XK_Delete:         return 0x2E;
            case XK_Insert:         return 0x2D;
            case XK_Home:           return 0x24;
            case XK_End:            return 0x23;
            case XK_Prior:          return 0x21; // Page Up
            case XK_Next:           return 0x22; // Page Down

            // arrow keys
            case XK_Left:           return 0x25;
            case XK_Right:          return 0x27;
            case XK_Up:             return 0x26;
            case XK_Down:           return 0x28;

            // modifiers
            case XK_Shift_L:
            case XK_Shift_R:        return 0x10;
            case XK_Control_L:
            case XK_Control_R:      return 0x11;
            case XK_Alt_L:
            case XK_Alt_R:          return 0x12;

            // space and printable ASCII — keysym == Unicode code point for Basic Latin
            default:
                if (keysym >= 0x20 && keysym <= 0x7E)
                {
                    // uppercase ASCII letters (match Win32 VK codes)
                    if (keysym >= 'a' && keysym <= 'z')
                    {
                        return static_cast<uint32_t>(keysym - 'a' + 'A');
                    }
                    return static_cast<uint32_t>(keysym);
                }
                return static_cast<uint32_t>(keysym);
        }
    }
}   // namespace keplar
