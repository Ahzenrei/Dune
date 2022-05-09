#include "pch.h"
#include "GraphicsRenderer.h"

#ifdef DUNE_PLATFORM_WINDOWS
#include "Platform/DirectX12/DX12GraphicsRenderer.h"
#endif

namespace Dune
{
	std::unique_ptr<GraphicsRenderer> GraphicsRenderer::Create(const void * window)
	{
#ifdef DUNE_PLATFORM_WINDOWS
		return std::make_unique<DX12GraphicsRenderer>((const WindowsWindow*)window);
#else
#error Platform not supported
#endif
	}
	void GraphicsRenderer::AddGraphicsElement(const GraphicsElement& elem)
	{
		m_graphicsElements.push_back(elem);
	}
}