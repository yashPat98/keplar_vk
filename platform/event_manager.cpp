// ────────────────────────────────────────────
//  File: event_manager.cpp · Created by Yash Patel · 9-26-2025
// ────────────────────────────────────────────

#include "event_manager.hpp"
#include <algorithm>

namespace keplar
{
    void EventManager::addListener(const std::shared_ptr<EventListener>& listener) noexcept
    {
        if (!listener)
        {
            return;
        }

        // check if the listener is already registered
        std::lock_guard<std::mutex> lock(m_mutex);
        if (std::none_of(m_listeners.begin(), m_listeners.end(), [&listener](const auto& weak){ return weak.lock() == listener; }))
        {
            m_listeners.emplace_back(listener);
        }
    }

    void EventManager::removeListener(const std::shared_ptr<EventListener>& listener) noexcept
    {
        if (!listener)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto itr = std::remove_if(m_listeners.begin(), m_listeners.end(), [&listener](const auto& weak)
        {
            auto registered = weak.lock();
            return !registered || registered == listener;
        });

        if (itr != m_listeners.end())
        {
            m_listeners.erase(itr, m_listeners.end());
        }
    }

    void EventManager::removeAllListeners() noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listeners.clear();
    }

    void EventManager::onWindowResize(uint32_t width, uint32_t height) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto& weak_ptr : snapshot)
        {
            if (auto listener = weak_ptr.lock())
            {
                listener->onWindowResize(width, height);
            }
        }
    }

    void EventManager::onWindowClose() const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto& weak_ptr : snapshot)
        {
            if (auto listener = weak_ptr.lock())
            {
                listener->onWindowClose();
            }
        }
    }

    void EventManager::onWindowFocus(bool focused) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto& weak_ptr : snapshot)
        {
            if (auto listener = weak_ptr.lock())
            {
                listener->onWindowFocus(focused);
            }
        }
    }

    void EventManager::onKeyPressed(uint32_t key) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto& weak_ptr : snapshot)
        {
            if (auto listener = weak_ptr.lock())
            {
                listener->onKeyPressed(key);
            }
        }
    }

    void EventManager::onKeyReleased(uint32_t key) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto& weak_ptr : snapshot)
        {
            if (auto listener = weak_ptr.lock())
            {
                listener->onKeyReleased(key);
            }
        }
    }

    void EventManager::onMouseMove(double xpos, double ypos) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto& weak_ptr : snapshot)
        {
            if (auto listener = weak_ptr.lock())
            {
                listener->onMouseMove(xpos, ypos);
            }
        }
    }

    void EventManager::onMouseScroll(double yoffset) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto& weak_ptr : snapshot)
        {
            if (auto listener = weak_ptr.lock())
            {
                listener->onMouseScroll(yoffset);
            }
        }
    }

    void EventManager::onMouseButtonPressed(uint32_t button, int xpos, int ypos) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto& weak_ptr : snapshot)
        {
            if (auto listener = weak_ptr.lock())
            {
                listener->onMouseButtonPressed(button, xpos, ypos);
            }
        }
    }

    void EventManager::onMouseButtonReleased(uint32_t button, int xpos, int ypos) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto& weak_ptr : snapshot)
        {
            if (auto listener = weak_ptr.lock())
            {
                listener->onMouseButtonReleased(button, xpos, ypos);
            }
        }
    }

    std::vector<std::weak_ptr<EventListener>> EventManager::getListenerSnapshot() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_listeners;
    }
}   // namespace keplar
