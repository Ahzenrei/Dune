#pragma once

#include "Dune/Graphics/GraphicsRenderer.h"

namespace Dune
{
	class GraphicsCore
	{
	public:
		GraphicsCore() = delete;
		~GraphicsCore() = delete;

		static void Init(const Window* window);
		static void Shutdown();
		static void Update(float dt);

		static inline bool IsInitialized() { return isInitialized; }
		static inline GraphicsRenderer& GetGraphicsRenderer() { return *m_graphicsRenderer; }

	private:
		static std::unique_ptr<GraphicsRenderer> m_graphicsRenderer;
		static bool isInitialized;
	};
}

