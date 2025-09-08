// ────────────────────────────────────────────
//  File: event_manager.cpp · Created by Yash Patel · 9-26-2025
// ────────────────────────────────────────────

#include "event_manager.hpp"

namespace keplar
{
    void EventManager::addListener(EventListener* listener) noexcept
    {
        if (!listener)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (std::find(m_listeners.begin(), m_listeners.end(), listener) == m_listeners.end())
        {
            m_listeners.emplace_back(listener);
        }
    }

    void EventManager::removeListener(EventListener* listener) noexcept
    {
        if (!listener)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener), m_listeners.end());
    }

    void EventManager::removeAllListeners() noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listeners.clear();
    }

    void EventManager::onWindowResize(uint32_t width, uint32_t height) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto listener : snapshot)
        {
            listener->onWindowResize(width, height);
        }
    }

    void EventManager::onWindowClose() const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto listener : snapshot)
        {
            listener->onWindowClose();
        }
    }

    void EventManager::onWindowFocus(bool focused) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto listener : snapshot)
        {
            listener->onWindowFocus(focused);
        }
    }

    void EventManager::onKeyPressed(uint32_t key) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto listener : snapshot)
        {
            listener->onKeyPressed(key);
        }
    }

    void EventManager::onKeyReleased(uint32_t key) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto listener : snapshot)
        {
            listener->onKeyReleased(key);
        }
    }

    void EventManager::onMouseMove(double x, double y) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto listener : snapshot)
        {
            listener->onMouseMove(x, y);
        }
    }

    void EventManager::onMouseScroll(double delta) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto listener : snapshot)
        {
            listener->onMouseScroll(delta);
        }
    }

    void EventManager::onMouseButtonPressed(uint32_t button) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto listener : snapshot)
        {
            listener->onMouseButtonPressed(button);
        }
    }

    void EventManager::onMouseButtonReleased(uint32_t button) const noexcept
    {
        auto snapshot = getListenerSnapshot();
        for (auto listener : snapshot)
        {
            listener->onMouseButtonReleased(button);
        }
    }

    std::vector<EventListener*> EventManager::getListenerSnapshot() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_listeners;
    }
}   // namespace keplar
