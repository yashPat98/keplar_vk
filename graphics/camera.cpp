// ────────────────────────────────────────────
//  File: camera.cpp · Created by Yash Patel · 9-7-2025
// ────────────────────────────────────────────

#include "camera.hpp"
#include <algorithm>

namespace 
{
    static inline constexpr float CAMERA_DEFAULT_SPEED       = 10.0f;
    static inline constexpr float CAMERA_DEFAULT_SENSITIVITY = 0.1f;
    static inline constexpr float CAMERA_DEFAULT_SCROLLSPEED = 2.0f;
    static inline constexpr float MAX_MOUSE_MOVE_THRESHOLD   = 50.0f;
    static inline constexpr float CAMERA_DEFAULT_DAMPING     = 100.0f;
    static inline constexpr float CAMERA_CINEMATIC_DAMPING   = 5.0f;
    static inline constexpr float CAMERA_TURNTABLE_DAMPING   = 6.0f;

    static inline constexpr float ORBIT_PITCH_LIMIT          = glm::radians(89.0f);
    static inline constexpr float ARCBALL_ZOOM_FACTOR        = 0.12f;
}

namespace keplar
{
    Camera::Camera(Mode mode, float fovy, float aspect, float znear, float zfar) noexcept
        : m_mode(mode)
        , m_fovy(fovy)
        , m_aspect(aspect)
        , m_znear(znear)
        , m_zfar(zfar)
        , m_width(1.0f)
        , m_height(1.0f)
        , m_position(0.0f, 0.0f, 3.0f)
        , m_front(0.0f, 0.0f, -1.0f)
        , m_up(0.0f, 1.0f, 0.0f)
        , m_right(1.0f, 0.0f, 0.0f)
        , m_orientation(1.0f, 0.0f, 0.0f, 0.0f)
        , m_targetOrientation(m_orientation)
        , m_sensitivity(CAMERA_DEFAULT_SENSITIVITY)
        , m_scrollSpeed(CAMERA_DEFAULT_SCROLLSPEED)
        , m_targetPosition(m_position)
        , m_speed(CAMERA_DEFAULT_SPEED)
        , m_keys{}
        , m_viewMatrix(1.0f)
        , m_lastMouseX(0.0)
        , m_lastMouseY(0.0)
        , m_firstMouse(true)
        , m_dragging(false)
        , m_dragStartX(0.0)
        , m_dragStartY(0.0)
        , m_dragStartYaw(0.0f)
        , m_dragStartPitch(0.0f)
    {
        switch (m_mode)
        {
            case Mode::Fps:
                {
                    m_posDamping = CAMERA_DEFAULT_DAMPING;
                    m_rotDamping = CAMERA_DEFAULT_DAMPING;
                }
                break;

            case Mode::Cinematic:
                {
                    m_posDamping = CAMERA_CINEMATIC_DAMPING;
                    m_rotDamping = CAMERA_CINEMATIC_DAMPING;
                }
                break;

            case Mode::Turntable:
                {
                    m_posDamping = CAMERA_TURNTABLE_DAMPING;
                    m_rotDamping = CAMERA_TURNTABLE_DAMPING;
                    initOrbitState();
                }   
                break;
        }

        updateProjection();
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
        if (width == 0 || height == 0)
            return;

        // update aspect ratio and projection matrix 
        m_width = float(width);
        m_height = float(height);
        m_aspect = m_width / m_height;
        updateProjection();
    
        // reset first-mouse to avoid jump after resize
        m_firstMouse = true;
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
        switch (m_mode)
        {
            case Mode::Fps:
            case Mode::Cinematic:
                {
                    // skip first event to prevent sudden jump
                    if (m_firstMouse)
                    {
                        m_lastMouseX = xpos;
                        m_lastMouseY = ypos;
                        m_firstMouse = false;
                        return;
                    }

                    // compute mouse movement delta
                    double dx = xpos - m_lastMouseX;
                    double dy = ypos - m_lastMouseY; 

                    // store current mouse position
                    m_lastMouseX = xpos;
                    m_lastMouseY = ypos;

                    // discard sudden large movement
                    if (std::abs(dx) > MAX_MOUSE_MOVE_THRESHOLD || std::abs(dy) > MAX_MOUSE_MOVE_THRESHOLD)
                    {
                        return;
                    }

                    // update yaw and pitch based on mouse movement and sensitivity
                    glm::quat yaw = glm::angleAxis(glm::radians(static_cast<float>(-dx) * m_sensitivity), glm::vec3(0.0f, 1.0f, 0.0f));    
                    glm::quat pitch = glm::angleAxis(glm::radians(static_cast<float>(dy) * m_sensitivity), glm::vec3(1.0f, 0.0f, 0.0f)); 
                    
                    // apply pitch then yaw to target orientation
                    m_targetOrientation = glm::normalize(yaw * m_targetOrientation * pitch);
                }
                break;

            case Mode::Turntable:
                {
                    if (!m_dragging)
                    {
                        m_lastMouseX = xpos;
                        m_lastMouseY = ypos;
                        break;
                    }

                    // compute mouse movement delta
                    double dx = xpos - m_lastMouseX;
                    double dy = ypos - m_lastMouseY; 

                    // store current mouse position
                    m_lastMouseX = xpos;
                    m_lastMouseY = ypos;

                    // discard sudden large movement
                    if (std::abs(dx) > MAX_MOUSE_MOVE_THRESHOLD || std::abs(dy) > MAX_MOUSE_MOVE_THRESHOLD)
                    {
                        // re-anchor drag to avoid snap
                        m_dragStartX = xpos;
                        m_dragStartY = ypos;
                        m_dragStartYaw = m_orbitTargetYaw;
                        m_dragStartPitch = m_orbitTargetPitch;
                        break;
                    }

                    // mouse delta from drag anchor
                    dx = xpos - m_dragStartX;
                    dy = ypos - m_dragStartY;

                    // mouse delta to orbit yaw/pitch
                    float yawDelta   = glm::radians(float(-dx) * m_sensitivity);
                    float pitchDelta = glm::radians(float(-dy) * m_sensitivity);

                    // build target orientation from orbit angles
                    m_orbitTargetYaw = std::remainder(m_dragStartYaw + yawDelta, glm::two_pi<float>());
                    m_orbitTargetPitch = glm::clamp(m_dragStartPitch + pitchDelta, -ORBIT_PITCH_LIMIT, ORBIT_PITCH_LIMIT);
                    m_targetOrientation = buildOrbitOrientation(m_orbitTargetYaw, m_orbitTargetPitch);
                }
                break;
        }
    }

