// Engine
#include <camera.h>
#include <input.h>

// GLM
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

Camera::Camera(float vFOV, float nClip, float fClip)
	: m_verticalFOV(vFOV), m_nearClip(nClip), m_farClip(fClip)
{
	m_position = glm::vec3(0, 0, 6);
	m_orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	recalculateView();
}

// Per-frame camera update: handles movement, rotation, and cursor input
// Returns true if the camera changed (position or orientation)
bool Camera::onUpdate(float ts)
{
    // Get current mouse position and compute delta since last frame
    glm::vec2 mousePos = Input::getMousePosition();
    glm::vec2 delta = (mousePos - m_lastMousePosition) * m_sensitivity;
    m_lastMousePosition = mousePos;

    // If right mouse button is not held, release cursor and skip camera control
    if (!Input::isMouseButtonDown(MouseButton::Right))
    {
        Input::setCursorMode(CursorMode::Normal);
        return false;
    }

    // Lock cursor while controlling camera (FPS-style look)
    Input::setCursorMode(CursorMode::Locked);

    bool moved = false;

    // Get camera basis vectors
    glm::vec3 forward = getForwardDirection();
    glm::vec3 right = getRightDirection();
    glm::vec3 up = getUpDirection();

    // Translation (WASD + QE vertical movement)
    if (Input::isKeyDown(KeyCode::W))
    {
        m_position += forward * m_speed * ts;
        moved = true;
    }

    if (Input::isKeyDown(KeyCode::S))
    {
        m_position -= forward * m_speed * ts;
        moved = true;
    }

    if (Input::isKeyDown(KeyCode::A))
    {
        m_position -= right * m_speed * ts;
        moved = true;
    }

    if (Input::isKeyDown(KeyCode::D))
    {
        m_position += right * m_speed * ts;
        moved = true;
    }

    if (Input::isKeyDown(KeyCode::Q))
    {
        m_position -= up * m_speed * ts;
        moved = true;
    }

    if (Input::isKeyDown(KeyCode::E))
    {
        m_position += up * m_speed * ts;
        moved = true;
    }

    // Mouse-based rotation input (pitch/yaw)
    float pitchDelta = delta.y * getRotationSpeed();
    float yawDelta = delta.x * getRotationSpeed();

    // Roll control via keyboard (ZC)
    float rollDelta = 0.0f;

    if (Input::isKeyDown(KeyCode::Z))
        rollDelta += getRotationSpeed() * ts;

    if (Input::isKeyDown(KeyCode::C))
        rollDelta -= getRotationSpeed() * ts;

    // Apply rotation only if there is input
    if (delta.x != 0.0f || delta.y != 0.0f || rollDelta != 0.0f)
    {
        // Recompute right axis (in case orientation changed from movement)
        right = getRightDirection();

        const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

        // Build incremental rotation quaternions
        glm::quat pitchQuat = glm::angleAxis(-pitchDelta, right);
        glm::quat yawQuat = glm::angleAxis(-yawDelta, worldUp);
        glm::quat rollQuat = glm::angleAxis(rollDelta, forward);

        // Apply combined rotation to current orientation
        m_orientation = glm::normalize(yawQuat * pitchQuat * rollQuat * m_orientation);

        moved = true;
    }

    // If anything changed, update derived camera data
    if (moved)
    {
        recalculateView();
        recalculateRayDirections();
    }

    return moved;
}

// Called when the viewport/window is resized.
// Updates internal camera state and rebuilds dependent matrices and ray data.
void Camera::onResize(uint32_t width, uint32_t height)
{
    // Early out if size hasn't actually changed
	if (width == m_viewportWidth && height == m_viewportHeight)
		return;

    // Update stored viewport dimensions
	m_viewportWidth = width;
	m_viewportHeight = height;

    // Recompute projection matrix (depends on aspect ratio)
	recalculateProjection();

    // Recompute per-pixel ray directions (depends on viewport resolution + projection)
	recalculateRayDirections();
}

// Recalculates the camera projection matrix and its inverse
// Used for converting between view space and clip/NDC space
void Camera::recalculateProjection()
{
    // Build perspective projection matrix using vertical FOV and viewport size
	m_projection = glm::perspectiveFov(glm::radians(m_verticalFOV), (float)m_viewportWidth, (float)m_viewportHeight, m_nearClip, m_farClip);

    // Precompute inverse projection for ray generation / unprojection
	m_inverseProjection = glm::inverse(m_projection);
}

// Recalculates the camera view matrix and its inverse
// Called whenever the camera position or orientation changes
void Camera::recalculateView()
{
    // Get camera orientation basis vectors
    glm::vec3 forward = getForwardDirection();
    glm::vec3 up = getUpDirection();

    // Build view matrix using eye position, target point, and up vector
    m_view = glm::lookAt(m_position, m_position + forward, up);

    // Precompute inverse view matrix for ray generation / world transforms
    m_inverseView = glm::inverse(m_view);
}

// Recomputes per-pixel ray directions for the camera
// Used for ray tracing / screen-space ray generation
void Camera::recalculateRayDirections()
{
    // Resize ray buffer to match viewport resolution (1 ray per pixel)
    m_rayDirections.resize(m_viewportWidth * m_viewportHeight);

    // Iterate over each pixel in the viewport
    for (uint32_t y = 0; y < m_viewportHeight; y++)
    {
        for (uint32_t x = 0; x < m_viewportWidth; x++)
        {
            // Convert pixel coordinates to normalized device coordinates (NDC)
            // Range: [0, width/height] -> [0, 1]
            glm::vec2 coord =
            {
                (float)x / (float)m_viewportWidth,
                (float)y / (float)m_viewportHeight
            };

            // Transform NDC [0,1] -> clip space [-1,1]
            coord = coord * 2.0f - 1.0f;

            // Transform from clip space to view space using inverse projection
            glm::vec4 target = m_inverseProjection * glm::vec4(coord.x, coord.y, 1.0f, 1.0f);

            // Perspective divide + transform into world space direction
            glm::vec3 rayDirection = glm::vec3(m_inverseView * glm::vec4(glm::normalize(glm::vec3(target) / target.w), 0.0f));

            // Store final world-space ray direction for this pixel
            m_rayDirections[x + y * m_viewportWidth] = rayDirection;
        }
    }
}