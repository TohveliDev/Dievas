// Engine
#include <entrypoint.h>
#include <application.h>
#include <image.h>
#include <random.h>
#include <timer.h>
#include <renderer.h>
#include <camera.h>

// GLM
#include <glm/gtc/type_ptr.hpp>

// C++
#include <algorithm>
#include <memory>
#include <Windows.h>
#include <commdlg.h>

// Global flags for Menu Callback
bool g_propertiesOpened = false;
bool g_materialCreatorOpened = false;
bool g_aboutOpened = false;
bool g_controlsOpened = false;
bool g_qrOpened = false;
bool g_cheatSheetOpened = false;
bool g_renderOpened = false;
bool g_importing = false;
bool g_resetView = false;
bool g_skyboxOpen = false;

namespace Helper
{
	// Converts a hex code to a ImVec4
	ImVec4 toRGBA(uint32_t argb)
	{
		ImVec4 color{};
		color.x = ((argb >> 16) & 0xFF) / 255.0f;
		color.y = ((argb >> 8) & 0xFF) / 255.0f;
		color.z = (argb & 0xFF) / 255.0f;
		color.w = ((argb >> 24) & 0xFF) / 255.0f;
		return color;
	}
};


class TestLayer : public Layer
{
public:
	TestLayer() : m_camera(45.f, 0.1f, 100.f)
	{
		// ImGui Style
		{
			ImGuiStyle& style = ImGui::GetStyle();
			ImVec4* colors = style.Colors;

			// Base Colours
			const ImVec4 bg0 = Helper::toRGBA(0xff18181b);
			const ImVec4 bg1 = Helper::toRGBA(0xff202024);
			const ImVec4 surface0 = Helper::toRGBA(0xff2a2d33);
			const ImVec4 surface1 = Helper::toRGBA(0xff353540);
			const ImVec4 surface2 = Helper::toRGBA(0xff40404d);
			const ImVec4 border = Helper::toRGBA(0xff4a4a57);
			
			// Text
			const ImVec4 text = Helper::toRGBA(0xffe4e4e7);
			const ImVec4 subtext = Helper::toRGBA(0xffa1a1aa);
			const ImVec4 textMuted = Helper::toRGBA(0xff71717a);

			// Blues
			const ImVec4 blue = Helper::toRGBA(0xff1e69cb);
			const ImVec4 blueHover = Helper::toRGBA(0xff2d78db);
			const ImVec4 blueActive = Helper::toRGBA(0xff438ef2);
			const ImVec4 blueSoft = Helper::toRGBA(0xff74b0ff);

			// Utility
			const ImVec4 warning = Helper::toRGBA(0xfffacc15);
			const ImVec4 success = Helper::toRGBA(0xff4ade80);
			const ImVec4 transparent = ImVec4(0, 0, 0, 0);

			colors[ImGuiCol_Text] = text;
			colors[ImGuiCol_TextDisabled] = textMuted;
			colors[ImGuiCol_WindowBg] = bg0;
			colors[ImGuiCol_ChildBg] = bg0;
			colors[ImGuiCol_PopupBg] = bg1;
			colors[ImGuiCol_Border] = border;
			colors[ImGuiCol_BorderShadow] = transparent;
			colors[ImGuiCol_FrameBg] = surface0;
			colors[ImGuiCol_FrameBgHovered] = blueHover;
			colors[ImGuiCol_FrameBgActive] = surface2;
			colors[ImGuiCol_TitleBg] = bg1;
			colors[ImGuiCol_TitleBgActive] = surface0;
			colors[ImGuiCol_TitleBgCollapsed] = bg1;
			colors[ImGuiCol_MenuBarBg] = bg1;
			colors[ImGuiCol_ScrollbarBg] = bg1;
			colors[ImGuiCol_ScrollbarGrab] = surface1;
			colors[ImGuiCol_ScrollbarGrabHovered] = blue;
			colors[ImGuiCol_ScrollbarGrabActive] = blueHover;
			colors[ImGuiCol_CheckMark] = blueHover;
			colors[ImGuiCol_SliderGrab] = blue;
			colors[ImGuiCol_SliderGrabActive] = blueHover;
			colors[ImGuiCol_Button] = blue;
			colors[ImGuiCol_ButtonHovered] = blueHover;
			colors[ImGuiCol_ButtonActive] = blueActive;
			colors[ImGuiCol_Header] = surface0;
			colors[ImGuiCol_HeaderHovered] = blueHover;
			colors[ImGuiCol_HeaderActive] = blueActive;
			colors[ImGuiCol_Separator] = border;
			colors[ImGuiCol_SeparatorHovered] = blue;
			colors[ImGuiCol_SeparatorActive] = blueHover;
			colors[ImGuiCol_ResizeGrip] = surface1;
			colors[ImGuiCol_ResizeGripHovered] = blue;
			colors[ImGuiCol_ResizeGripActive] = blueHover;
			colors[ImGuiCol_Tab] = blue;
			colors[ImGuiCol_TabHovered] = blueHover;
			colors[ImGuiCol_TabActive] = blueActive;
			colors[ImGuiCol_TabUnfocused] = surface1;
			colors[ImGuiCol_TabUnfocusedActive] = surface0;
			colors[ImGuiCol_DockingPreview] = border;
			colors[ImGuiCol_DockingEmptyBg] = bg0;
			colors[ImGuiCol_PlotLines] = surface0;
			colors[ImGuiCol_PlotLinesHovered] = blueHover;
			colors[ImGuiCol_PlotHistogram] = blue;
			colors[ImGuiCol_PlotHistogramHovered] = success;
			colors[ImGuiCol_TableHeaderBg] = surface0;
			colors[ImGuiCol_TableBorderStrong] = border;
			colors[ImGuiCol_TableBorderLight] = surface0;
			colors[ImGuiCol_TableRowBg] = transparent;
			colors[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.03f);
			colors[ImGuiCol_TextSelectedBg] = ImVec4(blue.x, blue.y, blue.z, 0.35f);
			colors[ImGuiCol_DragDropTarget] = warning;
			colors[ImGuiCol_NavHighlight] = blueSoft;
			colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.70f);
			colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.20f);
			colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.35f);
			colors[ImGuiCol_TextSelectedBg] = ImVec4(blueHover.x, blueHover.y, blueHover.z, 0.25f);
			colors[ImGuiCol_FrameBgActive] = ImVec4(blueActive.x * 0.35f, blueActive.y * 0.35f, blueActive.z * 0.35f, 1.0f);

			style.WindowRounding = 6.0f;
			style.ChildRounding = 6.0f;
			style.FrameRounding = 4.0f;
			style.PopupRounding = 4.0f;
			style.ScrollbarRounding = 9.0f;
			style.GrabRounding = 4.0f;
			style.TabRounding = 4.0f;
			style.WindowPadding = ImVec2(8.0f, 8.0f);
			style.FramePadding = ImVec2(5.0f, 3.0f);
			style.ItemSpacing = ImVec2(8.0f, 4.0f);
			style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
			style.IndentSpacing = 21.0f;
			style.ScrollbarSize = 14.0f;
			style.GrabMinSize = 10.0f;
			style.WindowBorderSize = 1.0f;
			style.ChildBorderSize = 1.0f;
			style.PopupBorderSize = 1.0f;
			style.FrameBorderSize = 0.0f;
			style.TabBorderSize = 0.0f;
		}

		// Base Material
		{
			Material& baseMaterial = m_scene.materials.emplace_back();
			baseMaterial.name = "Base Material";
			baseMaterial.albedo = { 1.0f, 1.0f, 1.0f};
		}
	}

	// Per-frame update for the layer/application
	// Advances camera and triggers renderer reset if view changed
	virtual void onUpdate(float ts) override
	{
		// Update camera (movement, rotation, input handling)
		// Returns true if camera state changed this frame
		if (m_camera.onUpdate(ts))
		{
			// Camera moved or rotated -> accumulated rendering becomes invalid
			// Reset frame index to restart progressive rendering
			m_renderer.resetFrameIndex();
		}
	}

	// Renders all the UI elements
	virtual void onUIRender() override
	{
		drawViewport();
		drawSideBar();
		drawBottomBar();

		if (g_propertiesOpened)
		{
			propertiesMenu();
			ImGui::OpenPopup("Properties");
		}

		if (g_materialCreatorOpened)
		{
			materialCreator();
			ImGui::OpenPopup("Material Creator");
		}

		if (g_aboutOpened)
		{
			drawAbout();
			ImGui::OpenPopup("About");
		}

		if (g_qrOpened)
		{
			drawQR();
			ImGui::OpenPopup("Quick Reference");
		}

		if (g_cheatSheetOpened)
		{
			drawCS();
			ImGui::OpenPopup("IoR Cheat Sheet");
		}

		if (g_renderOpened)
		{
			drawRenderPrompt();
			ImGui::OpenPopup("Render Frame");
		}

		if (g_importing)
		{
			importModel();
		}

		if (g_resetView)
		{
			resetView();
			g_resetView = false;
		}

		if (g_skyboxOpen)
		{
			skyboxPropertiesMenu();
			ImGui::OpenPopup("Skybox Properties");
		}

		if (m_shouldRender)
		{
			onRender();
		}

	}
	
	// Resets camera and renderer to the original state(s)
	void resetView()
	{
		m_renderer.setBounces(5);
		m_renderer.setClearCol(glm::vec3(0.17f, 0.17f, 0.17f));
		m_camera.setFOV(45.f);
		m_camera.setSpeed(5.f);
		m_camera.setSensitivity(0.002f);
		m_camera.setRotationSpeed(0.3f);
		m_camera.setPosition(glm::vec3(0, 0, 6));
		m_camera.setOrientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
		m_camera.recalculateView();
		m_renderer.setZenithCol(glm::vec3(0.1f, 0.3f, 0.8f));
		m_renderer.setHorizonCol(glm::vec3(0.9f, 0.6f, 0.8f));
		m_renderer.setGlowCol(glm::vec3(1.0f, 0.9f, 0.6f));
		m_renderer.setSunDirection(glm::vec3(0.3f, 0.7f, 0.2f));
		m_renderer.setSunIntensity(8.0f);
		m_renderer.setSkyIntensity(1.0f);
		m_renderer.setHorizonIntensity(0.15f);
		m_renderer.setGlowIntensity(0.4f);
		m_renderer.setSunDiscSize(1024.0f);
		m_renderer.setGlowSize(32.0f);
		m_renderer.setHorizonFalloff(8.0f);
		m_renderer.getSettings().skybox = false;
		m_renderer.getSettings().accumulate = false;
		m_renderer.resetFrameIndex();
	}

	// Opens a Windows file dialog to import an OBJ model into the scene.
	// If a file is selected, it is added to the scene and the renderer is updated.
	void importModel()
	{
		char filename[MAX_PATH] = "";

		// Configure Windows "Open File" dialog
		OPENFILENAMEA ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFilter = "OBJ Files\0*.obj\0";
		ofn.lpstrFile = filename;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

		// Show dialog and check if user selected a file
		if (GetOpenFileNameA(&ofn))
		{
			// Add model to scene with identity transform
			m_scene.addModelToScene(filename, 0, glm::mat4(1.0f));

			// Apply small uniform scale to prevent huge objects from crashing the application
			m_scene.objects[m_scene.objects.size() - 1].scale = glm::vec3(0.001f);

			// Recompute transformation matrices after scaling
			m_scene.objects[m_scene.objects.size() - 1].recalculateMatrixes();

			// Assign a default generated name
			std::string objectName = "Object " + std::to_string(m_scene.objects.size() - 1);
			m_scene.objects[m_scene.objects.size() - 1].name = objectName;

			// Rebuild/update renderer scene data
			m_renderer.recalculateScene(m_scene);

			// Exit import mode/state
			g_importing = false;
		}

		else
		{
			// User cancelled file selection
			g_importing = false;
		}
	}

	// Opens a Modal Popup that lets the user to screenshot the viewport image
	void drawRenderPrompt()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		char fileName[64] = "Dievas Output Image";
		auto viewportImage = m_renderer.getImage();

		if (ImGui::BeginPopupModal("Render Frame", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("NOTE: For the best result, you should ENABLE accumulation");
			ImGui::Text("(Edit -> Properties -> Enable Accumulation)");
			ImGui::Text("");
			ImGui::Text("And you should let the image accumulate +200 times, before rendering.");
			ImGui::Separator();
			ImGui::InputText("Output Image Name", fileName, IM_ARRAYSIZE(fileName));
			
			ImGui::Separator();

			if (ImGui::Button("Render"))
			{
				g_renderOpened = false;
				ImGui::CloseCurrentPopup();
				std::string imageName = std::string(fileName) + ".png";
				viewportImage->savePNG(imageName);
			}

			ImGui::SameLine();

			if (ImGui::Button("Exit"))
			{
				g_renderOpened = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	// Opens a modal popup with various common IoR values
	void drawCS()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("IoR Cheat Sheet", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Common Materials:");
			ImGui::Text("Air - 1.0");
			ImGui::Text("Ice - 1.31");
			ImGui::Text("Water - 1.33");
			ImGui::Text("Quartz - 1.46");
			ImGui::Text("Acrylic - 1.49");
			ImGui::Text("Glass - 1.52");
			ImGui::Text("Flint Glass - 1.62");
			ImGui::Separator();
			ImGui::Text("Gemstones:");
			ImGui::Text("Amethyst - 1.54");
			ImGui::Text("Opal - 1.45");
			ImGui::Text("Emerald - 1.57");
			ImGui::Text("Topaz - 1.61");
			ImGui::Text("Ruby - 1.77");
			ImGui::Text("Diamond - 2.42");
			ImGui::Separator();

			if (ImGui::Button("Exit"))
			{
				g_cheatSheetOpened = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	// Opens a modal popup that can be used to create a new material
	void materialCreator()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Material Creator", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			static char matNameBuf[128] = "";

			if (matNameBuf[0] == '\0')
			{
				int index = static_cast<int>(m_scene.materials.size());

				std::snprintf(
					matNameBuf,
					sizeof(matNameBuf),
					"Material %d",
					index
				);
			}

			static float matAlbedo[3] = {0.f, 0.f, 0.f};
			static float matRough = 0.0f;
			static float matMetal = 0.0f;
			static float matEmi = 0.0f;
			static float matEmCol[3] = {0.f, 0.f, 0.f};
			static float matTrans = 0.0f;
			static float matIOR = 1.0f;
			static float matAbs[3] = { 0.0f, 0.0f, 0.0f };
			static float ani = 0.0f;
			static float aniRot = 0.0f;

			ImGui::InputText("Material Name", matNameBuf, IM_ARRAYSIZE(matNameBuf));
			ImGui::Separator();
			ImGui::ColorEdit3("Material Albedo", matAlbedo);
			ImGui::DragFloat("Material Roughness", &matRough, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Material Metallic", &matMetal, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Material Anisotrophy", &ani, 0.01f, -1.0f, 1.0f);

			ImGui::SliderFloat("Material Anistrophy Rotation", &aniRot, 0.0f, 360.f, "%1.f deg");
			ImGui::Separator();
			ImGui::DragFloat("Material Emission Power", &matEmi, 0.01f, 0.0f, FLT_MAX);
			ImGui::ColorEdit3("Material Emission Color", matEmCol);
			ImGui::Separator();
			ImGui::DragFloat("Material Transmission", &matTrans, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat3("Material Absorption", matAbs, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("Material IoR", &matIOR, 0.01f, 1.0f, 2.5f);
			ImGui::Separator();

			if (ImGui::Button("Create Material"))
			{
				Material& tempMat = m_scene.materials.emplace_back();
				tempMat.name = matNameBuf;
				tempMat.albedo = glm::vec3(matAlbedo[0], matAlbedo[1], matAlbedo[2]);
				tempMat.roughness = matRough;
				tempMat.metallic = matMetal;
				tempMat.emissionPower = matEmi;
				tempMat.emissionColor = glm::vec3(matEmCol[0], matEmCol[1], matEmCol[2]);
				tempMat.transmission = matTrans;
				tempMat.ior = matIOR;
				tempMat.absorption = glm::vec3(matAbs[0], matAbs[1], matAbs[2]);
				tempMat.anisotropy = ani;
				tempMat.anisotropyRotation = glm::radians(aniRot);

				g_materialCreatorOpened = false;

				strcpy(matNameBuf, "New Material");

				matAlbedo[0] = matAlbedo[1] = matAlbedo[2] = 0.0f;
				matRough = 0.0f;
				matMetal = 0.0f;
				matEmi = 0.0f;
				matEmCol[0] = matEmCol[1] = matEmCol[2] = 0.0f;
				matTrans = 0.0f;
				matIOR = 1.0f;
				matAbs[0] = matAbs[1] = matAbs[2] = 0.0f;
				ani = 0.0f;
				aniRot = 0.0f;

				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			
			if (ImGui::Button("Exit"))
			{
				g_materialCreatorOpened = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	// Opens a modal popup that can be used to edit the Skybox variables
	void skyboxPropertiesMenu()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Skybox Properties", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			glm::vec3 zenithCol = m_renderer.getZenithColor();
			glm::vec3 horizonCol = m_renderer.getHorizonColor();
			glm::vec3 glowCol = m_renderer.getGlowColor();
			glm::vec3 sunDir = m_renderer.getSunDirection();

			float zenith[3] = { zenithCol.r, zenithCol.g, zenithCol.b };
			float horizon[3] = { horizonCol.r, horizonCol.g, horizonCol.b };
			float glow[3] = { glowCol.r, glowCol.g, glowCol.b };
			float sun[3] = { sunDir.x, sunDir.y, sunDir.z };
			float sunIntensity = m_renderer.getSunIntensity();
			float skyIntensity = m_renderer.getSkyIntensity();
			float glowIntensity = m_renderer.getGlowIntensity();
			float horizonIntensity = m_renderer.getHorizonIntensity();
			float sunDiscSize = m_renderer.getSunDiscSize();
			float sunGlowSize = m_renderer.getSunGlowSize();
			float horizonFalloff = m_renderer.getHorizonFalloff();

			ImGui::Text("Sun Properties");

			if (ImGui::DragFloat3("Sun Direction", sun, 0.1f, -1.0f, 1.0f)) 
			{ 
				m_renderer.setSunDirection(glm::vec3(sun[0], sun[1], sun[2]));
				m_renderer.resetFrameIndex();
			}

			if (ImGui::DragFloat("Sun Disc Size", &sunDiscSize, 0.1f, 1.0f, 10000.0f, "%.1f", ImGuiSliderFlags_Logarithmic))
			{
				m_renderer.setSunDiscSize(sunDiscSize);
				m_renderer.resetFrameIndex();
			}

			if (ImGui::ColorEdit3("Sun Glow Color", glow))
			{
				m_renderer.setGlowCol(glm::vec3(glow[0], glow[1], glow[2]));
				m_renderer.resetFrameIndex();
			}

			if (ImGui::DragFloat("Sun Intensity", &sunIntensity, 0.1f, 0.0f, 100.f, "%.1f", ImGuiSliderFlags_Logarithmic))
			{ 
				m_renderer.setSunIntensity(sunIntensity); 
				m_renderer.resetFrameIndex();
			}

			if (ImGui::DragFloat("Sun Glow Intensity", &glowIntensity, 0.1f, 0.0f, 1.0f))
			{
				m_renderer.setGlowIntensity(glowIntensity);
				m_renderer.resetFrameIndex();
			}

			if (ImGui::DragFloat("Sun Glow Size", &sunGlowSize, 0.1f, 1.0f, 256.f, "%.1f", ImGuiSliderFlags_Logarithmic))
			{
				m_renderer.setGlowSize(sunGlowSize);
				m_renderer.resetFrameIndex();
			}

			ImGui::Separator();

			ImGui::Text("Horizon Properties");

			if (ImGui::ColorEdit3("Zenith Color", zenith))
			{
				m_renderer.setZenithCol(glm::vec3(zenithCol[0], zenith[1], zenith[2]));
				m_renderer.resetFrameIndex();
			}

			if (ImGui::ColorEdit3("Horizon Color", horizon))
			{
				m_renderer.setHorizonCol(glm::vec3(horizon[0], horizon[1], horizon[2]));
				m_renderer.resetFrameIndex();
			}

			if (ImGui::DragFloat("Sky Intensity", &skyIntensity, 0.1f, 0.0f, 10.f))
			{
				m_renderer.setSkyIntensity(skyIntensity);
				m_renderer.resetFrameIndex();
			}

			if (ImGui::DragFloat("Horizon Intensity", &horizonIntensity, 0.1f, 0.0f, 5.f))
			{
				m_renderer.setHorizonIntensity(horizonIntensity);
				m_renderer.resetFrameIndex();
			}

			if (ImGui::DragFloat("Horizon Falloff", &horizonFalloff, 0.1f, 0.1f, 32.f))
			{
				m_renderer.setHorizonFalloff(horizonFalloff);
				m_renderer.resetFrameIndex();
			}

			ImGui::Separator();

			if (ImGui::Button("Close"))
			{
				g_skyboxOpen = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	// Opens a modal popup that allows the user to edit the application parameters
	void propertiesMenu()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Properties", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			glm::vec3 currentCol = m_renderer.getClearCol();
			int bounces = m_renderer.getBounces();
			float sensitivity = m_camera.getSensitivity();
			float speed = m_camera.getSpeed();
			float rotSpeed = m_camera.getRotationSpeed();
			float clearCol[3] = { currentCol.r, currentCol.g, currentCol.b };
			float FOV = m_camera.getFOV();

			ImGui::Text("Scene Properties");
			if (ImGui::ColorEdit3("Clear Color", clearCol))
			{
				m_renderer.setClearCol(glm::vec3(clearCol[0], clearCol[1], clearCol[2]));
				m_renderer.resetFrameIndex();
			}

			if (ImGui::DragInt("Ray Bounces", &bounces, 1, 1, 100))
			{
				m_renderer.setBounces(bounces);
				m_renderer.resetFrameIndex();
			}

			ImGui::Checkbox("Enable Accumulation", &m_renderer.getSettings().accumulate);
			if (ImGui::Checkbox("Enable Skybox", &m_renderer.getSettings().skybox)) { m_renderer.resetFrameIndex(); }

			ImGui::Separator();
			ImGui::Text("Camera Properties");

			if (ImGui::DragFloat("Camera Sensitivity", &sensitivity, 0.001f, 0.001f, 1.f))
			{
				m_camera.setSensitivity(sensitivity);
			}

			if (ImGui::DragFloat("Camera Speed", &speed, 0.1f, 0.1f, FLT_MAX))
			{
				m_camera.setSpeed(speed);
			}

			if (ImGui::DragFloat("Camera Rotation Speed", &rotSpeed, 0.01f, 0.01f, FLT_MAX))
			{
				m_camera.setRotationSpeed(rotSpeed);
			}

			if (ImGui::DragFloat("Camera Vertical FOV", &FOV, 1.0f, 45.f, 120.f))
			{
				m_camera.setFOV(FOV);
				m_renderer.resetFrameIndex();
			}

			ImGui::Separator();

			if (ImGui::Button("Close"))
			{
				g_propertiesOpened = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	// Renders the main viewport window using ImGui and displays the renderer output texture.
	// Also updates viewport dimensions based on available UI space.
	void drawViewport()
	{
		// Remove padding so the image fills the entire viewport area
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

		// Create ImGui window for the viewport
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;

		ImGui::Begin("Viewport", nullptr, flags);

		// Restore previous style state
		ImGui::PopStyleVar();

		// Update viewport size from available ImGui region
		m_viewportWidth = ImGui::GetContentRegionAvail().x;
		m_viewportHeight = ImGui::GetContentRegionAvail().y;

		// Get rendered image from renderer
		auto image = m_renderer.getImage();

		if (image)
		{
			// Display image as ImGui texture
			ImGui::Image(
				image->getDescriptorSet(),
				{ 
					(float)image->getWidth(), (float)image->getHeight() 
				},
				ImVec2(0, 1),	// UV top-left
				ImVec2(1, 0)	// UV bottom-right (flipped vertically)
			);
		}

		ImGui::End();
	}

	// Opens a modal popup about the basic information of the application
	void drawAbout()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Dievas v1.0.0");
			ImGui::Text("3D Scene Creator & Light Simulation");
			ImGui::Separator();
			ImGui::Text("Copyright (C) 2026-2026 TohveliDev");
			ImGui::Separator();

			if (ImGui::Button("Close"))
			{
				g_aboutOpened = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	// Opens a modal popup about the controls, and what various parameters do (Quick Reference)
	void drawQR()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Quick Reference", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Camera Controls:");
			ImGui::Text("Move (X-axis) - Mouse 2 + A/D");
			ImGui::Text("Move (Z-axis) - Mouse 2 + W/S");
			ImGui::Text("Move (Y-axis) - Mouse 2 + Q/E");
			ImGui::Text("Rotate - Mouse 2 + Z/C");
			ImGui::Separator();
			ImGui::Text("Scene Parameters:");
			ImGui::Text("Clear Color - Background Color (Only if Skybox is OFF)");
			ImGui::Text("Ray Bounces - How many times a Ray of light bounces from object to object");
			ImGui::Text("Accumulation - Allows the rendering of multiple frames");
			ImGui::Text("Skybox - Creates a realistic background for the simulation");
			ImGui::Separator();
			ImGui::Text("Skybox Parameters:");
			ImGui::Text("Sun Direction - Where the sun is located");
			ImGui::Text("Sun Disc Size - The size of the sun");
			ImGui::Text("Sun Glow Color - The color of light the sun produces");
			ImGui::Text("Sun Intensity - How bright the core of the sun is");
			ImGui::Text("Sun Glow Intensity - How strong the glow around the sun is");
			ImGui::Text("Sun Glow Size - How widely the glow around the sun spreads");
			ImGui::Text("Zenith Color - Color of the sky");
			ImGui::Text("Horizon Color - Color of the horizon");
			ImGui::Text("Sky Intensity - How bright the entire skybox is");
			ImGui::Text("Horizon Intensity - How strong the horizon glow is");
			ImGui::Text("Horizon Falloff - How thich the horizon band is");
			ImGui::Separator();
			ImGui::Text("Material Parameters:");
			ImGui::Text("Albedo - Base color of the Material (RGB)");
			ImGui::Text("Roughness - Sharpness of a reflection (0 Smooth -> 1 Matte)");
			ImGui::Text("Metallic - How metallic the Material is (0 Plastic -> 1 Metal)");
			ImGui::Text("Anisotrophy - How stretched the reflected light is (0 Circular -> 1 Brushed)");
			ImGui::Text("Anistrophy Rotation - The angle of anistrophy (0 Horizontal -> 90 Vertical)");
			ImGui::Text("Emission Power - How potent the light a Material emits is");
			ImGui::Text("Emission Color - The color of light a Material emits");
			ImGui::Text("Transmission - How seethrough a Material is (0 Opaque -> 1 Transparent)");
			ImGui::Text("Absorption - How much light the material absorbs (0 Clear Glass -> 5 Densely Coloured Glass)");
			ImGui::Text("Index of Refraction (IoR) - How strongly a Ray of light bends when entering a Material");
			ImGui::Text("(For more information about common IoR values, check the IoR Spread Sheet)");
			ImGui::Separator();

			if (ImGui::Button("Close"))
			{
				g_qrOpened = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	// The application sidebar, where the user can edit objects and materials
	void drawSideBar()
	{
		float sidebarWidth = 550.f;

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoCollapse;

		ImGui::Begin("Sidebar", nullptr, flags);

		sidebarWidth = ImGui::GetWindowWidth();

		float totalHeight = ImGui::GetContentRegionAvail().y - 40;
		float bottomHeight = 535.f;
		float topHeight = totalHeight - bottomHeight;

		if (ImGui::BeginTabBar("SidebarTabs"))
		{
			if (ImGui::BeginTabItem("Objects"))
			{
				if (ImGui::BeginChild("ObjectsTop", ImVec2(0, topHeight), true))
				{
					float width = ImGui::GetContentRegionAvail().x;

					for (int i = 0; i < m_scene.objects.size(); i++)
					{
						if (ImGui::Button(m_scene.objects[i].name.c_str(), ImVec2(width, 0.f)))
						{
							m_selectedMesh = i;
						}
					}

					ImGui::EndChild();
				}

				ImGui::Spacing();

				if (ImGui::BeginChild("ObjectsBottom", ImVec2(0, bottomHeight), true))
				{
					if (m_selectedMesh >= 0)
					{
						Object& obj = m_scene.objects[m_selectedMesh];
						static char buffer[64];

						std::strncpy(buffer, obj.name.c_str(), sizeof(buffer));
						buffer[sizeof(buffer) - 1] = '\0';

						ImGui::Text("Object ID: %u", m_selectedMesh);
						ImGui::Separator();
						if (ImGui::InputText("Object Name", buffer, IM_ARRAYSIZE(buffer))) { obj.name = buffer; }
						ImGui::Separator();
						if (ImGui::DragFloat3("Position", glm::value_ptr(obj.position), 0.05f, -FLT_MAX, FLT_MAX)) 
						{ 
							m_renderer.recalculateScene(m_scene);
							m_renderer.resetFrameIndex();
						}

						if (ImGui::DragFloat3("Scale", glm::value_ptr(obj.scale), 0.05f, 0.001f, FLT_MAX)) 
						{
							m_renderer.recalculateScene(m_scene);
							m_renderer.resetFrameIndex();
						}

						if (ImGui::DragFloat4("Rotation", glm::value_ptr(obj.rotation), 0.01f))
						{
							m_renderer.recalculateScene(m_scene);
							m_renderer.resetFrameIndex();
						}
						
						ImGui::Separator();

						int materialCount = static_cast<int>(m_scene.materials.size());
						int maxMaterialIndex = materialCount > 0 ? materialCount - 1 : 0;

						if (ImGui::DragInt("Material", &obj.materialIndex, 1.0f, 0, maxMaterialIndex, "%d", ImGuiSliderFlags_AlwaysClamp)) 
						{ 
							obj.materialIndex = std::clamp(obj.materialIndex, 0, maxMaterialIndex);
							m_renderer.resetFrameIndex(); 
						}

						ImGui::Separator();
						
						if (ImGui::Button("Delete Object", ImVec2(-FLT_MIN, 0.0f)))
						{
							int temp = m_selectedMesh;

							m_selectedMesh = -1;

							m_scene.objects.erase(m_scene.objects.begin() + temp);
							m_scene.meshes.erase(m_scene.meshes.begin() + temp);

							for (auto& obj : m_scene.objects)
							{
								if (obj.meshIndex > temp)
									obj.meshIndex--;
							}

							m_renderer.recalculateScene(m_scene);
							m_renderer.resetFrameIndex();
						}
					}

					ImGui::EndChild();
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Materials"))
			{
				if (ImGui::BeginChild("MaterialsTop", ImVec2(0, topHeight), true))
				{
					float width = ImGui::GetContentRegionAvail().x;

					for (int i = 0; i < m_scene.materials.size(); i++)
					{
						if (ImGui::Button(m_scene.materials[i].name.c_str(), ImVec2(width, 0.f)))
						{
							m_selectedMaterial = i;
						}
					}
					ImGui::EndChild();
				}

				ImGui::Spacing();

				if (ImGui::BeginChild("MaterialsBottom", ImVec2(0, bottomHeight), true))
				{
					if (m_selectedMaterial >= 0)
					{
						Material& material = m_scene.materials[m_selectedMaterial];
						char buffer[64];
						std::strncpy(buffer, material.name.c_str(), sizeof(buffer));
						buffer[sizeof(buffer) - 1] = '\0';

						float rotationDegrees = glm::degrees(material.anisotropyRotation);

						ImGui::Text("Material ID: %u", m_selectedMaterial);
						ImGui::Separator();

						if (ImGui::InputText("Material Name", buffer, IM_ARRAYSIZE(buffer))) { material.name = buffer; }
						ImGui::Separator();

						ImGui::Text("Basic Parameters");
						ImGui::Separator();
						if (ImGui::ColorEdit3("Albedo", glm::value_ptr(material.albedo))) { m_renderer.resetFrameIndex(); }
						if (ImGui::DragFloat("Roughness", &material.roughness, 0.01f, 0.0f, 1.0f)) { m_renderer.resetFrameIndex(); }
						if (ImGui::DragFloat("Metallic", &material.metallic, 0.01f, 0.0f, 1.0f)) { m_renderer.resetFrameIndex(); }
						if (ImGui::DragFloat("Anisotrophy", &material.anisotropy, 0.01f, -1.0f, 1.0f)) { m_renderer.resetFrameIndex(); }
						if (ImGui::SliderFloat("Anistrophy Rotation", &rotationDegrees, 0.0f, 360.0f, "%1.f deg"))
						{
							material.anisotropyRotation = glm::radians(rotationDegrees);
							m_renderer.resetFrameIndex();
						}

						ImGui::Separator();
						ImGui::Text("Light Parameters");
						ImGui::Separator();

						if (ImGui::ColorEdit3("Emission Color", glm::value_ptr(material.emissionColor))) { m_renderer.resetFrameIndex(); }
						if (ImGui::DragFloat("Emission Power", &material.emissionPower, 0.01f, 0.0f, FLT_MAX)) { m_renderer.resetFrameIndex(); }

						ImGui::Separator();
						ImGui::Text("Glass Parameters");
						ImGui::Separator();

						if (ImGui::DragFloat("Transmission", &material.transmission, 0.01f, 0.0f, 1.0f)) { m_renderer.resetFrameIndex(); }
						if (ImGui::DragFloat3("Absorption", glm::value_ptr(material.absorption), 0.01f, 0.0f, 10.0f)) { m_renderer.resetFrameIndex(); }
						if (ImGui::DragFloat("IoR", &material.ior, 0.01f, 1.0f, 2.5f)) { m_renderer.resetFrameIndex(); }

						if (ImGui::Button("Calculate Absorption", ImVec2(-FLT_MIN, 0.0f)))
						{
							material.calculateAbsorption();
							m_renderer.resetFrameIndex();
						}

						ImGui::Separator();

						if (ImGui::Button("Delete Material", ImVec2(-FLT_MIN, 0.0f)))
						{
							int temp = m_selectedMaterial;

							for (int i = 0; i < m_scene.objects.size(); i++)
							{
								if (m_scene.objects[i].materialIndex >= temp)
								{
									m_scene.objects[i].materialIndex = m_scene.objects[i].materialIndex - 1;
								}
							}

							m_selectedMaterial = -1;
							m_scene.materials.erase(m_scene.materials.begin() + temp);
							m_renderer.resetFrameIndex();
						}
					}

					ImGui::EndChild();
				}

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();
	}

	// The bottom bar, where the user can see the size of the viewport, how long rendering took time and how many times
	// the scene has been accumulated
	void drawBottomBar()
	{
		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoCollapse;

		ImGui::Begin("Bottom Bar", nullptr, flags);

		ImGui::Text("Viewport: %ux%u", m_viewportWidth, m_viewportHeight);
		ImGui::SameLine();
		ImGui::Text("Last Render Time: %.3fms", m_lastRenderTime);
		ImGui::SameLine();
		ImGui::Text("Times Accumulated: %u", m_renderer.getFrameIndex());

		ImGui::End();
	}

	// Per-frame render function: updates render targets, camera, and executes rendering
	// Also measures CPU-side frame render time
	void onRender()
	{
		// Start CPU timer for measuring render duration
		Timer timer;

		// Ensure renderer matches current viewport size (may recreate targets/buffers)
		m_renderer.onResize(m_viewportWidth, m_viewportHeight);

		// Update camera projection + ray directions for new viewport size
		m_camera.onResize(m_viewportWidth, m_viewportHeight);

		// Execute main rendering pass (scene + camera)
		m_renderer.render(m_scene, m_camera);

		// Store CPU time spent in this render call (in milliseconds)
		m_lastRenderTime = timer.elapsedMillis();
	}

private:
	Renderer m_renderer;
	Camera m_camera;
	Scene m_scene;
	uint32_t m_viewportWidth = 0, m_viewportHeight = 0;
	float m_lastRenderTime = 0.0f;
	bool m_shouldRender = true;
	bool m_shouldAcculate = false;
	int m_selectedMaterial = -1;
	int m_selectedMesh = -1;
};

// Factory function that creates and configures the main Application instance.
// Sets up window specs, layers, and ImGui menubar callbacks.
Application* createApplication(int argc, char** argv)
{
	// Define application/window configuration
	AppSpecification specs;
	specs.name = "Dievas Editor";
	specs.height = 1000;
	specs.width = 1400;

	// Create application instance
	Application* app = new Application(specs);

	// Push main editor layer
	app->pushLayer<TestLayer>();

	// Define ImGui menubar UI
	app->setMenubarCallback([app]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Import 3D Object (.obj)"))
			{
				g_importing = true;
			}

			if (ImGui::MenuItem("Create Material"))
			{
				g_materialCreatorOpened = true;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Export As PNG"))
			{
				g_renderOpened = true;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Quit"))
			{
				app->close();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Properties"))
			{
				g_propertiesOpened = true;
			}

			if (ImGui::MenuItem("Skybox Properties"))
			{
				g_skyboxOpen = true;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Reset Defaults"))
			{
				g_resetView = true;
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{

			if (ImGui::MenuItem("Quick Reference"))
			{
				g_qrOpened = true;
			}

			if (ImGui::MenuItem("IoR Cheat Sheet"))
			{
				g_cheatSheetOpened = true;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("About"))
			{
				g_aboutOpened = true;
			}
			ImGui::EndMenu();
		}

	});
	return app;
}