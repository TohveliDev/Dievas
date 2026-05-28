#pragma once

// GLM
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

// C++
#include <vector>

class Camera
{
public:
	Camera(float vFOV, float nClip, float fClip);

	bool onUpdate(float ts);
	void onResize(uint32_t width, uint32_t height);

	const glm::mat4& getProjection() const { return m_projection; }
	const glm::mat4& getInverseProjection() const { return m_inverseProjection; }
	const glm::mat4& getView() const { return m_view; }
	const glm::mat4& getInverseView() const { return m_inverseView; }
	const float getSensitivity() const { return m_sensitivity; }
	const float getSpeed() const { return m_speed; }
	const float getRotSpeed() const { return m_rotationSpeed; }
	const float getFOV() const { return m_verticalFOV; }

	const glm::vec3& getPosition() const { return m_position; }

	const std::vector<glm::vec3>& getRayDirections() const { return m_rayDirections; }

	const float getRotationSpeed() const { return m_rotationSpeed; }
	const glm::vec3 getForwardDirection() const { return glm::rotate(m_orientation, glm::vec3(0.0f, 0.0f, -1.0f)); }
	const glm::vec3 getRightDirection() const { return glm::rotate(m_orientation, glm::vec3(1.0f, 0.0f, 0.0f)); }
	const glm::vec3 getUpDirection() const { return glm::rotate(m_orientation, glm::vec3(0.0f, 1.0f, 0.0f)); }

	void setSpeed(float speed) { m_speed = speed; }
	void setSensitivity(float sens) { m_sensitivity = sens; }
	void setRotationSpeed(float rS) { m_rotationSpeed = rS; }
	void setPosition(glm::vec3 pos) { m_position = pos; }
	void setOrientation(glm::quat orientation) { m_orientation = orientation; }

	void setFOV(float FOV) 
	{ 
		m_verticalFOV = FOV; 
		recalculateProjection();
		recalculateRayDirections();
	}
	
	void recalculateView();

private:
	void recalculateProjection();
	void recalculateRayDirections();
private:
	glm::mat4 m_projection{ 1.0f };
	glm::mat4 m_view{ 1.0f };
	glm::mat4 m_inverseProjection{ 1.0f };
	glm::mat4 m_inverseView{ 1.0f };

	float m_verticalFOV = 45.0f;
	float m_nearClip = 0.1f;
	float m_farClip = 100.f;

	float m_sensitivity = 0.002f;
	float m_speed = 5.0f;
	float m_rotationSpeed = 0.3f;

	glm::vec3 m_position{ 0.0f, 0.0f, 0.0f };
	glm::quat m_orientation;

	std::vector<glm::vec3> m_rayDirections;

	glm::vec2 m_lastMousePosition{ 0.0f, 0.0f };

	uint32_t m_viewportWidth = 0, m_viewportHeight = 0;
};