// ────────────────────────────────────────────
//  File: camera.cpp · Created by Yash Patel · 9-7-2025
// ────────────────────────────────────────────

#include "camera.hpp"
#include <algorithm>

namespace 
{
    static inline constexpr float CAMERA_DEFAULT_SPEED       = 10.0f;
    static inline constexpr float CAMERA_DEFAULT_DAMPING     = 1000.0f;
    static inline constexpr float CAMERA_CINEMATIC_DAMPING   = 8.0f;
    static inline constexpr float CAMERA_DEFAULT_SENSITIVITY = 0.1f;
    static inline constexpr float CAMERA_DEFAULT_SCROLLSPEED = 2.0f;
}

namespace keplar
{
    Camera::Camera(float fovy, float aspect, float znear, float zfar, bool cinematic) noexcept
        : m_fovy(fovy)
        , m_aspect(aspect)
        , m_znear(znear)
        , m_zfar(zfar)
        , m_cinematicMode(cinematic)
        , m_position(0.0f, 1.0f, 3.0f)
        , m_front(0.0f, 0.0f, -1.0f)
        , m_up(0.0f, 1.0f, 0.0f)
        , m_right(1.0f, 0.0f, 0.0f)
        , m_yaw(-90.0f)
        , m_pitch(0.0f)
        , m_sensitivity(CAMERA_DEFAULT_SENSITIVITY)
        , m_scrollSpeed(CAMERA_DEFAULT_SCROLLSPEED)
        , m_targetPosition(0.0f, 1.0f, 3.0f)
        , m_targetYaw(-90.0f)
        , m_targetPitch(0.0f)
        , m_speed(CAMERA_DEFAULT_SPEED)
        , m_rotAcceleration(20.0f)
        , m_rotVelocity(0.0f)
        , m_keys{}
        , m_viewMatrix(1.0f)
        , m_lastMouseX(0.0)
        , m_lastMouseY(0.0)
        , m_firstMouse(true)
    {
        // set damping based on camera mode
        if (m_cinematicMode)
        {
            m_posDamping = CAMERA_CINEMATIC_DAMPING;
            m_rotDamping = CAMERA_CINEMATIC_DAMPING;
        }
        else 
        {
            m_posDamping = CAMERA_DEFAULT_DAMPING;
            m_rotDamping = CAMERA_DEFAULT_DAMPING;
        }

        // initialize projection matrix and flip y-axis for Vulkan NDC
        m_projectionMatrix = glm::perspective(glm::radians(m_fovy), m_aspect, m_znear, m_zfar);
        m_projectionMatrix[1][1] *= -1.0f;
        updateVectors();
    }

    void Camera::update(float dt)
    {
        // update camera state for current frame
        processKeyboard(dt);
        processMouse(dt);
        updateVectors();
    }

    void Camera::onWindowResize(uint32_t width, uint32_t height) 
    {
        if (height != 0)
        {
            // update aspect ratio and projection matrix 
            m_aspect = static_cast<float>(width) / height;
            m_projectionMatrix = glm::perspective(glm::radians(m_fovy), m_aspect, m_znear, m_zfar);
            m_projectionMatrix[1][1] *= -1.0f;

            // reset first-mouse to avoid jump after resize
            m_firstMouse = true;
        }
    }

    void Camera::onKeyPressed(uint32_t key) 
    {
        // track key press states
        if (key < m_keys.size()) 
        {
            m_keys[key] = true;
        }
    }

    void Camera::onKeyReleased(uint32_t key) 
    {
        // track key release states
        if (key < m_keys.size()) 
        {
            m_keys[key] = false;
        }
    }

    void Camera::onMouseMove(double xpos, double ypos) 
    {
        // skip first event to prevent sudden jump
        if (m_firstMouse)
        {
            m_lastMouseX = xpos;
            m_lastMouseY = ypos;
            m_firstMouse = false;
        }

        // compute mouse movement delta
        double dx = xpos - m_lastMouseX;
        double dy = m_lastMouseY - ypos; 

        // store current mouse position
        m_lastMouseX = xpos;
        m_lastMouseY = ypos;

        // update target yaw and pitch based on mouse movement and sensitivity
        m_targetYaw   += static_cast<float>(dx) * m_sensitivity;
        m_targetPitch += static_cast<float>(dy) * m_sensitivity;

        // clamp pitch to avoid gimbal lock
        m_targetPitch = std::clamp(m_targetPitch, -89.0f, 89.0f);
    }

