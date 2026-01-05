// ────────────────────────────────────────────
//  File: camera.hpp · Created by Yash Patel · 9-7-2025
// ────────────────────────────────────────────

#pragma once

#include <array>

#include "platform/event_listener.hpp"
#include "math3d.hpp"

namespace keplar
{
    class Camera : public EventListener
    {
        public:
            enum class Mode
            {
                Fps,
                Cinematic,
                Turntable
            };

            // creation and destruction
            Camera(Mode mode, float fovy, float aspect, float znear, float zfar) noexcept;
            ~Camera() = default;

            // update every frame
            void update(float dt);

            // handle window and user input events
            void onWindowResize(uint32_t width, uint32_t height) override;
            void onKeyPressed(uint32_t key) override;
            void onKeyReleased(uint32_t key) override;
            void onMouseMove(double xpos, double ypos) override;
            void onMouseScroll(double yoffset) override;
            void onMouseButtonPressed(uint32_t button, int xpos, int ypos) override;
            void onMouseButtonReleased(uint32_t button, int xpos, int ypos) override;

            // accessors
            const glm::mat4& getViewMatrix() const noexcept         { return m_viewMatrix; }
            const glm::mat4& getProjectionMatrix() const noexcept   { return m_projectionMatrix; }
            const glm::vec3& getPosition() const noexcept           { return m_position; }
            const glm::vec3& getFront() const noexcept              { return m_front; }
            const glm::vec3& getUp() const noexcept                 { return m_up; }
            const glm::vec3& getRight() const noexcept              { return m_right; }
            
            float getFov() const noexcept                           { return m_fovy; }
            float getAspectRatio() const noexcept                   { return m_aspect; }
            float getNearClip() const noexcept                      { return m_znear; }
            float getFarClip() const noexcept                       { return m_zfar; }
            float getSpeed() const noexcept                         { return m_speed; }
            float getSensitivity() const noexcept                   { return m_sensitivity; }
            float getScrollSpeed() const noexcept                   { return m_scrollSpeed; }
            float getPositionDamping() const noexcept               { return m_posDamping; }
            float getRotationDamping() const noexcept               { return m_rotDamping; }
            
            // controls 
            void setFov(float fovy) noexcept;
            void setClipPlanes(float znear, float zfar) noexcept;
            void setAspectRatio(float aspect) noexcept;
            void setSpeed(float speed) noexcept;
            void setSensitivity(float sensitivity) noexcept;
            void setScrollSpeed(float speed) noexcept;
            void setPositionDamping(float damping) noexcept;
            void setRotationDamping(float damping) noexcept;
            void setOrbitTarget(const glm::vec3& target) noexcept;

        private:
            // internal helpers
            void updateProjection() noexcept;
            void updateVectors() noexcept;
            void processKeyboard(float dt) noexcept;
            void processMouse(float dt) noexcept;

            // turntable helpers
            void initOrbitState() noexcept;
            glm::quat buildOrbitOrientation(float yaw, float pitch) const noexcept;
            glm::quat safeRotationBetweenVectors(const glm::vec3& from, const glm::vec3& to) const noexcept;

        private:
            // camera mode
            Mode m_mode;

            // projection 
            float m_fovy;
            float m_aspect;
            float m_znear;
            float m_zfar;
            float m_width;
            float m_height;

            // transform
            glm::vec3 m_position;
            glm::vec3 m_front;
            glm::vec3 m_up;
            glm::vec3 m_right;

            // rotation
            glm::quat m_orientation;
            glm::quat m_targetOrientation;
            float m_sensitivity;
            float m_scrollSpeed;

            // movement state
            glm::vec3 m_targetPosition;
            float m_speed;
            float m_posDamping;
            float m_rotDamping;
            std::array<bool, 1024> m_keys;

            // matrices
            glm::mat4 m_viewMatrix;
            glm::mat4 m_projectionMatrix;

            // mouse tracking 
            double m_lastMouseX;
            double m_lastMouseY;
            bool m_firstMouse;
            bool m_dragging; 

            // turntable camera state
            glm::vec3 m_orbitTarget;
            float m_orbitDistance;
            float m_orbitTargetDistance;

            // turntable angles 
            float m_orbitYaw;
            float m_orbitPitch;
            float m_orbitTargetYaw;
            float m_orbitTargetPitch;

            // drag anchor
            double m_dragStartX;
            double m_dragStartY;
            float  m_dragStartYaw;
            float  m_dragStartPitch;
    };
}   // namespace keplar

