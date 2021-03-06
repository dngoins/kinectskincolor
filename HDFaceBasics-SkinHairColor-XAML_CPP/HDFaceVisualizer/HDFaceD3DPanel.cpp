//------------------------------------------------------------------------------
// <copyright file="HDFaceD3DPanel.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

//	This control is based on the following article: http://msdn.microsoft.com/en-us/library/windows/apps/hh825871.aspx
//	And the code here: http://code.msdn.microsoft.com/windowsapps/XAML-SwapChainPanel-00cb688b

#pragma once
#include "pch.h"

#pragma warning( disable:28204 )

#include "HDFaceD3DPanel.h"

using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace DirectX;
using namespace HDFaceVisualizer;
using namespace DX;

void HDFaceD3DPanel::CalculateNormals(DX::VertexPositionColorNormal* vertices, unsigned int verticesCount, unsigned int* indices, unsigned int indicesCount)
{
	unsigned int requiredTempDataLength = verticesCount * 2;

	if (faceNormalsCalculationDataLength != requiredTempDataLength)
	{
		if (faceNormalsCalculationData != nullptr)
		{
			_aligned_free(faceNormalsCalculationData);
		}

		// We need two temporary XMVECTORs for each vertex. These will be packed as position0, normal0, position1, normal1, ...
		size_t countRequired = requiredTempDataLength *sizeof(XMVECTOR);
		faceNormalsCalculationData = (XMVECTOR*)_aligned_malloc(countRequired, __alignof(XMVECTOR));

		if (nullptr == faceNormalsCalculationData)
		{
			return;
		}

		ZeroMemory(faceNormalsCalculationData, countRequired);

		faceNormalsCalculationDataLength = requiredTempDataLength;
	}

	// Load face tracking vertex locations
	for (unsigned int i = 0; i < verticesCount; i++)
	{
		XMFLOAT4 currectVector(vertices[i].pos);
		faceNormalsCalculationData[2 * i] = XMLoadFloat4(&currectVector);
	}

	// Compute sum of area vectors adjacent to each vertex. This results in an area-weighted average for the face normals.
	for (unsigned int i = 0; i < indicesCount; i+=3)
	{
		uint32 x = indices[i];
		uint32 y = indices[i+1];
		uint32 z = indices[i+2];

		const XMUINT3 &tri = XMUINT3(x, y, z);

		const XMVECTOR &xi = faceNormalsCalculationData[2 * tri.x];
		const XMVECTOR &xj = faceNormalsCalculationData[2 * tri.y];
		const XMVECTOR &xk = faceNormalsCalculationData[2 * tri.z];

		XMVECTOR faceNormal = XMVector3Cross(XMVectorSubtract(xi, xj), XMVectorSubtract(xi, xk));

		faceNormalsCalculationData[2 * tri.x + 1] = XMVectorAdd(faceNormalsCalculationData[2 * tri.x + 1], faceNormal);
		faceNormalsCalculationData[2 * tri.y + 1] = XMVectorAdd(faceNormalsCalculationData[2 * tri.y + 1], faceNormal);
		faceNormalsCalculationData[2 * tri.z + 1] = XMVectorAdd(faceNormalsCalculationData[2 * tri.z + 1], faceNormal);
	}

	FXMVECTOR vInitialColor = { 0.5f, 0.5f, 0.0f, 1.0f };
	// Normalize normals and store into the XMFLOAT3 data in the vertex buffer
	for (unsigned int i = 0; i < verticesCount; i++)
	{
		XMStoreFloat4(&vertices[i].pos, faceNormalsCalculationData[2 * i]);
		XMStoreFloat3(&vertices[i].normal, XMVector3Normalize(faceNormalsCalculationData[2 * i + 1]));
		XMStoreFloat4(&vertices[i].color, vInitialColor);
	}
}

