#include "pch.h"

#include "Platform/DirectX12/DX12GraphicsRenderer.h"
#include "Platform/DirectX12/DX12GraphicsBuffer.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Dune/Core/Logger.h"
#include "Dune/Utilities/StringUtils.h"
#include "Dune/Core/EngineCore.h"

namespace Dune
{
	DX12GraphicsRenderer::DX12GraphicsRenderer(const WindowsWindow* window)
	{
		CreateFactory();
		CreateDevice();
		CreateCommandQueues();
		CreateSwapChain(window->GetHandle());
		CreateRenderTargets();
		CreateDepthStencil(window->GetWidth(), window->GetHeight());
		CreateCommandAllocators();
		CreateRootSignature();
		CreatePipeline();
		CreateCommandLists();
		CreateFences();
		CreateLightsBuffer((dU32)sizeof(PointLight));

		dMatrix identity;
		GraphicsBufferDesc camBufferDesc;
		camBufferDesc.size = sizeof(dMatrix);
		camBufferDesc.usage = EBufferUsage::Upload;
		m_cameraMatrixBuffer = CreateBuffer(&identity, camBufferDesc);

		//Create ImGui descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 1;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_imguiHeap)));
		ImGui_ImplDX12_Init(m_device.Get(), FrameCount,
			DXGI_FORMAT_R8G8B8A8_UNORM, m_imguiHeap.Get(),
			m_imguiHeap->GetCPUDescriptorHandleForHeapStart(),
			m_imguiHeap->GetGPUDescriptorHandleForHeapStart());
	}

	DX12GraphicsRenderer::~DX12GraphicsRenderer()
	{
#ifdef _DEBUG
		Microsoft::WRL::ComPtr<ID3D12DebugDevice2> debugDevice;
		m_device->QueryInterface(IID_PPV_ARGS(&debugDevice));
		debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
#endif
	}

	void DX12GraphicsRenderer::WaitForFrame(const dU64 frameIndex)
	{
		rmt_ScopedCPUSample(WaitForFrame, 0);

		Assert(m_fence && m_fenceEvent.IsValid());


		const UINT64 frameFenceValue = m_fenceValues[frameIndex];
		if (m_fence->GetCompletedValue() < frameFenceValue)
		{
			ThrowIfFailed(m_fence->SetEventOnCompletion(frameFenceValue, m_fenceEvent.Get()))
			std::ignore = WaitForSingleObjectEx(m_fenceEvent.Get(), INFINITE, FALSE);
		}

		//Temp since we assume that mesh get instantly uploaded
		WaitForCopy();
		
	}

	void DX12GraphicsRenderer::WaitForCopy()
	{
		Assert(m_copyFence && m_copyFenceEvent.IsValid());

		const UINT64 copyFenceValue = m_copyFenceValue;
		if (m_copyFence->GetCompletedValue() < copyFenceValue)
		{
			ThrowIfFailed(m_copyFence->SetEventOnCompletion(copyFenceValue, m_copyFenceEvent.Get()))
				std::ignore = WaitForSingleObjectEx(m_copyFenceEvent.Get(), INFINITE, FALSE);
		}
	}

