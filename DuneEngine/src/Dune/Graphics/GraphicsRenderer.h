#pragma once

#include "GraphicsElement.h"

namespace Dune
{
	class Window;
	class GraphicsBuffer;
	struct GraphicsBufferDesc;

	class GraphicsRenderer
	{
	public:
		virtual ~GraphicsRenderer() = default;
		GraphicsRenderer(const GraphicsRenderer&) = delete;
		GraphicsRenderer& operator=(const GraphicsRenderer&) = delete;

		static std::unique_ptr<GraphicsRenderer> Create(const Window * window);

		void ClearGraphicsElement() { m_graphicsElements.clear(); }
		void RemoveGraphicsElement(EntityID id) { m_graphicsElements.erase(id); }
		void AddGraphicsElement(EntityID id, const GraphicsElement& elem);
		void UpdateCamera();

		virtual void Render() = 0;
		virtual void OnShutdown() = 0;
		virtual void OnResize(int width, int height) = 0;
		//TODO : Get rid of const void* data
		virtual std::unique_ptr<GraphicsBuffer> CreateBuffer(const void* data, const GraphicsBufferDesc& desc) = 0;
		virtual void UpdateBuffer(std::unique_ptr<GraphicsBuffer>& buffer, const void* data) = 0;


	protected:
		GraphicsRenderer() = default;
		dHashMap<EntityID, GraphicsElement> m_graphicsElements;
		std::unique_ptr<GraphicsBuffer>	m_cameraMatrixBuffer;
	};
}