void HDFaceD3DPanel::UpdateMesh(Windows::Foundation::Collections::IVectorView<WindowsPreview::Kinect::CameraSpacePoint>^ newVertices, Windows::Foundation::Collections::IVectorView<UINT>^ newIndices)
{
	bool dxBufferToChange = false;
	XMFLOAT3 currentNormal(0.0f, 0.0f, 0.0f);

	UINT newVerticesCount = newVertices->Size;

	if (newVerticesCount != this->modelVerticesCount)
	{
		dxBufferToChange = true;
		delete[] modelVertices;

		this->modelVerticesCount = newVerticesCount;
		this->modelVertices = new DX::VertexPositionColorNormal[this->modelVerticesCount];
	}

	for (UINT i = 0; i < this->modelVerticesCount; i++)
	{
		auto currentVertex = newVertices->GetAt(i);

		this->modelVertices[i] = { XMFLOAT4(currentVertex.X, currentVertex.Y, currentVertex.Z, 0.0f), currentNormal };
	}

	UINT newIndicesCount = newIndices->Size;
	
	if (newIndicesCount != this->modelIndicesCount)
	{
		dxBufferToChange = true;
		delete[] modelIndices;

		this->modelIndicesCount = newIndicesCount;
		this->modelIndices = new unsigned int[this->modelIndicesCount];

		for (UINT i = 0; i < this->modelIndicesCount; i++)
		{
			this->modelIndices[i] = newIndices->GetAt(i);
		}
	}

	CalculateNormals(this->modelVertices, this->modelVerticesCount, this->modelIndices, this->modelIndicesCount);

	if (dxBufferToChange)
	{
		this->CreateDXBuffers();
	}
	else
	{
		UpdateDXBuffers();
	}
}

HDFaceD3DPanel::HDFaceD3DPanel() :
    m_degreesPerSecond(0),
	modelVerticesCount(0),
	modelVertices(nullptr),
	modelIndicesCount(0),
	modelIndices(nullptr),	
	faceNormalsCalculationData(nullptr),
	faceNormalsCalculationDataLength(0),
	faceNormalsCalculationDataCalculated(false)
{
    critical_section::scoped_lock lock(m_criticalSection);
    CreateDeviceIndependentResources();
    CreateDeviceResources();
    CreateSizeDependentResources();
}

HDFaceD3DPanel::~HDFaceD3DPanel()
{
    m_renderLoopWorker->Cancel();

	if (nullptr != modelVertices)
		delete[] modelVertices;

	if (nullptr != modelIndices)
		delete[] modelIndices;

	

	if (faceNormalsCalculationData != nullptr)
		_aligned_free(faceNormalsCalculationData);


}

void HDFaceD3DPanel::StartRenderLoop()
{
    // If the animation render loop is already running then do not start another thread.
    if (m_renderLoopWorker != nullptr && m_renderLoopWorker->Status == AsyncStatus::Started)
    {
        return;
    }

    // Create a task that will be run on a background thread.
    auto workItemHandler = ref new WorkItemHandler([this](IAsyncAction ^ action)
    {
        // Calculate the updated frame and render once per vertical blanking interval.
        while (action->Status == AsyncStatus::Started)
        {
            m_timer.Tick([&]()
            {
                critical_section::scoped_lock lock(m_criticalSection);
                Render();
            });

            // Halt the thread until the next vblank is reached.
            // This ensures the app isn't updating and rendering faster than the display can refresh,
            // which would unnecessarily spend extra CPU and GPU resources.  This helps improve battery life.
            m_dxgiOutput->WaitForVBlank();
        }
    });
    
    // Run task on a dedicated high priority background thread.
    m_renderLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);
}

void HDFaceD3DPanel::StopRenderLoop()
{
    // Cancel the asynchronous task and let the render thread exit.
    m_renderLoopWorker->Cancel();
}

