// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "ThreadManager.h"

DWORD WINAPI DDProc(_In_ void* Param);			// WINAPI��__stdcall, �������������ҵ�����ջ������DDProc��<DesktopDuplication.cpp>�б�����

THREADMANAGER::THREADMANAGER() : m_ThreadCount(0),
                                 m_ThreadHandles(nullptr),
                                 m_ThreadData(nullptr)
{
    RtlZeroMemory(&m_PtrInfo, sizeof(m_PtrInfo));	// Windows�µ�memset
}

THREADMANAGER::~THREADMANAGER()
{
    Clean();		// ����this->Clean()�ͷ��ڴ���߳�
}

//
// Clean up resources
// ������Դ(�̺߳��ڴ�)
//
void THREADMANAGER::Clean()
{
    if (m_PtrInfo.PtrShapeBuffer)
    {
        delete [] m_PtrInfo.PtrShapeBuffer;
        m_PtrInfo.PtrShapeBuffer = nullptr;
    }
    RtlZeroMemory(&m_PtrInfo, sizeof(m_PtrInfo));	// ** ���m_PtrInfo, ���ָ����Ϣ

    if (m_ThreadHandles)
    {
        for (UINT i = 0; i < m_ThreadCount; ++i)
        {
            if (m_ThreadHandles[i])
            {
                CloseHandle(m_ThreadHandles[i]);			// �رյ����߳̾��
            }
        }
        delete [] m_ThreadHandles;
        m_ThreadHandles = nullptr;
    }																			// ** ���m_ThreadHandles, �߳̾��

    if (m_ThreadData)
    {
        for (UINT i = 0; i < m_ThreadCount; ++i)
        {
            CleanDx(&m_ThreadData[i].DxRes);			// ʹ��this->CleanDx()�������̵߳�D3D��Դ��CleanDx()�����涨��
        }
        delete [] m_ThreadData;
        m_ThreadData = nullptr;
    }																			// ** ���m_ThreadData, �߳�����

    m_ThreadCount = 0;											// ** �߳�����0
}

//
// Clean up DX_RESOURCES
// ����߳���������m_ThreadData[i]�е�D3D��Դ
//
void THREADMANAGER::CleanDx(_Inout_ DX_RESOURCES* Data)
{
    if (Data->Device)
    {
        Data->Device->Release();
        Data->Device = nullptr;
    }

    if (Data->Context)
    {
        Data->Context->Release();
        Data->Context = nullptr;
    }

    if (Data->VertexShader)
    {
        Data->VertexShader->Release();
        Data->VertexShader = nullptr;
    }

    if (Data->PixelShader)
    {
        Data->PixelShader->Release();
        Data->PixelShader = nullptr;
    }

    if (Data->InputLayout)
    {
        Data->InputLayout->Release();
        Data->InputLayout = nullptr;
    }

    if (Data->SamplerLinear)
    {
        Data->SamplerLinear->Release();
        Data->SamplerLinear = nullptr;
    }
}