	void DX12GraphicsRenderer::Render()
	{
		rmt_ScopedCPUSample(Render, 0);
		const UINT64 frameIndex = m_swapChain->GetCurrentBackBufferIndex();
		WaitForFrame(frameIndex);
		ImGui::Render();
		PopulateCommandList();

		// Execute the command list.
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// Present the frame.
		ThrowIfFailed(m_swapChain->Present(1, 0));

		//Signal when frame is over
		ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_frameNumber));
		m_fenceValues[frameIndex] = m_frameNumber;

		m_frameNumber++;

	}

	void DX12GraphicsRenderer::OnShutdown()
	{
		for (dU32 i = 0; i < FrameCount; i++)
		{
			WaitForFrame(i);
		}

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
	}

	void DX12GraphicsRenderer::OnResize(int x, int y)
	{
		// Wait until all previous frames are processed.
		for (dU32 i = 0; i < FrameCount; i++)
		{
			WaitForFrame(i);
		}

		// Release resources that are tied to the swap chain.
		for (dU32 i = 0; i < FrameCount; i++)
		{
			m_renderTargets[i].Reset();
		}

		ThrowIfFailed(m_swapChain->ResizeBuffers(
			FrameCount,
			x,
			y,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			0
		));

		CreateRenderTargets();
		CreateDepthStencil(x, y);

		m_viewport.Width = static_cast<float>(x);
		m_viewport.Height = static_cast<float>(y);

		m_scissorRect.right = static_cast<LONG>(x);
		m_scissorRect.bottom = static_cast<LONG>(y);
	}

	// TODO :Should use a copy engine
	std::unique_ptr<GraphicsBuffer> DX12GraphicsRenderer::CreateBuffer(const void* data, const GraphicsBufferDesc& desc)
	{
		std::unique_ptr<DX12GraphicsBuffer> buffer = std::make_unique<DX12GraphicsBuffer>();
		buffer->SetDescription(desc);

		const UINT bufferSize = desc.size;

		D3D12_HEAP_PROPERTIES heapProps;
		D3D12_RESOURCE_STATES resourceState;

		if (desc.usage == EBufferUsage::Upload)
		{
			heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
			resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
		}
		else
		{
			heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
			resourceState = D3D12_RESOURCE_STATE_COMMON;
		}

		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC resourceDesc;
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = bufferSize;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			resourceState,
			nullptr,
			IID_PPV_ARGS(&buffer->m_buffer)));

		if (data)
		{
			UpdateBuffer(buffer.get(), data);
		}

		return std::move(buffer);
	}

	void DX12GraphicsRenderer::UpdateBuffer(GraphicsBuffer* buffer, const void* data)
	{
		DX12GraphicsBuffer* graphicsBuffer = static_cast<DX12GraphicsBuffer*>(buffer);

		GraphicsBufferDesc desc = graphicsBuffer->GetDescription();

		if (desc.usage == EBufferUsage::Default)
		{
			D3D12_HEAP_PROPERTIES heapProps;
			heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProps.CreationNodeMask = 1;
			heapProps.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC resourceDesc;
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = desc.size;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
			ThrowIfFailed(m_device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&uploadBuffer)));

			UINT8* pDataBegin;
			D3D12_RANGE readRange;
			readRange.Begin = 0;
			readRange.End = 0;
			ThrowIfFailed(uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pDataBegin)));
			memcpy(pDataBegin, data, desc.size);
			uploadBuffer->Unmap(0, nullptr);

			WaitForCopy();
			ThrowIfFailed(m_copyCommandAllocator->Reset());
			ThrowIfFailed(m_copyCommandList->Reset(m_copyCommandAllocator.Get(), nullptr));

			m_copyCommandList->CopyBufferRegion(graphicsBuffer->m_buffer.Get(), 0, uploadBuffer.Get(), 0, desc.size);
			m_copyCommandList->Close();
			dVector<ID3D12CommandList*> ppCommandLists{ m_copyCommandList.Get() };
			m_copyCommandQueue->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()), ppCommandLists.data());

			ThrowIfFailed(m_copyCommandQueue->Signal(m_copyFence.Get(), m_copyFenceValue + 1));
			m_copyFenceValue += 1;

			//TODO : Store somewhere the temp upload buffer so we don't have to wait so it doesn't release too soon
			WaitForCopy();
		}
		else if (desc.usage == EBufferUsage::Upload)
		{
			UINT8* pDataBegin;
			D3D12_RANGE readRange;
			readRange.Begin = 0;
			readRange.End = 0;
			ThrowIfFailed(graphicsBuffer->m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&pDataBegin)));
			memcpy(pDataBegin, data, desc.size);
			graphicsBuffer->m_buffer->Unmap(0, nullptr);
		}
	}

	void DX12GraphicsRenderer::CreateFactory()
	{
		UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
		{
			// Create a Debug Controller to track errors
			Microsoft::WRL::ComPtr<ID3D12Debug> dc;
			Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
			ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&dc)));
			ThrowIfFailed(dc->QueryInterface(IID_PPV_ARGS(&debugController)));
			debugController->EnableDebugLayer();
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
#endif
		// Create Factory
		ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

	}

	void DX12GraphicsRenderer::CreateDevice()
	{
		// Create Adapter
		Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

		for (UINT adapterIndex = 0;
			DXGI_ERROR_NOT_FOUND != m_factory->EnumAdapters1(adapterIndex, &adapter);
			++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			// Don't select the Basic Render Driver adapter.
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			//  Check if the adapter supports Direct3D 12, and use that for the rest
			// of the application
			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
				_uuidof(ID3D12Device), nullptr)))
			{
				break;
			}

			// Else we won't use this iteration's adapter, so release it
			adapter->Release();
		}

		// Create Device
		ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&m_device)));
		NameDXObject(m_device, L"Device");
	}

	void DX12GraphicsRenderer::CreateCommandQueues()
	{
		// Describe and create the command queue.
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
		NameDXObject(m_commandQueue, L"CommandQueue");
		
		D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
		copyQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
		ThrowIfFailed(m_device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&m_copyCommandQueue)));
		NameDXObject(m_copyCommandQueue, L"CopyCommandQueue");
	}

	void DX12GraphicsRenderer::CreateSwapChain(HWND handle)
	{
		RECT clientRect;
		GetClientRect(handle, &clientRect);

		m_scissorRect.left = 0;
		m_scissorRect.top = 0;
		m_scissorRect.right = static_cast<LONG>(clientRect.right);
		m_scissorRect.bottom = static_cast<LONG>(clientRect.bottom);

		m_viewport.TopLeftX = 0.0f;
		m_viewport.TopLeftY = 0.0f;
		m_viewport.Width = static_cast<float>(clientRect.right);
		m_viewport.Height = static_cast<float>(clientRect.bottom);
		m_viewport.MinDepth = 0.f;
		m_viewport.MaxDepth = 1.f;

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferCount = FrameCount;
		swapChainDesc.BufferDesc.Width = clientRect.right;
		swapChainDesc.BufferDesc.Height = clientRect.bottom;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.OutputWindow = handle;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.Windowed = TRUE;

		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
		ThrowIfFailed(m_factory->CreateSwapChain(
			m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
			&swapChainDesc,
			&swapChain
		));
		ThrowIfFailed(m_factory->MakeWindowAssociation(handle, DXGI_MWA_NO_ALT_ENTER));
		ThrowIfFailed(swapChain.As(&m_swapChain));
	}

	void DX12GraphicsRenderer::CreateRenderTargets()
	{

		// Create descriptor heaps.
		{
			// Describe and create a render target view (RTV) descriptor heap.
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = FrameCount;
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
			NameDXObject(m_rtvHeap,L"RtvHeap");
			m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		// Create frame resources.
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

			// Create a RTV for each frame.
			for (UINT i = 0; i < FrameCount; i++)
			{
				ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
				m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
				NameDXObjectIndexed(m_renderTargets[i], i, L"RenderTarget");
				rtvHandle.ptr += (1 * m_rtvDescriptorSize);
			}
		}
	}

	void DX12GraphicsRenderer::CreateDepthStencil(int width, int height)
	{
		// Resize screen dependent resources.
		// Create a depth buffer.
		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		optimizedClearValue.DepthStencil = { 1.0f, 0 };

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		dsvHeapDesc.NodeMask = 0;

		ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
		NameDXObject(m_dsvHeap, L"DsvHeap");

		D3D12_HEAP_PROPERTIES heapProps;
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 0;
		heapProps.VisibleNodeMask = 0;

		D3D12_RESOURCE_DESC dsDesc = {};
		dsDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		dsDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsDesc.Width = width;
		dsDesc.Height = height;
		dsDesc.DepthOrArraySize = 1;
		dsDesc.MipLevels = 1;
		dsDesc.SampleDesc.Count = 1;
		dsDesc.SampleDesc.Quality = 0;
		dsDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		dsDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		dsDesc.Alignment = 0;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&dsDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optimizedClearValue,
			IID_PPV_ARGS(&m_depthStencilBuffer)
		));
		NameDXObject(m_depthStencilBuffer, L"DepthStencilBuffer");

		// Update the depth-stencil view.
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc,
			m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	void DX12GraphicsRenderer::CreateCommandAllocators()
	{
		for (dU32 i = 0; i < FrameCount; i++)
		{
			ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
			NameDXObjectIndexed(m_commandAllocators[i], i,  L"CommandAllocators");
		}

		ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_copyCommandAllocator)));
		NameDXObject(m_copyCommandAllocator, L"CopyCommandAllocator");
	}

	void DX12GraphicsRenderer::CreateRootSignature()
	{
		D3D12_ROOT_PARAMETER1 rootParameters[4];
		//Instance Matrices (MVP and Normal)
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[0].Constants.RegisterSpace = 0;
		rootParameters[0].Constants.ShaderRegister = 0;
		rootParameters[0].Constants.Num32BitValues = 2 * sizeof(dMatrix4x4) / 4;

		//Material (BaseColor)
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[1].Constants.RegisterSpace = 0;
		rootParameters[1].Constants.ShaderRegister = 1;
		rootParameters[1].Constants.Num32BitValues = sizeof(dVec4) / 4;

		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[2].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
		rootParameters[2].Descriptor.RegisterSpace = 0;
		rootParameters[2].Descriptor.ShaderRegister = 2;

		D3D12_DESCRIPTOR_RANGE1 range;
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // we're using structured buffers - it's a SRV
		range.NumDescriptors = 1; // we have 2 structured buffers and 2 descriptors
		range.BaseShaderRegister = 0; // we start from the first register (t0)
		range.RegisterSpace = 0; // this allows us to use the same register name if we use different space
		range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

		rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[3].DescriptorTable.pDescriptorRanges = &range;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSignatureDesc.Desc_1_1.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		rootSignatureDesc.Desc_1_1.NumParameters = _countof(rootParameters);
		rootSignatureDesc.Desc_1_1.pParameters = rootParameters;
		rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
		rootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		Microsoft::WRL::ComPtr<ID3DBlob> error;
		if (FAILED(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error)))
		{
			OutputDebugStringA((const char *)error->GetBufferPointer());
			Assert(0);
		}
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	}

	void DX12GraphicsRenderer::CreatePipeline()
	{
		Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
		Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		dWString vertexPath = SHADER_DIR;
		vertexPath.append(L"VertexShader.cso");
		dWString pixelPath = SHADER_DIR;
		pixelPath.append(L"PixelShader.cso");

		ThrowIfFailed(D3DReadFileToBlob(vertexPath.c_str(), &vertexShader));
		ThrowIfFailed(D3DReadFileToBlob(pixelPath.c_str(), &pixelShader));
		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = { reinterpret_cast<UINT8*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
		psoDesc.PS = { reinterpret_cast<UINT8*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
		D3D12_RASTERIZER_DESC rasterDesc;
		rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasterDesc.FrontCounterClockwise = FALSE;
		rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasterDesc.DepthClipEnable = TRUE;
		rasterDesc.MultisampleEnable = FALSE;
		rasterDesc.AntialiasedLineEnable = FALSE;
		rasterDesc.ForcedSampleCount = 0;
		rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		psoDesc.RasterizerState = rasterDesc;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		D3D12_BLEND_DESC blendDesc;
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;
		const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
			FALSE,
			FALSE,
			D3D12_BLEND_ONE,
			D3D12_BLEND_ZERO,
			D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE,
			D3D12_BLEND_ZERO,
			D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		};
		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
			blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
		psoDesc.BlendState = blendDesc;
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	}

	void DX12GraphicsRenderer::CreateCommandLists()
	{
		// Create the command list.
		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
		ThrowIfFailed(m_commandList->Close());
		NameDXObject(m_commandList, L"CommandList");

		ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_copyCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_copyCommandList)));
		ThrowIfFailed(m_copyCommandList->Close());
		NameDXObject(m_copyCommandList,L"CopyCommandList");
	}

	void DX12GraphicsRenderer::CreateFences()
	{
		// Create synchronization objects and wait until assets have been uploaded to the GPU.
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		NameDXObject(m_fence, L"Fence");
		for (dU32 i = 0; i < FrameCount; i++)
		{
			m_fenceValues[i] = 0;
		}

		m_fenceEvent.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
		if (!m_fenceEvent.IsValid())
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_copyFence)));
		NameDXObject(m_copyFence, L"CopyFence");
		m_copyFenceValue = 0;

		m_copyFenceEvent.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
		if (!m_copyFenceEvent.IsValid())
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}
	}

	void DX12GraphicsRenderer::CreateLightsBuffer(dU32 size)
	{
		GraphicsBufferDesc desc;
		desc.size = size;
		desc.usage = EBufferUsage::Upload;
		m_lightsBuffer = CreateBuffer(nullptr, desc);

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
		ZeroMemory(&heapDesc, sizeof(heapDesc));
		heapDesc.NumDescriptors = 1;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NodeMask = 0;

		ThrowIfFailed(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_lightsHeap.ReleaseAndGetAddressOf())));
	}

	void DX12GraphicsRenderer::PopulateCommandList()
	{
		rmt_ScopedCPUSample(PopulateCommandList, 0);
		// Command list allocators can only be reset when the associated 
		// command lists have finished execution on the GPU; apps should use 
		// fences to determine GPU execution progress.
		const UINT64 frameIndex = m_swapChain->GetCurrentBackBufferIndex();
		ThrowIfFailed(m_commandAllocators[frameIndex]->Reset());

		// However, when ExecuteCommandList() is called on a particular command 
		// list, that command list can then be reset at any time and must be before 
		// re-recording.
		ThrowIfFailed(m_commandList->Reset(m_commandAllocators[frameIndex].Get(), m_pipelineState.Get()));

		// Set necessary state.
		m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
		m_commandList->RSSetViewports(1, &m_viewport);
		m_commandList->RSSetScissorRects(1, &m_scissorRect);

		// Indicate that the back buffer will be used as a render target.
		D3D12_RESOURCE_BARRIER renderTargetBarrier;
		renderTargetBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		renderTargetBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		renderTargetBarrier.Transition.pResource = m_renderTargets[frameIndex].Get();
		renderTargetBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		renderTargetBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		renderTargetBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		m_commandList->ResourceBarrier(1, &renderTargetBarrier);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		rtvHandle.ptr = rtvHandle.ptr + (frameIndex * m_rtvDescriptorSize);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

		// Record commands.
		const float clearColor[] = { 0.05f, 0.05f, 0.075f, 1.0f };
		m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		Microsoft::WRL::ComPtr<ID3D12Resource> cameraMatrixBuffer = static_cast<DX12GraphicsBuffer*>(m_cameraMatrixBuffer.get())->m_buffer.Get();
		m_commandList->SetGraphicsRootConstantBufferView(2, cameraMatrixBuffer->GetGPUVirtualAddress());

		UpdateLights();

		ID3D12DescriptorHeap* ppHeaps[] = { m_lightsHeap.Get() };
		m_commandList->SetDescriptorHeaps(1, ppHeaps);
		D3D12_GPU_DESCRIPTOR_HANDLE d{ m_lightsHeap->GetGPUDescriptorHandleForHeapStart() };
		m_commandList->SetGraphicsRootDescriptorTable(3, d);

		rmt_ScopedCPUSample(SubmitGraphicsElements, 0);
		for (const auto& elem: m_graphicsElements)
		{
			const Mesh* mesh = elem.GetMesh();

			// At this point, mesh should be already uploaded.
			Assert(mesh->IsUploaded());

			const DX12GraphicsBuffer* const vertexBuffer = static_cast<const DX12GraphicsBuffer* const>(mesh->GetVertexBuffer());
			const DX12GraphicsBuffer* const indexBuffer = static_cast<const DX12GraphicsBuffer* const>(mesh->GetIndexBuffer());

			D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
			// Initialize the vertex buffer view.
			vertexBufferView.BufferLocation = vertexBuffer->m_buffer->GetGPUVirtualAddress();
			vertexBufferView.StrideInBytes = sizeof(Vertex);
			vertexBufferView.SizeInBytes = vertexBuffer->GetDescription().size;

			D3D12_INDEX_BUFFER_VIEW indexBufferView;
			indexBufferView.BufferLocation = indexBuffer->m_buffer->GetGPUVirtualAddress();
			indexBufferView.Format = DXGI_FORMAT_R32_UINT;
			indexBufferView.SizeInBytes = indexBuffer->GetDescription().size;

			m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			m_commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			m_commandList->IASetIndexBuffer(&indexBufferView);

			DirectX::XMMATRIX normalMatrix = elem.GetTransform();
			normalMatrix = DirectX::XMMatrixInverse(nullptr, normalMatrix);
			normalMatrix = DirectX::XMMatrixTranspose(normalMatrix);
				
			struct InstanceMatrices
			{
				DirectX::XMFLOAT4X4 modelMatrix;
				DirectX::XMFLOAT4X4 normalMatrix;
			};

			InstanceMatrices instanceMatrices;
			DirectX::XMStoreFloat4x4(&instanceMatrices.modelMatrix, elem.GetTransform());
			DirectX::XMStoreFloat4x4(&instanceMatrices.normalMatrix, normalMatrix);

			m_commandList->SetGraphicsRoot32BitConstants(0, sizeof(InstanceMatrices) / 4, &instanceMatrices, 0);
			m_commandList->SetGraphicsRoot32BitConstants(1, sizeof(dVec4) / 4, &elem.GetMaterial()->m_baseColor, 0);

			dU32 indexCount = indexBuffer->GetDescription().size / (sizeof(dU32));
			m_commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
		}

		m_commandList->SetDescriptorHeaps(1, m_imguiHeap.GetAddressOf());
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

		// Indicate that the back buffer will now be used to present.
		D3D12_RESOURCE_BARRIER presentBarrier;
		presentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		presentBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		presentBarrier.Transition.pResource = m_renderTargets[frameIndex].Get();
		presentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		presentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		presentBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		m_commandList->ResourceBarrier(1, &presentBarrier);

		ThrowIfFailed(m_commandList->Close());
	}

	void DX12GraphicsRenderer::UpdateLights()
	{
		if (m_pointLights.empty())
		{
			D3D12_CPU_DESCRIPTOR_HANDLE d{ m_lightsHeap->GetCPUDescriptorHandleForHeapStart() };
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			ZeroMemory(&srvDesc, sizeof(srvDesc));
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = static_cast<UINT>(m_pointLights.size());
			srvDesc.Buffer.StructureByteStride = static_cast<UINT>(sizeof(PointLight));
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			m_device->CreateShaderResourceView(0, &srvDesc, d);
			return;
		}
		
		if (m_pointLights.size() > m_lightsBuffer->GetDescription().size / sizeof(PointLight))
		{
			CreateLightsBuffer((dU32)(m_pointLights.size() * sizeof(PointLight)));
		}

		DX12GraphicsBuffer* lightBuffer = static_cast<DX12GraphicsBuffer*>(m_lightsBuffer.get());

		UINT8* pDataBegin;
		D3D12_RANGE readRange;
		readRange.Begin = 0;
		readRange.End = 0;
		ThrowIfFailed(lightBuffer->m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&pDataBegin)));
		memcpy(pDataBegin, m_pointLights.data(), m_pointLights.size() * sizeof(PointLight));
		lightBuffer->m_buffer->Unmap(0, nullptr);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = static_cast<UINT>(m_pointLights.size());
		srvDesc.Buffer.StructureByteStride = static_cast<UINT>(sizeof(PointLight));
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		D3D12_CPU_DESCRIPTOR_HANDLE d{ m_lightsHeap->GetCPUDescriptorHandleForHeapStart() };
		m_device->CreateShaderResourceView(lightBuffer->m_buffer.Get(), &srvDesc, d);
	}

}