void HDFaceD3DPanel::Render()
{
	if (nullptr == modelVertices || nullptr == modelIndices)
	{
		return;
	}

    if (!m_loadingComplete || m_timer.GetFrameCount() == 0)
    {
        return;
    }

	buffersLock.lock();

	static const XMVECTORF32 eye = { 0.0f, 0.0f, -0.5f, 0.0f };
	static const XMVECTORF32 at = { 0.0f, 0.0f, 1.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

    // Convert degrees to radians, then convert seconds to rotation angle
    float radiansPerSecond = XMConvertToRadians(m_degreesPerSecond);
	double totalRotation = m_timer.GetTotalSeconds() * radiansPerSecond;
    float animRadians = (float) fmod(totalRotation, XM_2PI);

    // Prepare to pass the view matrix, and updated model matrix, to the shader
    XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtLH(eye, at, up)));
    XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(XMMatrixRotationY(animRadians)));

	static const FXMVECTOR scaling = { 1.0f, 1.0f, 0.05f, 1.0f };
	static const FXMVECTOR rotationOrgin = { 0.0f, 0.0f, 0.0f, 0.0f };
	static const FXMVECTOR rotationQuat = { 0.0f, 0.0f, 0.0f, 0.0f };
	static const XMVECTORF32 translation = { 0.0f, 0.0f, 0.0f, 0.0f };
	FXMVECTOR vInitColor = { 0.2f, 0.7f, 0.2f, 1.0f };

	XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(XMMatrixAffineTransformation(scaling, rotationOrgin, rotationQuat, translation)));

	//set the color to color the face
	if (m_colorIsSet)
	{
		FXMVECTOR vFaceColor = { m_faceColor.r, m_faceColor.g, m_faceColor.b, m_faceColor.a};
		XMStoreFloat4(&m_constantBufferData.vertexcolor, vFaceColor);
	}
	else
	{
		XMStoreFloat4(&m_constantBufferData.vertexcolor, vInitColor);
	}

    // Set render targets to the screen.
    ID3D11RenderTargetView *const targets[1] = { m_renderTargetView.Get() };
    m_d3dContext->OMSetRenderTargets(1, targets, m_depthStencilView.Get());

    // Clear the back buffer and depth stencil view.
    m_d3dContext->ClearRenderTargetView(m_renderTargetView.Get(), DirectX::Colors::White);
    m_d3dContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // Prepare the constant buffer to send it to the Graphics device.
    m_d3dContext->UpdateSubresource(m_constantBuffer.Get(), 0, NULL, &m_constantBufferData, 0, 0);

	//D3D11_MAPPED_SUBRESOURCE mappedResource;
	//PixelShaderBufferType* dataPtr;
	//unsigned int bufferNumber;

	//// Lock the constant buffer so it can be written to.
	//HRESULT result = m_d3dContext->Map(m_pixelShaderBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	//

	//// Get a pointer to the data in the constant buffer.
	//dataPtr = (PixelShaderBufferType*)mappedResource.pData;

	//// Copy the matrices into the constant buffer.
	//dataPtr->color.x = 0.5f;
	//dataPtr->color.y = 0.5f;
	//dataPtr->color.z = 0.0f;
	//dataPtr->color.w = 1.0f;
	//
	//// Unlock the constant buffer.
	//m_d3dContext->Unmap(m_pixelShaderBuffer.Get(), 0);

	//// Set the position of the constant buffer in the vertex shader.
	//bufferNumber = 0;

	//// Finanly set the constant buffer in the vertex shader with the updated values.
	//m_d3dContext->VSSetConstantBuffers(bufferNumber, 1, m_pixelShaderBuffer.GetAddressOf());

    // Each vertex is one instance of the VertexPositionColor struct.
    UINT stride = sizeof(VertexPositionColorNormal);
    UINT offset = 0;
    m_d3dContext->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

    m_d3dContext->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	//TO Use a wire frame just un-comment this line
	// Attach the WireFrame Rasterizer
	//m_d3dContext->RSSetState(m_WireFrameRS.Get());

    m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_d3dContext->IASetInputLayout(m_inputLayout.Get());

    // Attach our vertex shader.
    m_d3dContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);

    // Send the constant buffer to the Graphics device.
    m_d3dContext->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

    // Attach our pixel shader.
    m_d3dContext->PSSetShader(m_pixelShader.Get(), nullptr, 0);

	
    // Draw the objects.
    m_d3dContext->DrawIndexed(this->modelIndicesCount, 0, 0);

    Present();

	buffersLock.unlock();
}

