// ────────────────────────────────────────────
//  File: camera.hpp · Created by Yash Patel · 9-7-2025
// ────────────────────────────────────────────

#pragma once

#include <array>

#include "platform/event_listener.hpp"
#include "common.hpp"

namespace keplar
{
    class Camera : public EventListener
    {
        public:
            // creation and destruction
            Camera(float fovy, float aspect, float znear, float zfar, bool cinematic = false) noexcept;
            ~Camera() = default;

            // update every frame
            void update(float dt);

            // accessors
            const glm::mat4& getViewMatrix() const noexcept         { return m_viewMatrix; }
            const glm::mat4& getProjectionMatrix() const noexcept   { return m_projectionMatrix; }
            const glm::vec3& getPosition() const noexcept           { return m_position; }
            const glm::vec3& getFront() const noexcept              { return m_front; }
            const glm::vec3& getUp() const noexcept                 { return m_up; }
            const glm::vec3& getRight() const noexcept              { return m_right; }

            // handle window and user input events
            void onWindowResize(uint32_t width, uint32_t height) override;
            void onKeyPressed(uint32_t key) override;
            void onKeyReleased(uint32_t key) override;
            void onMouseMove(double xpos, double ypos) override;
            void onMouseScroll(double yoffset) override;

            // optional 
            void setAspectRatio(float aspect) noexcept;
            void setCinematicMode(bool enabled) noexcept;
            void setSpeed(float speed) noexcept;
            void setPositionDamping(float damping) noexcept;
            void setRotationDamping(float damping) noexcept;

        private:
            // internal helpers
            void updateVectors() noexcept;
            void processKeyboard(float dt) noexcept;
            void processMouse(float dt) noexcept;

        private:
            // projection 
            float m_fovy;
            float m_aspect;
            float m_znear;
            float m_zfar;
            bool m_cinematicMode;

            // transform
            glm::vec3 m_position;
            glm::vec3 m_front;
            glm::vec3 m_up;
            glm::vec3 m_right;

            // rotation
            float m_yaw;
            float m_pitch;
            float m_sensitivity;
            float m_scrollSpeed;

            // movement state
            glm::vec3 m_targetPosition;
            float m_targetYaw;
            float m_targetPitch;
            float m_speed;
            float m_posDamping;
            float m_rotDamping;
            float m_rotAcceleration;
            glm::vec2 m_rotVelocity;
            std::array<bool, 1024> m_keys;

            // matrices
            glm::mat4 m_viewMatrix;
            glm::mat4 m_projectionMatrix;

            // mouse tracking 
            double m_lastMouseX;
            double m_lastMouseY;
            bool m_firstMouse;
    };
}   // namespace keplar