    void Camera::onMouseScroll(double yoffset) 
    {
        if (m_mode == Mode::Turntable)
        {
            // exponential scroll zoom
            float factor = std::exp(-static_cast<float>(yoffset) * ARCBALL_ZOOM_FACTOR * m_scrollSpeed);
            m_orbitTargetDistance = std::clamp(m_orbitTargetDistance * factor, 0.05f, 500.0f);
        }
        else 
        {
            // adjust field-of-view with scroll and update projection
            m_fovy -= static_cast<float>(yoffset) * m_scrollSpeed;
            m_fovy = std::clamp(m_fovy, 1.0f, 120.0f);
            updateProjection();
        }
    }

    void Camera::onMouseButtonPressed(uint32_t button, int xpos, int ypos) 
    {
        if (m_mode == Mode::Turntable && button == 0)
        {
            m_dragging = true;
            m_firstMouse = false;
            
            // anchor drag position (screen space)
            m_dragStartX = double(xpos);
            m_dragStartY = double(ypos);
            m_dragStartYaw = m_orbitTargetYaw;     
            m_dragStartPitch = m_orbitTargetPitch;

            // store current mouse position
            m_lastMouseX = double(xpos);
            m_lastMouseY = double(ypos);
        }
    }

    void Camera::onMouseButtonReleased(uint32_t button, int, int) 
    {
        if (m_mode == Mode::Turntable && button == 0)
        {
            m_dragging = false;
        }
    }

    void Camera::setFov(float fovy) noexcept
    {
        m_fovy = fovy;
        updateProjection();
    }
    
    void Camera::setClipPlanes(float znear, float zfar) noexcept
    {
        m_znear = znear;
        m_zfar = zfar;
        updateProjection();
    }

    void Camera::setAspectRatio(float aspect) noexcept
    {
        m_aspect = aspect;
        updateProjection();
    }

    void Camera::setSpeed(float speed) noexcept
    {
        m_speed = speed;
    }

    void Camera::setSensitivity(float sensitivity) noexcept
    {
        m_sensitivity = std::max(0.001f, sensitivity);
    }

    void Camera::setScrollSpeed(float speed) noexcept
    {
        m_scrollSpeed = speed;
    }

    void Camera::setPositionDamping(float damping) noexcept
    {
        m_posDamping = damping;
    }

    void Camera::setRotationDamping(float damping) noexcept
    {
        m_rotDamping = damping;
    }

    void Camera::setOrbitTarget(const glm::vec3& target) noexcept 
    { 
        m_orbitTarget = target; 
    }

    void Camera::updateProjection() noexcept
    {
        // build perspective projection and flip Y axis for vulkan clip space
        m_projectionMatrix = glm::perspective(glm::radians(m_fovy), m_aspect, m_znear, m_zfar);
        m_projectionMatrix[1][1] *= -1.0f;
    }

    void Camera::updateVectors() noexcept
    {
        // camera basis from orbit orientation
        m_front = glm::normalize(m_orientation * glm::vec3(0.0f, 0.0f, -1.0f));
        m_right = glm::normalize(m_orientation * glm::vec3(1.0f, 0.0f, 0.0f));
        m_up    = glm::normalize(m_orientation * glm::vec3(0.0f, 1.0f, 0.0f));

        switch (m_mode)
        {
            case Mode::Fps:
            case Mode::Cinematic:
                {
                    // update view matrix
                    m_viewMatrix = glm::lookAt(m_position, m_position + m_front, m_up);
                }
                break;

            case Mode::Turntable:
                {
                    // orbit position around target
                    glm::vec3 offset = m_orientation * glm::vec3(0.0f, 0.0f, m_orbitDistance);
                    m_position = m_orbitTarget + offset;

                    // update view matrix
                    m_viewMatrix = glm::lookAt(m_position, m_orbitTarget, m_up);
                }
                break;
        }
    }