void HDFaceD3DPanel::UpdateDXBuffers()
{
	if (!m_loadingComplete || m_timer.GetFrameCount() == 0)
	{
		return;
	}

	buffersLock.lock();

	D3D11_MAPPED_SUBRESOURCE vertexBufferMappedResource;
	ZeroMemory(&vertexBufferMappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));

	ThrowIfFailed(
		m_d3dContext->Map(m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &vertexBufferMappedResource)
		);
	//	Update the vertex buffer here.
	auto verteciesSize = sizeof(DX::VertexPositionColorNormal) * this->modelVerticesCount;
	memcpy(vertexBufferMappedResource.pData, modelVertices, verteciesSize);
	//	Reenable GPU access to the vertex buffer data.
	m_d3dContext->Unmap(m_vertexBuffer.Get(), 0);

	//	Disable GPU access to the index buffer data.
	D3D11_MAPPED_SUBRESOURCE indexBufferMappedResource;
	ZeroMemory(&indexBufferMappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));

	ThrowIfFailed(
		m_d3dContext->Map(m_indexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &indexBufferMappedResource)
		);
	//	Update the index buffer here.
	auto indicesSize = this->modelIndicesCount * sizeof(unsigned int);
	memcpy(indexBufferMappedResource.pData, modelIndices, indicesSize);
	//	Reenable GPU access to the index buffer data.
	m_d3dContext->Unmap(m_indexBuffer.Get(), 0);

	////update the pixel shader buffer
	//D3D11_MAPPED_SUBRESOURCE pixelShaderBufferMappedResource;
	//ZeroMemory(&pixelShaderBufferMappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));

	//ThrowIfFailed(
	//	m_d3dContext->Map(m_pixelShaderBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &pixelShaderBufferMappedResource)
	//	);

	buffersLock.unlock();
}

void HDFaceD3DPanel::CreateDXBuffers()
{
	if (nullptr == modelVertices || nullptr == modelIndices)
	{
		return;
	}

	// Load mesh vertices. Each vertex has a position and a color.

	m_vertexBuffer = nullptr;
	
	D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
	vertexBufferData.pSysMem = modelVertices;
	vertexBufferData.SysMemPitch = 0;
	vertexBufferData.SysMemSlicePitch = 0;

	UINT modelVerticesSize = this->modelVerticesCount * sizeof(VertexPositionColorNormal);
	CD3D11_BUFFER_DESC vertexBufferDesc(modelVerticesSize, D3D11_BIND_VERTEX_BUFFER);
	vertexBufferDesc.ByteWidth = modelVerticesSize;
	vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	ThrowIfFailed(
		m_d3dDevice->CreateBuffer(
		&vertexBufferDesc,
		&vertexBufferData,
		&m_vertexBuffer
		)
		);

	m_indexBuffer = nullptr;

	D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
	indexBufferData.pSysMem = modelIndices;
	indexBufferData.SysMemPitch = 0;
	indexBufferData.SysMemSlicePitch = 0;

	UINT modelIndicesSize = this->modelIndicesCount * sizeof(unsigned int);

	CD3D11_BUFFER_DESC indexBufferDesc(modelIndicesSize, D3D11_BIND_INDEX_BUFFER);
	indexBufferDesc.ByteWidth = modelIndicesSize;
	indexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	ThrowIfFailed(
		m_d3dDevice->CreateBuffer(
		&indexBufferDesc,
		&indexBufferData,
		&m_indexBuffer
		)
		);

	

}

void HDFaceD3DPanel::CreateDeviceResources()
{
    DirectXPanelBase::CreateDeviceResources();

    // Retrieve DXGIOutput representing the main adapter output.
    ComPtr<IDXGIFactory1> dxgiFactory;
    ThrowIfFailed(
        CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory))
        );

    ComPtr<IDXGIAdapter> dxgiAdapter;
    ThrowIfFailed(
        dxgiFactory->EnumAdapters(0, &dxgiAdapter)
        );

    ThrowIfFailed(
        dxgiAdapter->EnumOutputs(0, &m_dxgiOutput)
        );

	
    // Asynchronously load vertex shader and create input layout.
    auto loadVSTask = DX::ReadDataAsync(L"HDFaceVisualizer\\HDFaceVertexShader.cso");
    auto createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData) {
        ThrowIfFailed(
            m_d3dDevice->CreateVertexShader(
            &fileData[0],
            fileData.size(),
            nullptr,
            &m_vertexShader
            )
            );

        static const D3D11_INPUT_ELEMENT_DESC vertexDesc [] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        ThrowIfFailed(
            m_d3dDevice->CreateInputLayout(
            vertexDesc,
            ARRAYSIZE(vertexDesc),
            &fileData[0],
            fileData.size(),
            &m_inputLayout
            )
            );
    });

	
    // Asynchronously load vertex shader and create constant buffer.
    auto loadPSTask = DX::ReadDataAsync(L"HDFaceVisualizer\\HDFacePixelShader.cso");
    auto createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData) {
        ThrowIfFailed(
            m_d3dDevice->CreatePixelShader(
            &fileData[0],
            fileData.size(),
            nullptr,
            &m_pixelShader
            )
            );

		/*CD3D11_BUFFER_DESC pixelBufferDesc(sizeof(PixelShaderBuffer), D3D11_BIND_CONSTANT_BUFFER);
		ThrowIfFailed(
			m_d3dDevice->CreateBuffer(
			&pixelBufferDesc,
			nullptr,
			&m_pixelShaderBuffer
			)
			);*/
		
        CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
        ThrowIfFailed(
            m_d3dDevice->CreateBuffer(
            &constantBufferDesc,
            nullptr,
            &m_constantBuffer
            )
            );
    });

    // Once both shaders are loaded, create the mesh.
	auto createCubeTask = (createPSTask && createVSTask).then([this]() {
		CreateDXBuffers();
	});

    // Once the cube is loaded, the object is ready to be rendered.
    createCubeTask.then([this]() {
        m_loadingComplete = true;
    });
}

