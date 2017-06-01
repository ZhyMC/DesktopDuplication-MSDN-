// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "DisplayManager.h"
using namespace DirectX;

//
// Constructor NULLs out vars
// ��ʼ���б��캯��
//
DISPLAYMANAGER::DISPLAYMANAGER() : m_Device(nullptr),
                                   m_DeviceContext(nullptr),
                                   m_MoveSurf(nullptr),
                                   m_VertexShader(nullptr),
                                   m_PixelShader(nullptr),
                                   m_InputLayout(nullptr),
                                   m_RTV(nullptr),
                                   m_SamplerLinear(nullptr),
                                   m_DirtyVertexBufferAlloc(nullptr),
                                   m_DirtyVertexBufferAllocSize(0)
{
}

//
// Destructor calls CleanRefs to destroy everything
// ������ü���������
//
DISPLAYMANAGER::~DISPLAYMANAGER()
{
    CleanRefs();

    if (m_DirtyVertexBufferAlloc)
    {
        delete [] m_DirtyVertexBufferAlloc;
        m_DirtyVertexBufferAlloc = nullptr;
    }
}

//
// Initialize D3D variables
// ��ʼ��D3D����
//
void DISPLAYMANAGER::InitD3D(DX_RESOURCES* Data)		// DX_RESOURCES������<CommonTypes.h>��
{
    m_Device = Data->Device;
    m_DeviceContext = Data->Context;
    m_VertexShader = Data->VertexShader;
    m_PixelShader = Data->PixelShader;
    m_InputLayout = Data->InputLayout;
    m_SamplerLinear = Data->SamplerLinear;

    m_Device->AddRef();					// �������ü���
    m_DeviceContext->AddRef();
    m_VertexShader->AddRef();
    m_PixelShader->AddRef();
    m_InputLayout->AddRef();
    m_SamplerLinear->AddRef();
}

//
// Process a given frame and its metadata
// ����������Data(FRAME_DATA*����, ����2D����DXGI_OUTDUPL_FRAME_INFO��Ԫ����)�������֡
// --- ˵��:	��Ҫ�õ���DISPLAYMANAGER::CopyMove()��CopyDirty()����private����
//
DUPL_RETURN DISPLAYMANAGER::ProcessFrame(_In_ FRAME_DATA* Data, _Inout_ ID3D11Texture2D* SharedSurf, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc)
{
    DUPL_RETURN Ret = DUPL_RETURN_SUCCESS;

    // Process dirties and moves
	// ������������ƶ�����
    if (Data->FrameInfo.TotalMetadataBufferSize)
    {
        D3D11_TEXTURE2D_DESC Desc;
        Data->Frame->GetDesc(&Desc);	// ID3D11Texture2D::GetDesc(), ��ȡTexture2D��������Ϣ(����)

        if (Data->MoveCount)
        {
            Ret = CopyMove(SharedSurf, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(Data->MetaData), Data->MoveCount, OffsetX, OffsetY, DeskDesc, Desc.Width, Desc.Height);	// DISPLAYMANAGER::CopyMove()������<DisplayManager.h>��, ���ڱ��ļ��в鿴������
            if (Ret != DUPL_RETURN_SUCCESS)
            {
                return Ret;
            }
        }

        if (Data->DirtyCount)
        {
            Ret = CopyDirty(Data->Frame, SharedSurf, reinterpret_cast<RECT*>(Data->MetaData + (Data->MoveCount * sizeof(DXGI_OUTDUPL_MOVE_RECT))), Data->DirtyCount, OffsetX, OffsetY, DeskDesc);	// DISPLAYMANAGER::CopyDirty()������<DisplayManager.h>��, ���ڱ��ļ��в鿴������
        }
    }

    return Ret;
}

//
// Returns D3D device being used
// ��������ʹ�õ�D3D�豸
//
ID3D11Device* DISPLAYMANAGER::GetDevice()
{
    return m_Device;
}