    void Camera::onMouseScroll(double yoffset) 
    {
        // adjust field-of-view with scroll and update projection
        m_fovy -= static_cast<float>(yoffset) * m_scrollSpeed;
        m_fovy = std::clamp(m_fovy, 1.0f, 120.0f);
        m_projectionMatrix = glm::perspective(glm::radians(m_fovy), m_aspect, m_znear, m_zfar);
        m_projectionMatrix[1][1] *= -1.0f;
    }

    void Camera::setAspectRatio(float aspect) noexcept
    {
        // update aspect ratio and projection matrix
        m_aspect = aspect;
        m_projectionMatrix = glm::perspective(glm::radians(m_fovy), m_aspect, m_znear, m_zfar);
        m_projectionMatrix[1][1] *= -1.0f;
    }

    void Camera::setCinematicMode(bool enabled) noexcept
    {
        m_cinematicMode = enabled;
        if (m_cinematicMode)
        {
            m_posDamping = CAMERA_CINEMATIC_DAMPING;
            m_rotDamping = CAMERA_CINEMATIC_DAMPING;
        }
        else 
        {
            m_posDamping = CAMERA_DEFAULT_DAMPING;
            m_rotDamping = CAMERA_DEFAULT_DAMPING;

            // immediate snap and clear velocities to avoid drift
            m_targetPosition = m_position;
            m_targetYaw = m_yaw;
            m_targetPitch = m_pitch;
            m_rotVelocity = glm::vec2(0.0f);
        }
    }

    void Camera::setSpeed(float speed) noexcept
    {
        m_speed = speed;
    }

    void Camera::setPositionDamping(float damping) noexcept
    {
        m_posDamping = damping;
    }

    void Camera::setRotationDamping(float damping) noexcept
    {
        m_rotDamping = damping;
    }

    void Camera::updateVectors() noexcept
    {
        // recompute vector from updated yaw and pitch
        glm::vec3 front;
        front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        front.y = sin(glm::radians(m_pitch));
        front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));

        m_front = glm::normalize(front);
        m_right = glm::normalize(glm::cross(m_front, glm::vec3(0.0f, 1.0f, 0.0f)));
        m_up    = glm::normalize(glm::cross(m_right, m_front));

        // update view matrix
        m_viewMatrix = glm::lookAt(m_position, m_position + m_front, m_up);
    }

    void Camera::processKeyboard(float dt) noexcept
    {
        // compute displacement based on currently pressed keys
        glm::vec3 displacement(0.0f);

        if (m_keys['W']) displacement += m_front;
        if (m_keys['S']) displacement -= m_front;
        if (m_keys['A']) displacement -= m_right;
        if (m_keys['D']) displacement += m_right;
        if (m_keys['E']) displacement += m_up;
        if (m_keys['Q']) displacement -= m_up;

        // normalize and scale displacement
        if (glm::length(displacement) > 0.0f)
        {
            displacement = glm::normalize(displacement) * m_speed * dt;
        }

        if (m_cinematicMode)
        {
            // smooth movement
            m_targetPosition += displacement;
            float posLerp = 1.0f - std::exp(-m_posDamping * dt);
            m_position += (m_targetPosition - m_position) * posLerp;
        }
        else
        {
            // instant movement
            m_position += displacement;
            m_targetPosition = m_position;
        }
    }

    void Camera::processMouse(float dt) noexcept
    {
        if (m_cinematicMode)
        {
            // compute difference between target and current rotation
            glm::vec2 delta(m_targetYaw - m_yaw, m_targetPitch - m_pitch);

            // compute damping and acceleration toward the target delta 
            glm::vec2 acceleration = delta * m_rotAcceleration;
            glm::vec2 dampedVelocity = m_rotVelocity * std::exp(-m_rotDamping * dt);
            m_rotVelocity = dampedVelocity + acceleration * dt;

            // update actual rotation
            m_yaw   += m_rotVelocity.x * dt;
            m_pitch += m_rotVelocity.y * dt;

            // clamp pitch as fail-safe
            m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
        }
        else
        {
            // instant rotation
            m_yaw   = m_targetYaw;
            m_pitch = m_targetPitch;
        }
    }
}
