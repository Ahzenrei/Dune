#include "pch.h"
#include "EngineCore.h"
#include "Dune/Core/Logger.h"
#include "Dune/Utilities/StringUtils.h"

namespace Dune
{
	std::unique_ptr<EntityManager> EngineCore::m_entityManager = nullptr;
	std::unique_ptr<ComponentManager<TransformComponent>> EngineCore::m_transformManager = nullptr;
	std::unique_ptr<ComponentManager<GraphicsComponent>> EngineCore::m_graphicsManager = nullptr;
	std::unique_ptr<ComponentManager<BindingComponent>> EngineCore::m_bindingManager = nullptr;
	bool EngineCore::m_isInitialized = false;
	SceneGraph EngineCore::m_sceneGraph;
	EntityID EngineCore::m_selectedEntity;
	bool EngineCore::m_showImGuiDemo;

	void EngineCore::Init()
	{
		if (m_isInitialized)
		{
			LOG_CRITICAL("Tried to initialize EngineCore which was already initialized");
			return;
		}

		m_entityManager = std::make_unique<EntityManager>();

		m_transformManager = std::make_unique<ComponentManager<TransformComponent>>();
		m_bindingManager = std::make_unique<ComponentManager<BindingComponent>>();
		m_graphicsManager = std::make_unique<ComponentManager<GraphicsComponent>>();

		m_isInitialized = true;
	}

	void EngineCore::Shutdown()
	{
#ifdef _DEBUG
		if (!m_isInitialized)
		{
			LOG_CRITICAL("Tried to shutdown EngineCore which was not initialized");
			return;
		}
#endif // _DEBUG
	}

	void EngineCore::Update()
	{
#ifdef _DEBUG
		if (!m_isInitialized)
		{
			LOG_CRITICAL("Tried to update EngineCore which was not initialized");
			return;
		}
#endif // _DEBUG


		DrawMainMenuBar();
		DrawInterface();
		if (m_showImGuiDemo)
			ImGui::ShowDemoWindow(&m_showImGuiDemo);
	}

	EntityID EngineCore::CreateEntity(const dString& name)
	{
#ifdef _DEBUG
		if (!m_isInitialized)
		{
			LOG_CRITICAL("Tried to create an entity while EngineCore was not initialized");
			return ID::invalidID;
		}
#endif // _DEBUG

		EntityID id = m_entityManager->CreateEntity();
		
		//Add mandatory components
		m_transformManager->Create(id);
		m_bindingManager->Create(id);

		m_sceneGraph.AddNode(id, name);
		return id;
	}

	void EngineCore::RemoveEntity(EntityID id)
	{
#ifdef _DEBUG
		if (!m_isInitialized)
		{
			LOG_CRITICAL("Tried to remove an entity while EngineCore was not initialized");
			return;
		}
#endif // _DEBUG
		m_entityManager->RemoveEntity(id);
		m_sceneGraph.DeleteNode(id);
	}

	void EngineCore::DrawMainMenuBar()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("Main"))
			{
				if (ImGui::MenuItem("Show ImGui demo"))
				{
					m_showImGuiDemo = true;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	void EngineCore::DrawInterface()
	{
		DrawScene();
		DrawInspector();
	}

	void EngineCore::DrawScene()
	{
		static ID::IDType counter = 0;

		ImGui::Begin("Scene");
		ImGui::Text("Scene graph");
		ImGui::SameLine();
		if (ImGui::Button("Add Entity"))
		{
			CreateEntity(dStringUtils::printf("%u", counter++).c_str());
		}
		ImGui::SameLine();
		if (ImGui::Button("Remove Entity") && (ID::IsValid(m_selectedEntity)))
		{
			RemoveEntity(m_selectedEntity);
			m_selectedEntity = ID::invalidID;
		}
		ImGui::Separator();
		DrawGraph();
		ImGui::End();
	}

	void EngineCore::DrawGraph()
	{
		for (const SceneGraph::Node* child : m_sceneGraph.GetRoot()->GetChildren())
		{
			DrawNode(child);
		}
	}

	void EngineCore::DrawNode(const SceneGraph::Node* node)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

		//if selected, highlight it
		if (node->GetSelf() == m_selectedEntity)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		//Create tree node
		bool isOpen = ImGui::TreeNodeEx((void*)(uintptr_t)node->GetSelf(), flags, node->GetName().c_str());

		//Select on click
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			m_selectedEntity = node->GetSelf();
		}

		//Draw children if opened
		if (isOpen)
		{
			for (const SceneGraph::Node* child : node->GetChildren())
			{
				DrawNode(child);
			}

			ImGui::TreePop();
		}
	}
	void EngineCore::DrawInspector()
	{
		ImGui::Begin("Inspector");
		if (const SceneGraph::Node * node = m_sceneGraph.GetNode(m_selectedEntity))
		{
			ImGui::Text("%s", node->GetName());
			ImGui::Separator();
			if (TransformComponent * transform = m_transformManager->GetComponent(m_selectedEntity))
			{
				ImGui::Text("Transform :");

				dVector3& pos = transform->position;
				float imGuiPos[3] = { pos.x, pos.y, pos.z };
				ImGui::InputFloat3("Position", imGuiPos, "%.2f");
				pos.x = imGuiPos[0];
				pos.y = imGuiPos[1];
				pos.z = imGuiPos[2];

				dVector3& rot = transform->rotation;
				float imGuiRot[3] = { rot.x, rot.y, rot.z };
				ImGui::InputFloat3("Rotation", imGuiRot, "%.2f");
				rot.x = imGuiRot[0];
				rot.y = imGuiRot[1];
				rot.z = imGuiRot[2];

				dVector3& scale = transform->scale;
				float imGuiScale[3] = { scale.x, scale.y, scale.z };
				ImGui::InputFloat3("Scale", imGuiScale, "%.2f");
				scale.x = imGuiScale[0];
				scale.y = imGuiScale[1];
				scale.z = imGuiScale[2];
			}
		}
		ImGui::End();
	}
}