//
// Start up threads for DDA
// ����������߳�
// ���� :	INT				SingleOutput,					---- �������
//				UINT			OutputCount,					---- �����, �߳���
//				HANDLE		UnexpextedErrorEvent,	---- ��Ԥ���쳣
//				HANDLE		ExpetcedErrorEvent,		---- Ԥ���쳣
//				HANDLE		TerminateThreadsEvent,	---- �߳���ֹ
//				HANDLE		SharedHandle,					---- ������
//				_In_ RECT*	DesktopDim					---- �������(left, top, width, height)
//
DUPL_RETURN THREADMANAGER::Initialize(INT SingleOutput, UINT OutputCount, HANDLE UnexpectedErrorEvent, HANDLE ExpectedErrorEvent, HANDLE TerminateThreadsEvent, HANDLE SharedHandle, _In_ RECT* DesktopDim)
{
    m_ThreadCount = OutputCount;
    m_ThreadHandles = new (std::nothrow) HANDLE[m_ThreadCount];			// std::nothrow��new�ڴ治��ʱ���׳��쳣���ǽ�ָ��ֵNULL
    m_ThreadData = new (std::nothrow) THREAD_DATA[m_ThreadCount];		// ** ��ʼ���ڴ�
    if (!m_ThreadHandles || !m_ThreadData)
    {
        return ProcessFailure(nullptr, L"Failed to allocate array for threads", L"Error", E_OUTOFMEMORY);
    }

    // Create appropriate # of threads for duplication
	// �����߳���m_ThreadCount, �����߳���������m_ThreadData[]���߳̾������m_ThreadHandles[]
    DUPL_RETURN Ret = DUPL_RETURN_SUCCESS;
    for (UINT i = 0; i < m_ThreadCount; ++i)
    {
        m_ThreadData[i].UnexpectedErrorEvent = UnexpectedErrorEvent;
        m_ThreadData[i].ExpectedErrorEvent = ExpectedErrorEvent;
        m_ThreadData[i].TerminateThreadsEvent = TerminateThreadsEvent;
        m_ThreadData[i].Output = (SingleOutput < 0) ? i : SingleOutput;
        m_ThreadData[i].TexSharedHandle = SharedHandle;
        m_ThreadData[i].OffsetX = DesktopDim->left;
        m_ThreadData[i].OffsetY = DesktopDim->top;
        m_ThreadData[i].PtrInfo = &m_PtrInfo;

        RtlZeroMemory(&m_ThreadData[i].DxRes, sizeof(DX_RESOURCES));
        Ret = InitializeDx(&m_ThreadData[i].DxRes);											// ����this->InitializeDx()��ʼ��D3D��Դ, �����涨��
        if (Ret != DUPL_RETURN_SUCCESS)
        {
            return Ret;
        }

        DWORD ThreadId;
        m_ThreadHandles[i] = CreateThread(nullptr, 0, DDProc, &m_ThreadData[i], 0, &ThreadId);	// �����߳̾���������, CreateThread()������<Windows.h>�У�����ԭ������ :
																																						// --- ���� :	_In_opt_		LPSECURITY_ATTRIBUTES			...,		---- ����ܷ񱻼̳�, Ĭ�ϲ��̳�
																																						// 					_In_				SIZE_T										...,		---- ���Ϊ0, �߳�ʹ��Ĭ�ϴ�С��ջ
																																						// 					_In_				LPTHREAD_START_ROUTINE	...,		---- �̵߳���ʼ��ַ, ͨ���Ǻ���
																																						// 					_In_opt_		LPVOID									...,		---- ���ݸ��̵߳Ĳ���
																																						// 					_In_				DWORD									...,		---- ���Ϊ0, �߳��ڴ�������������
																																						//					_Out_opt_		LPDWORD								...,		---- �߳̾��
        if (m_ThreadHandles[i] == nullptr)
        {
            return ProcessFailure(nullptr, L"Failed to create thread", L"Error", E_FAIL);
        }
    }

    return Ret;
}