//
// Set appropriate source and destination rects for move rects
// ���ݳ������Ϣ, ���ú��ʵ�Դ�ƶ����κ�Ŀ���ƶ�����
//
void DISPLAYMANAGER::SetMoveRect(_Out_ RECT* SrcRect, _Out_ RECT* DestRect, _In_ DXGI_OUTPUT_DESC* DeskDesc, _In_ DXGI_OUTDUPL_MOVE_RECT* MoveRect, INT TexWidth, INT TexHeight)
{
    switch (DeskDesc->Rotation)
    {
        case DXGI_MODE_ROTATION_UNSPECIFIED:	// δ�ӹ涨�ĳ���
        case DXGI_MODE_ROTATION_IDENTITY:			// Ĭ�ϳ���
        {
            SrcRect->left = MoveRect->SourcePoint.x;
            SrcRect->top = MoveRect->SourcePoint.y;
            SrcRect->right = MoveRect->SourcePoint.x + MoveRect->DestinationRect.right - MoveRect->DestinationRect.left;
            SrcRect->bottom = MoveRect->SourcePoint.y + MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top;

            *DestRect = MoveRect->DestinationRect;
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE90:		// ��ת90��
        {
            SrcRect->left = TexHeight - (MoveRect->SourcePoint.y + MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top);
            SrcRect->top = MoveRect->SourcePoint.x;
            SrcRect->right = TexHeight - MoveRect->SourcePoint.y;
            SrcRect->bottom = MoveRect->SourcePoint.x + MoveRect->DestinationRect.right - MoveRect->DestinationRect.left;

            DestRect->left = TexHeight - MoveRect->DestinationRect.bottom;
            DestRect->top = MoveRect->DestinationRect.left;
            DestRect->right = TexHeight - MoveRect->DestinationRect.top;
            DestRect->bottom = MoveRect->DestinationRect.right;
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE180:		// ��ת180��
        {
            SrcRect->left = TexWidth - (MoveRect->SourcePoint.x + MoveRect->DestinationRect.right - MoveRect->DestinationRect.left);
            SrcRect->top = TexHeight - (MoveRect->SourcePoint.y + MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top);
            SrcRect->right = TexWidth - MoveRect->SourcePoint.x;
            SrcRect->bottom = TexHeight - MoveRect->SourcePoint.y;

            DestRect->left = TexWidth - MoveRect->DestinationRect.right;
            DestRect->top = TexHeight - MoveRect->DestinationRect.bottom;
            DestRect->right = TexWidth - MoveRect->DestinationRect.left;
            DestRect->bottom =  TexHeight - MoveRect->DestinationRect.top;
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE270:		// ��ת270��
        {
            SrcRect->left = MoveRect->SourcePoint.x;
            SrcRect->top = TexWidth - (MoveRect->SourcePoint.x + MoveRect->DestinationRect.right - MoveRect->DestinationRect.left);
            SrcRect->right = MoveRect->SourcePoint.y + MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top;
            SrcRect->bottom = TexWidth - MoveRect->SourcePoint.x;

            DestRect->left = MoveRect->DestinationRect.top;
            DestRect->top = TexWidth - MoveRect->DestinationRect.right;
            DestRect->right = MoveRect->DestinationRect.bottom;
            DestRect->bottom =  TexWidth - MoveRect->DestinationRect.left;
            break;
        }
        default:
        {
            RtlZeroMemory(DestRect, sizeof(RECT));
            RtlZeroMemory(SrcRect, sizeof(RECT));
            break;
        }
    }
}

//
// Copy move rectangles
// �����ƶ�����
//
DUPL_RETURN DISPLAYMANAGER::CopyMove(_Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(MoveCount) DXGI_OUTDUPL_MOVE_RECT* MoveBuffer, UINT MoveCount, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc, INT TexWidth, INT TexHeight)
{
    D3D11_TEXTURE2D_DESC FullDesc;
    SharedSurf->GetDesc(&FullDesc);	// FullDesc����SharedSurf��������Ϣ(����)

    // Make new intermediate surface to copy into for moving
	// Ϊ�����ƶ����� �����µ��м��
    if (!m_MoveSurf)
    {
        D3D11_TEXTURE2D_DESC MoveDesc;
        MoveDesc = FullDesc;
        MoveDesc.Width = DeskDesc->DesktopCoordinates.right - DeskDesc->DesktopCoordinates.left;
        MoveDesc.Height = DeskDesc->DesktopCoordinates.bottom - DeskDesc->DesktopCoordinates.top;
        MoveDesc.BindFlags = D3D11_BIND_RENDER_TARGET;	// D3D11_BIND_FLAG, ����󶨵��Կ����ߵķ�ʽ, �ο�"�Կ���Ⱦ��ˮ��"
        MoveDesc.MiscFlags = 0;
        HRESULT hr = m_Device->CreateTexture2D(&MoveDesc, nullptr, &m_MoveSurf);	// ID3D11Device::CreateTexture2D(), ����Texture2D��m_MoveSurf

        if (FAILED(hr))
        {
            return ProcessFailure(m_Device, L"Failed to create staging texture for move rects", L"Error", hr, SystemTransitionsExpectedErrors);
        }
    }

	// ��ʼ����
    for (UINT i = 0; i < MoveCount; ++i)
    {
        RECT SrcRect;
        RECT DestRect;

        SetMoveRect(&SrcRect, &DestRect, DeskDesc, &(MoveBuffer[i]), TexWidth, TexHeight);	// DISPLAYMANAGER::SetMoveRect(), ��MoveBuffer������DeskDesc��ʼ��SrcRect��DestRect

        // Copy rect out of shared surface
		// �ӹ�������и���rect
        D3D11_BOX Box;		// D3D11����ģ��
        Box.left = SrcRect.left + DeskDesc->DesktopCoordinates.left - OffsetX;
        Box.top = SrcRect.top + DeskDesc->DesktopCoordinates.top - OffsetY;
        Box.front = 0;
        Box.right = SrcRect.right + DeskDesc->DesktopCoordinates.left - OffsetX;
        Box.bottom = SrcRect.bottom + DeskDesc->DesktopCoordinates.top - OffsetY;
        Box.back = 1;
        m_DeviceContext->CopySubresourceRegion(m_MoveSurf, 0, SrcRect.left, SrcRect.top, 0, SharedSurf, 0, &Box);		// ID3D11DeviceContext::CopySubresourceRegion(), ����Դ��Դ��Ŀ����Դ������, ������Ϊ_in_
																																															// --- ����˵��:		m_MoveSurf				Ŀ����Դ, �ƶ�����(�м��)
																																															//							0								Ŀ������Դ���Ϊ0
																																															//							SrcRect.left				Ŀ�������x����
																																															//							SrcRect.top				Ŀ�������y����
																																															//							0								Ŀ�������z����
																																															//							SharedSurf				Դ��Դ, �������
																																															//							0								Դ����Դ�ı��Ϊ0
																																															//							&Box						�����Ƶķ�Χ����Box�ķ�Χ

        // Copy back to shared surface
		// ���ƻع������
        Box.left = SrcRect.left;
        Box.top = SrcRect.top;
        Box.front = 0;
        Box.right = SrcRect.right;
        Box.bottom = SrcRect.bottom;
        Box.back = 1;
        m_DeviceContext->CopySubresourceRegion(SharedSurf, 0, DestRect.left + DeskDesc->DesktopCoordinates.left - OffsetX, DestRect.top + DeskDesc->DesktopCoordinates.top - OffsetY, 0, m_MoveSurf, 0, &Box);
    }

    return DUPL_RETURN_SUCCESS;
}

//
// Sets up vertices for dirty rects for rotated desktops
// ��������εĶ���, ֧��������ת
//
#pragma warning(push)
#pragma warning(disable:__WARNING_USING_UNINIT_VAR) // false positives in SetDirtyVert due to tool bug

void DISPLAYMANAGER::SetDirtyVert(_Out_writes_(NUMVERTICES) VERTEX* Vertices, _In_ RECT* Dirty, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc, _In_ D3D11_TEXTURE2D_DESC* FullDesc, _In_ D3D11_TEXTURE2D_DESC* ThisDesc)	// VERTEX������<CommonTypes.h>��; ������������η�Χ��XYУ��������豸������2��Texture2D����(�ֱ�ΪFullDesc��ThisDesc)
{
    INT CenterX = FullDesc->Width / 2;	// ��Ļ����X
    INT CenterY = FullDesc->Height / 2;	// ��Ļ����Y

    INT Width = DeskDesc->DesktopCoordinates.right - DeskDesc->DesktopCoordinates.left;
    INT Height = DeskDesc->DesktopCoordinates.bottom - DeskDesc->DesktopCoordinates.top;

    // Rotation compensated destination rect
	// Ŀ�������
    RECT DestDirty = *Dirty;

    // Set appropriate coordinates compensated for rotation
    switch (DeskDesc->Rotation)
    {
        case DXGI_MODE_ROTATION_ROTATE90:		// ��ת90��
        {
            DestDirty.left = Width - Dirty->bottom;
            DestDirty.top = Dirty->left;
            DestDirty.right = Width - Dirty->top;
            DestDirty.bottom = Dirty->right;

            Vertices[0].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[1].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[2].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[5].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE180:		// ��ת180��
        {
            DestDirty.left = Width - Dirty->right;
            DestDirty.top = Height - Dirty->bottom;
            DestDirty.right = Width - Dirty->left;
            DestDirty.bottom = Height - Dirty->top;

            Vertices[0].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[1].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[2].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[5].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            break;
        }
        case DXGI_MODE_ROTATION_ROTATE270:		// ��ת270��
        {
            DestDirty.left = Dirty->top;
            DestDirty.top = Height - Dirty->right;
            DestDirty.right = Dirty->bottom;
            DestDirty.bottom = Height - Dirty->left;

            Vertices[0].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[1].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[2].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[5].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            break;
        }
        default:
            assert(false); // drop through
        case DXGI_MODE_ROTATION_UNSPECIFIED:	// δ����Ƕ�
        case DXGI_MODE_ROTATION_IDENTITY:			// Ĭ�� 
        {
			// Vertices[i].TexCoord����������2D��������
			// XMFLOAT2()������<DirectXMath.h>, ����2D����
            Vertices[0].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));	
            Vertices[1].TexCoord = XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[2].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
            Vertices[5].TexCoord = XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
            break;
        }
    }

    // Set positions
    Vertices[0].Pos = XMFLOAT3((DestDirty.left + DeskDesc->DesktopCoordinates.left - OffsetX - CenterX) / static_cast<FLOAT>(CenterX),
                             -1 * (DestDirty.bottom + DeskDesc->DesktopCoordinates.top - OffsetY - CenterY) / static_cast<FLOAT>(CenterY),
                             0.0f);
    Vertices[1].Pos = XMFLOAT3((DestDirty.left + DeskDesc->DesktopCoordinates.left - OffsetX - CenterX) / static_cast<FLOAT>(CenterX),
                             -1 * (DestDirty.top + DeskDesc->DesktopCoordinates.top - OffsetY - CenterY) / static_cast<FLOAT>(CenterY),
                             0.0f);
    Vertices[2].Pos = XMFLOAT3((DestDirty.right + DeskDesc->DesktopCoordinates.left - OffsetX - CenterX) / static_cast<FLOAT>(CenterX),
                             -1 * (DestDirty.bottom + DeskDesc->DesktopCoordinates.top - OffsetY - CenterY) / static_cast<FLOAT>(CenterY),
                             0.0f);
    Vertices[3].Pos = Vertices[2].Pos;
    Vertices[4].Pos = Vertices[1].Pos;
    Vertices[5].Pos = XMFLOAT3((DestDirty.right + DeskDesc->DesktopCoordinates.left - OffsetX - CenterX) / static_cast<FLOAT>(CenterX),
                             -1 * (DestDirty.top + DeskDesc->DesktopCoordinates.top - OffsetY - CenterY) / static_cast<FLOAT>(CenterY),
                             0.0f);

    Vertices[3].TexCoord = Vertices[2].TexCoord;
    Vertices[4].TexCoord = Vertices[1].TexCoord;
}

#pragma warning(pop) // re-enable __WARNING_USING_UNINIT_VAR

//
// Copies dirty rectangles
// ���������
//
DUPL_RETURN DISPLAYMANAGER::CopyDirty(_In_ ID3D11Texture2D* SrcSurface, _Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(DirtyCount) RECT* DirtyBuffer, UINT DirtyCount, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc)
{
    HRESULT hr;

    D3D11_TEXTURE2D_DESC FullDesc;
    SharedSurf->GetDesc(&FullDesc);

    D3D11_TEXTURE2D_DESC ThisDesc;
    SrcSurface->GetDesc(&ThisDesc);

    if (!m_RTV)
    {
        hr = m_Device->CreateRenderTargetView(SharedSurf, nullptr, &m_RTV);		// ID3D11Device::CreateRenderTargetView(), Ϊ��Դ���ݴ�����ȾĿ����ͼ
																																//	--- ԭ�� :	_in_		ID3D11Resouce*										---- Ҫ���Ƶ���Դ					
																																//					_in_		D3D11_RENDER_TARGET_VIEW_DESC*	---- ���ΪNULL, �����������Դ
																																//					_out_	ID3D11RenderTargetView**						---- ��ȾĿ����ͼ���׵�ַ
        if (FAILED(hr))
        {
            return ProcessFailure(m_Device, L"Failed to create render target view for dirty rects", L"Error", hr, SystemTransitionsExpectedErrors);
        }
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC ShaderDesc;							// ��ɫ������(����)
    ShaderDesc.Format = ThisDesc.Format;													// DXGI_FORMAT
    ShaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;	// ά��Ϊ2D
	ShaderDesc.Texture2D.MostDetailedMip = ThisDesc.MipLevels - 1;		// �趨mipmap�߽�ֵ(= mipmap���ֵ - 1), �μ�"mipmap"
    ShaderDesc.Texture2D.MipLevels = ThisDesc.MipLevels;						// �趨mipmap���ֵ

    // Create new shader resource view
	// �����µ���ɫ����Դ��ͼ
    ID3D11ShaderResourceView* ShaderResource = nullptr;
    hr = m_Device->CreateShaderResourceView(SrcSurface, &ShaderDesc, &ShaderResource);		// ID3D11Device::CreateShaderResourceView(), Ϊ��Դ���ݴ�����ɫ����Դ
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create shader resource view for dirty rects", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    FLOAT BlendFactor[4] = {0.f, 0.f, 0.f, 0.f};																						// ��ɫ����
    m_DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);									// ����OM(output-merger)�Ļ��״̬
    m_DeviceContext->OMSetRenderTargets(1, &m_RTV, nullptr);													// ����OM����ȾĿ��
    m_DeviceContext->VSSetShader(m_VertexShader, nullptr, 0);													// ���ö�����ɫ��
    m_DeviceContext->PSSetShader(m_PixelShader, nullptr, 0);														// ����������ɫ��
    m_DeviceContext->PSSetShaderResources(0, 1, &ShaderResource);											// ����������ɫ����Դ 
    m_DeviceContext->PSSetSamplers(0, 1, &m_SamplerLinear);														// ���ò�����
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);		// ���ײ�ԭ�����������������н���

    // Create space for vertices for the dirty rects if the current space isn't large enough
	// �������εĶ���ռ䲻��, ����Ҫ���¿��ٿռ�
    UINT BytesNeeded = sizeof(VERTEX) * NUMVERTICES * DirtyCount;
    if (BytesNeeded > m_DirtyVertexBufferAllocSize)
    {
        if (m_DirtyVertexBufferAlloc)
        {
            delete [] m_DirtyVertexBufferAlloc;
        }

        m_DirtyVertexBufferAlloc = new (std::nothrow) BYTE[BytesNeeded];
        if (!m_DirtyVertexBufferAlloc)
        {
            m_DirtyVertexBufferAllocSize = 0;
            return ProcessFailure(nullptr, L"Failed to allocate memory for dirty vertex buffer.", L"Error", E_OUTOFMEMORY);
        }

        m_DirtyVertexBufferAllocSize = BytesNeeded;
    }

    // Fill them in
	// �򿪱ٵĿռ�DirtyVertex�������
    VERTEX* DirtyVertex = reinterpret_cast<VERTEX*>(m_DirtyVertexBufferAlloc);
    for (UINT i = 0; i < DirtyCount; ++i, DirtyVertex += NUMVERTICES)
    {
        SetDirtyVert(DirtyVertex, &(DirtyBuffer[i]), OffsetX, OffsetY, DeskDesc, &FullDesc, &ThisDesc);
    }

    // Create vertex buffer
	// �������㻺��
    D3D11_BUFFER_DESC BufferDesc;
    RtlZeroMemory(&BufferDesc, sizeof(BufferDesc));
    BufferDesc.Usage = D3D11_USAGE_DEFAULT;
    BufferDesc.ByteWidth = BytesNeeded;
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;	// ���ߵİ�������VERTEX_BUFFER
    BufferDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    RtlZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = m_DirtyVertexBufferAlloc;

    ID3D11Buffer* VertBuf = nullptr;
    hr = m_Device->CreateBuffer(&BufferDesc, &InitData, &VertBuf);		// ����Id3D11Buffer*���� 
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to create vertex buffer in dirty rect processing", L"Error", hr, SystemTransitionsExpectedErrors);
    }
    UINT Stride = sizeof(VERTEX);
    UINT Offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, &VertBuf, &Stride, &Offset);	// �趨��������(IA)�Ķ��㻺��

    D3D11_VIEWPORT VP;
    VP.Width = static_cast<FLOAT>(FullDesc.Width);
    VP.Height = static_cast<FLOAT>(FullDesc.Height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;
    m_DeviceContext->RSSetViewports(1, &VP);												// �趨���ߵĹ�դ 

    m_DeviceContext->Draw(NUMVERTICES * DirtyCount, 0);							// ����ԭ��

    VertBuf->Release();
    VertBuf = nullptr;

    ShaderResource->Release();
    ShaderResource = nullptr;

    return DUPL_RETURN_SUCCESS;
}

//
// Clean all references
// ����������ü���
//
void DISPLAYMANAGER::CleanRefs()
{
    if (m_DeviceContext)
    {
        m_DeviceContext->Release();
        m_DeviceContext = nullptr;
    }

    if (m_Device)
    {
        m_Device->Release();
        m_Device = nullptr;
    }

    if (m_MoveSurf)
    {
        m_MoveSurf->Release();
        m_MoveSurf = nullptr;
    }

    if (m_VertexShader)
    {
        m_VertexShader->Release();
        m_VertexShader = nullptr;
    }

    if (m_PixelShader)
    {
        m_PixelShader->Release();
        m_PixelShader = nullptr;
    }

    if (m_InputLayout)
    {
        m_InputLayout->Release();
        m_InputLayout = nullptr;
    }

    if (m_SamplerLinear)
    {
        m_SamplerLinear->Release();
        m_SamplerLinear = nullptr;
    }

    if (m_RTV)
    {
        m_RTV->Release();
        m_RTV = nullptr;
    }
}