    void Camera::processKeyboard(float dt) noexcept
    {
        if (m_mode == Mode::Turntable)
        {
            return;
        }

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

        if (m_mode == Mode::Cinematic)
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
        switch (m_mode)
        {
            case Mode::Fps: 
                {
                    // instant rotation
                    m_orientation = m_targetOrientation;
                }
                break;

            case Mode::Cinematic:
                {
                    // smoothly interpolate from current orientation to target orientation
                    // using exponential damping factor for smooth acceleration/deceleration

                    // convert damping parameters to interpolation alpha
                    float alpha = 1.0f - std::exp(-m_rotDamping * dt);
                    m_orientation = glm::slerp(m_orientation, m_targetOrientation, alpha);

                    // normalize to prevent drift
                    m_orientation = glm::normalize(m_orientation);
                }
                break;

            case Mode::Turntable:
                {
                    // exponential smoothing factors
                    const float rotAlpha  = 1.0f - std::exp(-m_rotDamping * dt);
                    const float zoomAlpha = 1.0f - std::exp(-m_posDamping * dt);

                    // orbit yaw 
                    const float yawErr = std::remainder(m_orbitTargetYaw - m_orbitYaw, glm::two_pi<float>());
                    m_orbitYaw = std::remainder(m_orbitYaw + yawErr * rotAlpha, glm::two_pi<float>());

                    // orbit pitch
                    m_orbitPitch += (m_orbitTargetPitch - m_orbitPitch) * rotAlpha;
                    m_orbitPitch = glm::clamp(m_orbitPitch, -ORBIT_PITCH_LIMIT, ORBIT_PITCH_LIMIT);

                    // orientation from damped angles
                    m_orientation = buildOrbitOrientation(m_orbitYaw, m_orbitPitch);

                    // orbit distance
                    m_orbitDistance += (m_orbitTargetDistance - m_orbitDistance) * zoomAlpha;
                    m_orbitDistance = std::clamp(m_orbitDistance, 0.05f, 500.0f);
                }
                break;
        }
    }

    void Camera::initOrbitState() noexcept
    {
        // orbit pivot (origin)
        m_orbitTarget = glm::vec3(0.0f);
        m_orbitDistance = glm::length(m_position - m_orbitTarget);
        m_orbitTargetDistance = m_orbitDistance;

        // derive yaw/pitch from current forward 
        glm::vec3 forward = glm::normalize(m_orbitTarget - m_position);

        // yaw around world up
        m_orbitYaw = std::atan2(forward.x, -forward.z);

        // pitch from vertical component
        m_orbitPitch = std::asin(glm::clamp(forward.y, -1.0f, 1.0f));
        m_orbitPitch = glm::clamp(m_orbitPitch, -ORBIT_PITCH_LIMIT, ORBIT_PITCH_LIMIT);

        // initialize targets to avoid snapping
        m_orbitTargetYaw = m_orbitYaw;
        m_orbitTargetPitch = m_orbitPitch;

         // build orbit orientation
        m_orientation = buildOrbitOrientation(m_orbitYaw, m_orbitPitch);
        m_targetOrientation = m_orientation;
    }

    glm::quat Camera::buildOrbitOrientation(float yaw, float pitch) const noexcept
    {
        // yaw about world up
        glm::quat qYaw = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));

        // pitch about camera-right after yaw (turntable)
        glm::vec3 right = qYaw * glm::vec3(1.0f, 0.0f, 0.0f);
        glm::quat qPitch = glm::angleAxis(pitch, right);

        // apply yaw then pitch; roll-free
        return glm::normalize(qPitch * qYaw);
    }

    glm::quat Camera::safeRotationBetweenVectors(const glm::vec3& from, const glm::vec3& to) const noexcept
    {
        const glm::vec3 fromDir = glm::normalize(from);
        const glm::vec3 toDir   = glm::normalize(to);

        // clamped dot product of two vectors
        const float cosTheta = glm::clamp(glm::dot(fromDir, toDir), -1.0f, 1.0f);

        // nearly identical: no rotation
        if (cosTheta > 0.999999f)
        {
            return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }

        // nearly opposite: pick a stable orthogonal axis
        if (cosTheta < -0.999999f)
        {
            glm::vec3 axis = glm::cross(fromDir, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::length2(axis) < 1e-8f)
            {
                axis = glm::cross(fromDir, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            axis = glm::normalize(axis);
            return glm::angleAxis(glm::pi<float>(), axis);
        }

        // general case: quaternion from two unit vectors
        const glm::vec3 axis = glm::cross(fromDir, toDir);
        const float scale = std::sqrt((1.0f + cosTheta) * 2.0f);
        const float invScale = 1.0f / scale;

        return glm::normalize(glm::quat(0.5f * scale, axis.x * invScale, axis.y * invScale, axis.z * invScale));
    }
}