//
// Get DX_RESOURCES
// ��ʼ��D3D��Դ
// --- ���� ��	�����豸��������ɫ�������벼�֡�������ɫ����������
//
DUPL_RETURN THREADMANAGER::InitializeDx(_Out_ DX_RESOURCES* Data)
{
    HRESULT hr = S_OK;

    // Driver types supported
	// ֧�ֵ���������
    D3D_DRIVER_TYPE DriverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT NumDriverTypes = ARRAYSIZE(DriverTypes);		// ARRAYSIZE������<Windows.h>, ���ڻ�ȡ�����С

    // Feature levels supported
	// D3D�ļ��ݰ汾
    D3D_FEATURE_LEVEL FeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1
    };
    UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);

    D3D_FEATURE_LEVEL FeatureLevel;

    // Create device
	// �����豸
    for (UINT DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex)
    {
        hr = D3D11CreateDevice(nullptr, DriverTypes[DriverTypeIndex], nullptr, 0, FeatureLevels, NumFeatureLevels,
                                D3D11_SDK_VERSION, &Data->Device, &FeatureLevel, &Data->Context);		// �����豸, D3D11CreateDevice()������<d3d11.h>, ����ԭ������ �� 
																																							// --- ���� :	_In_opt_		IDXGIAdapter*					...,		---- ��Ƶ������, ΪNULLʱ��ʹ��IDXGIFactory1ö�ٵĵ�һ��������	
																																							//										D3D_DRIVER_TYPE			...,		---- ��������
																																							//										HMODULE						...,		---- �����������Ϊ_SOFTWARE, ������ΪNULL
																																							//										UINT								...,		---- �����豸��FLAG, enum����, ���԰�λ��
																																							//					_In_opt_		D3D_FEATURE_LEVEL*		...,		---- ���ݰ汾����
																																							//										UINT								...,		---- ���ݰ汾��
																																							//										UINT								...,		---- SDK�汾, D3D11_SDK_VERSION
																																							//					_Out_opt_		ID3D11Device**				...,		---- �豸
																																							//					_Out_opt_		D3D_FEATURE_LEVEL*		...,		---- ��ǰ���ݰ汾
																																							//					_Out_opt_		ID3D11DeviceContext**	...		---- �豸������
        if (SUCCEEDED(hr))
        {
            // Device creation success, no need to loop anymore
			// �豸�Ѿ�ƥ�����������ͺ�D3D�汾���������ѭ��
            break;
        }
    }
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to create device in InitializeDx", L"Error", hr);
    }

    // VERTEX shader
	// ������ɫ��
    UINT Size = ARRAYSIZE(g_VS);
    hr = Data->Device->CreateVertexShader(g_VS, Size, nullptr, &Data->VertexShader);
    if (FAILED(hr))
    {
        return ProcessFailure(Data->Device, L"Failed to create vertex shader in InitializeDx", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    // Input layout
	// ���벼��
    D3D11_INPUT_ELEMENT_DESC Layout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    UINT NumElements = ARRAYSIZE(Layout);
    hr = Data->Device->CreateInputLayout(Layout, NumElements, g_VS, Size, &Data->InputLayout);
    if (FAILED(hr))
    {
        return ProcessFailure(Data->Device, L"Failed to create input layout in InitializeDx", L"Error", hr, SystemTransitionsExpectedErrors);
    }
    Data->Context->IASetInputLayout(Data->InputLayout);

    // Pixel shader
	// ������ɫ��
    Size = ARRAYSIZE(g_PS);
    hr = Data->Device->CreatePixelShader(g_PS, Size, nullptr, &Data->PixelShader);
    if (FAILED(hr))
    {
        return ProcessFailure(Data->Device, L"Failed to create pixel shader in InitializeDx", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    // Set up sampler
	// ȡ����
    D3D11_SAMPLER_DESC SampDesc;
    RtlZeroMemory(&SampDesc, sizeof(SampDesc));
    SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SampDesc.MinLOD = 0;
    SampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = Data->Device->CreateSamplerState(&SampDesc, &Data->SamplerLinear);
    if (FAILED(hr))
    {
        return ProcessFailure(Data->Device, L"Failed to create sampler state in InitializeDx", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    return DUPL_RETURN_SUCCESS;
}

//
// Getter for the PTR_INFO structure
// ��ȡ���ָ����Ϣ
//
PTR_INFO* THREADMANAGER::GetPointerInfo()
{
    return &m_PtrInfo;
}

//
// Waits infinitely for all spawned threads to terminate
// �ȴ���ֱ�����е��߳���ֹ
//
void THREADMANAGER::WaitForThreadTermination()
{
    if (m_ThreadCount != 0)
    {
        WaitForMultipleObjectsEx(m_ThreadCount, m_ThreadHandles, TRUE, INFINITE, FALSE);	// �ȴ��ں˶���WaitForMultipleObjectsEx(), ������<Windows.h>
																																					// --- ������ ��	bWaitAll = TRUE, 
																																					//						dwMilliSeconds = INFINITE,
																																					//						bAlert = FALSE
    }
}
