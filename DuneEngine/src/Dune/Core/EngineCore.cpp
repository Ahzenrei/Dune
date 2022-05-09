#include "pch.h"
#include "EngineCore.h"
#include "Dune/Core/Logger.h"

namespace Dune
{
	std::unique_ptr<EntityManager> EngineCore::m_entityManager = nullptr;
	bool EngineCore::isInitialized = false;

	void EngineCore::Init()
	{
		if (isInitialized)
		{
			LOG_CRITICAL("Tried to initialize EngineCore which is already initialized");
			return;
		}

		m_entityManager = std::make_unique<EntityManager>();

		srand((unsigned int)time(NULL));

		for (int i = 0; i < 10000; i++)
		{
			EntityID entity = m_entityManager->CreateEntity();
			if((rand() % 11) < 3)
			{
				m_entityManager->RemoveEntity(entity);
			}
		}

		isInitialized = true;
	}

	void EngineCore::Shutdown()
	{
#ifdef _DEBUG
		if (!isInitialized)
		{
			LOG_CRITICAL("Tried to shutdown EngineCore which is not initialized");
			return;
		}
#endif // _DEBUG
	}

	void EngineCore::Update()
	{
#ifdef _DEBUG
		if (!isInitialized)
		{
			LOG_CRITICAL("Tried to update EngineCore which is not initialized");
			return;
		}
#endif // _DEBUG
	}
}