void HDFaceD3DPanel::CreateSizeDependentResources()
{
    m_renderTargetView = nullptr;
    m_depthStencilView = nullptr;

    DirectXPanelBase::CreateSizeDependentResources();

    // Create a render target view of the swap chain back buffer.
    ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(
        m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
        );

    // Create render target view.
    ThrowIfFailed(
        m_d3dDevice->CreateRenderTargetView(
        backBuffer.Get(),
        nullptr,
        &m_renderTargetView)
        );
	
	
    // Create and set viewport.
    D3D11_VIEWPORT viewport = CD3D11_VIEWPORT(
        0.0f,
        0.0f,
        m_renderTargetWidth,
        m_renderTargetHeight
        );

    m_d3dContext->RSSetViewports(1, &viewport);

    // Create depth/stencil buffer descriptor.
    CD3D11_TEXTURE2D_DESC depthStencilDesc(
        DXGI_FORMAT_D24_UNORM_S8_UINT,
        static_cast<UINT>(m_renderTargetWidth),
        static_cast<UINT>(m_renderTargetHeight),
        1,
        1,
        D3D11_BIND_DEPTH_STENCIL
        );

    // Allocate a 2-D surface as the depth/stencil buffer.
    ComPtr<ID3D11Texture2D> depthStencil;
    ThrowIfFailed(
        m_d3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, &depthStencil)
        );

    // Create depth/stencil view based on depth/stencil buffer.
    const CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION_TEXTURE2D);
	
    ThrowIfFailed(
        m_d3dDevice->CreateDepthStencilView(
        depthStencil.Get(),
        &depthStencilViewDesc,
        &m_depthStencilView
        )
        );

	
    float aspectRatio = m_renderTargetWidth / m_renderTargetHeight;

	float cameraFovAngleY = XMConvertToRadians(70.0f);
    if (aspectRatio < 1.0f)
    {
		cameraFovAngleY /= aspectRatio;
    }

    XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovLH(
		cameraFovAngleY,
        aspectRatio,
        0.01f,
        100.0f
        );

	XMMATRIX orientationMatrix = XMMatrixIdentity();

    XMStoreFloat4x4(
        &m_constantBufferData.projection,
        XMMatrixTranspose(perspectiveMatrix * orientationMatrix)
        );

	InitRenderState();
}

void HDFaceD3DPanel::SetFaceColor(Windows::UI::Color color)
{
	m_colorIsSet = true;
	m_faceColor = DX::ConvertToColorF(color);
}

void HDFaceD3DPanel::InitRenderState()
{
	D3D11_RASTERIZER_DESC wfd;
	ZeroMemory(&wfd, sizeof(D3D11_RASTERIZER_DESC));
	wfd.FillMode = D3D11_FILL_WIREFRAME;
	wfd.CullMode = D3D11_CULL_NONE;
	
	//to enable clipping
	wfd.DepthClipEnable = true;

	m_d3dDevice->CreateRasterizerState(&wfd, m_WireFrameRS.GetAddressOf());

}