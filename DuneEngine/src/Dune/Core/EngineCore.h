#pragma once

#include "Dune/Core/SceneGraph.h"
#include "Dune/Core/ECS/EntityManager.h"
#include "Dune/Core/ECS/Components/ComponentManager.h"
#include "Dune/Core/ECS/Components/TransformComponent.h"
#include "Dune/Core/ECS/Components/BindingComponent.h"
#include "Dune/Core/ECS/Components/GraphicsComponent.h"

namespace Dune
{

	class EngineCore
	{
	public:
		EngineCore() = delete;
		~EngineCore() = delete;

		static void Init();
		static void Shutdown();
		static void Update();

		static EntityID CreateEntity(const dString& name);
		static void RemoveEntity(EntityID);

		template<typename Component>
		static void AddComponent(EntityID id)
		{
			ComponentManager<Component>::Create(id);
		}

		template<typename Component>
		static Component* GetComponent(EntityID id)
		{
			return ComponentManager<Component>::GetComponent(id);
		}

		template<typename Component>
		static void RemoveComponent(EntityID id)
		{
			if (ComponentManager<Component>::Contains(id))
			{
				ComponentManager<Component>::Remove(id);
			}
		}

		inline static bool IsInitialized() { return m_isInitialized; }

	private:
		static void DrawMainMenuBar();
		static void DrawInterface();
		static void DrawScene();
		static void DrawGraph();
		static void DrawNode(const SceneGraph::Node* node);
		static void DrawInspector();
	private:
		static bool m_isInitialized;
		static std::unique_ptr<EntityManager> m_entityManager;
		static SceneGraph m_sceneGraph;
		static EntityID m_selectedEntity;
		static bool m_showImGuiDemo;
	};
}

