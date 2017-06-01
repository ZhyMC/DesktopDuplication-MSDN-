// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "DuplicationManager.h"

//
// Constructor sets up references / variables
// ��ʼ���б��캯��
//
DUPLICATIONMANAGER::DUPLICATIONMANAGER() : m_DeskDupl(nullptr),
                                           m_AcquiredDesktopImage(nullptr),
                                           m_MetaDataBuffer(nullptr),
                                           m_MetaDataSize(0),
                                           m_OutputNumber(0),
                                           m_Device(nullptr)
{
    RtlZeroMemory(&m_OutputDesc, sizeof(m_OutputDesc));
}

//
// Destructor simply calls CleanRefs to destroy everything
// �������ü�������
// 
DUPLICATIONMANAGER::~DUPLICATIONMANAGER()
{
    if (m_DeskDupl)
    {
        m_DeskDupl->Release();		// Release()���豸ָ�����еĺ���
        m_DeskDupl = nullptr;
    }

    if (m_AcquiredDesktopImage)
    {
        m_AcquiredDesktopImage->Release();
        m_AcquiredDesktopImage = nullptr;
    }

    if (m_MetaDataBuffer)
    {
        delete [] m_MetaDataBuffer;
        m_MetaDataBuffer = nullptr;
    }

    if (m_Device)
    {
        m_Device->Release();
        m_Device = nullptr;
    }
}

//
// Initialize duplication interfaces
// ��ʼ��������ӿ�
// --- ����: ��ʼ��m_Device, m_OutputDesc, m_DeskDupl(IDXGIOutputDuplication����)
//
DUPL_RETURN DUPLICATIONMANAGER::InitDupl(_In_ ID3D11Device* Device, UINT Output)
{
    m_OutputNumber = Output;

    // Take a reference on the device
	// ����˽�г�Աm_Device�����ü���
    m_Device = Device;
    m_Device->AddRef();

    // Get DXGI device
	// ��ȡDXGI�豸, DXGI��"DirectXͼ�λ����ܹ�"
    IDXGIDevice* DxgiDevice = nullptr;
    HRESULT hr = m_Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&DxgiDevice));	// __uuidof()���Ի�ȡ�ṹ���ӿڼ���ָ�롢���õȱ�����GUID; QueryInterface()���Է���ָ��˽ӿڵ�ָ��, ʧ���򷵻ش������
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to QI for DXGI Device", L"Error", hr);
    }

    // Get DXGI adapter
	// ��ȡDXGI������
    IDXGIAdapter* DxgiAdapter = nullptr;
    hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&DxgiAdapter));	// ����DXGI��ι�ϵ, Adapter����Device��Output
    DxgiDevice->Release();
    DxgiDevice = nullptr;
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to get parent DXGI Adapter", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    // Get output
	// ��ȡDXGI���
    IDXGIOutput* DxgiOutput = nullptr;
    hr = DxgiAdapter->EnumOutputs(Output, &DxgiOutput);		// Adapter��EnumOutputs()����ö��������������� 
    DxgiAdapter->Release();
    DxgiAdapter = nullptr;
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to get specified output in DUPLICATIONMANAGER", L"Error", hr, EnumOutputsExpectedErrors);
    }

    DxgiOutput->GetDesc(&m_OutputDesc);								// ���������������˽�г�Աm_OutputDesc

    // QI for Output 1
    IDXGIOutput1* DxgiOutput1 = nullptr;									// DxgiOutput1�̳���DxgiOutput
    hr = DxgiOutput->QueryInterface(__uuidof(DxgiOutput1), reinterpret_cast<void**>(&DxgiOutput1));
    DxgiOutput->Release();
    DxgiOutput = nullptr;
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to QI for DxgiOutput1 in DUPLICATIONMANAGER", L"Error", hr);
    }

    // Create desktop duplication
	// �������渴����
    hr = DxgiOutput1->DuplicateOutput(m_Device, &m_DeskDupl);	// ����: m_Device; ���: m_DeskDupl; ����DuplicateOutput()���Դ�һ��Adapter��Output����һ��IDXGIOutputDuplication
    DxgiOutput1->Release();
    DxgiOutput1 = nullptr;
    if (FAILED(hr))
    {
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
        {
            MessageBoxW(nullptr, L"There is already the maximum number of applications using the Desktop Duplication API running, please close one of those applications and then try again.", L"Error", MB_OK);
            return DUPL_RETURN_ERROR_UNEXPECTED;
        }
        return ProcessFailure(m_Device, L"Failed to get duplicate output in DUPLICATIONMANAGER", L"Error", hr, CreateDuplicationExpectedErrors);
    }

    return DUPL_RETURN_SUCCESS;
}

//
// Retrieves mouse info and write it into PtrInfo
// ���������Ϣ������PtrInfo
// --- ����: DXGI_OUTDUPL_FRAME_INFO�б�����������Ϣ, ����ֱ��ȡ��
//	--- ���: ����PtrInfo�����г�Ա(��������ʱ)
//
DUPL_RETURN DUPLICATIONMANAGER::GetMouse(_Inout_ PTR_INFO* PtrInfo, _In_ DXGI_OUTDUPL_FRAME_INFO* FrameInfo, INT OffsetX, INT OffsetY)
{
    // A non-zero mouse update timestamp indicates that there is a mouse position update and optionally a shape change
	// һ�������������ʱ������������λ�ø��¶��ҿ�������״�仯
    if (FrameInfo->LastMouseUpdateTime.QuadPart == 0)		// QuadPart�Ǹ�64λ������, ��֤�������֧��64λlong long��ʱ��Ҳ��ʹ��
    {
        return DUPL_RETURN_SUCCESS;
    }

    bool UpdatePosition = true;

    // Make sure we don't update pointer position wrongly
	// ȷ���������ظ���ָ��λ��
    // If pointer is invisible, make sure we did not get an update from another output that the last time that said pointer
    // was visible, if so, don't set it to invisible or update.
	// ���ָ�벻�ɼ�, ȷ����Ҫ�� ���һ��ָ��ָ��ɼ�����һ����� �л�ȡ����, �����������, ��Ҫ��ָ����Ϊ���ɼ������Ѹ���
    if (!FrameInfo->PointerPosition.Visible && (PtrInfo->WhoUpdatedPositionLast != m_OutputNumber))
    {
        UpdatePosition = false;
    }

    // If two outputs both say they have a visible, only update if new update has newer timestamp
	// ���FrameInfo(ϵͳʵ�����)��PtrInfo(�û�����)��ָ�������пɼ���ָ��, ��ôֻ�� �µĸ������µ�ʱ��������� �¸���
    if (FrameInfo->PointerPosition.Visible && PtrInfo->Visible && (PtrInfo->WhoUpdatedPositionLast != m_OutputNumber) && (PtrInfo->LastTimeStamp.QuadPart > FrameInfo->LastMouseUpdateTime.QuadPart))
    {
        UpdatePosition = false;
    }

    // Update position
	// ��UpdatePosition == trueʱ, �Ÿ���
    if (UpdatePosition)
    {
        PtrInfo->Position.x = FrameInfo->PointerPosition.Position.x + m_OutputDesc.DesktopCoordinates.left - OffsetX;
        PtrInfo->Position.y = FrameInfo->PointerPosition.Position.y + m_OutputDesc.DesktopCoordinates.top - OffsetY;
        PtrInfo->WhoUpdatedPositionLast = m_OutputNumber;
        PtrInfo->LastTimeStamp = FrameInfo->LastMouseUpdateTime;
        PtrInfo->Visible = FrameInfo->PointerPosition.Visible != 0;
    }

    // No new shape
	// ��״������Ϊ0ʱ, ���ø���ָ����״
    if (FrameInfo->PointerShapeBufferSize == 0)
    {
        return DUPL_RETURN_SUCCESS;
    }

    // Old buffer too small
	// ���PtrInfo����״������С��FrameInfo����״������, ��ô��Ҫ���¿��ٿռ�
    if (FrameInfo->PointerShapeBufferSize > PtrInfo->BufferSize)
    {
        if (PtrInfo->PtrShapeBuffer)
        {
            delete [] PtrInfo->PtrShapeBuffer;
            PtrInfo->PtrShapeBuffer = nullptr;
        }
        PtrInfo->PtrShapeBuffer = new (std::nothrow) BYTE[FrameInfo->PointerShapeBufferSize];	// std::nothrow, ��newʧ��ʱ����NULL�������׳��쳣
        if (!PtrInfo->PtrShapeBuffer)
        {
            PtrInfo->BufferSize = 0;
            return ProcessFailure(nullptr, L"Failed to allocate memory for pointer shape in DUPLICATIONMANAGER", L"Error", E_OUTOFMEMORY);
        }

        // Update buffer size
        PtrInfo->BufferSize = FrameInfo->PointerShapeBufferSize;
    }

    // Get shape
	// ��ȡ��״��Ϣ
    UINT BufferSizeRequired;
    HRESULT hr = m_DeskDupl->GetFramePointerShape(FrameInfo->PointerShapeBufferSize, reinterpret_cast<VOID*>(PtrInfo->PtrShapeBuffer), &BufferSizeRequired, &(PtrInfo->ShapeInfo));	// IDXGIOutputDuplication::GetFramePointerShape()����, ���Ի�ȡ��ǰ�����ָ����״
																																																																												// --- ԭ��:	_in_		UINT															...,		---- ��״��������С
																																																																												//					_out_	void*															...,		---- ��״�������׵�ַ
																																																																												//					_out_	UINT*															...,		---- �������״��������С
																																																																												//					_out_	DXGI_OUTDUPL_POINTER_SHAPE_INFO		...		---- DXGI��״��Ϣ
    if (FAILED(hr))
    {
        delete [] PtrInfo->PtrShapeBuffer;
        PtrInfo->PtrShapeBuffer = nullptr;
        PtrInfo->BufferSize = 0;
        return ProcessFailure(m_Device, L"Failed to get frame pointer shape in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
    }

    return DUPL_RETURN_SUCCESS;
}


//
// Get next frame and write it into Data
// ��ȡ��һ֡��Ϣ��д��Data
//
_Success_(*Timeout == false && return == DUPL_RETURN_SUCCESS)
DUPL_RETURN DUPLICATIONMANAGER::GetFrame(_Out_ FRAME_DATA* Data, _Out_ bool* Timeout)
{
    IDXGIResource* DesktopResource = nullptr;	// Device��������Resource��Surface
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;		// DXGI֡��Ϣ

    // Get new frame
	// ��ȡ�µ�֡
    HRESULT hr = m_DeskDupl->AcquireNextFrame(500, &FrameInfo, &DesktopResource);	// IDXGIOutputDuplication::AcquireNextFrame()����, ָ��Ӧ�ó����Ѿ�׼���ô�����һ֡����ͼƬ
																																				// --- ԭ��:	_in_		UINT											...,		---- ���ٺ����㳬ʱ
																																				//					_out_	DXGI_OUTDUPL_FRAME_INFO		...,		---- ����DXGI֡��Ϣ
																																				//					_out_	IDXGIResource								...,		---- ��������λͼ�Ľӿ�
    if (hr == DXGI_ERROR_WAIT_TIMEOUT)	// ��ʱ, �򲻸���
    {
        *Timeout = true;
        return DUPL_RETURN_SUCCESS;
    }
    *Timeout = false;

    if (FAILED(hr))											// ��������, ���׳��쳣
    {
        return ProcessFailure(m_Device, L"Failed to acquire next frame in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
    }

    // If still holding old frame, destroy it
	// �����ǰ��m_AcquiredDesktopImage(IDXGITexture2D*����)��ΪNULL, ��������
    if (m_AcquiredDesktopImage)
    {
        m_AcquiredDesktopImage->Release();
        m_AcquiredDesktopImage = nullptr;
    }

    // QI for IDXGIResource
	// Ϊm_AcquiredDesktopImage���²�ѯ�ӿ�
    hr = DesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&m_AcquiredDesktopImage));
    DesktopResource->Release();
    DesktopResource = nullptr;
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to QI for ID3D11Texture2D from acquired IDXGIResource in DUPLICATIONMANAGER", L"Error", hr);
    }

    // Get metadata
	// ��ȡԪ����, ������m_MetaData��ص�˽�г�Ա, ������Data(FRAME_DATA*����)�ĳ�Ա
    if (FrameInfo.TotalMetadataBufferSize)
    {
		// 1 - ����m_MetaDataSize
        // Old buffer too small
		// ���յ�Ԫ���ݻ�������С > ˽�г�Աm_MetaDataSize, ����Ҫ���·���ռ�
        if (FrameInfo.TotalMetadataBufferSize > m_MetaDataSize)
        {
            if (m_MetaDataBuffer)
            {
                delete [] m_MetaDataBuffer;
                m_MetaDataBuffer = nullptr;
            }
            m_MetaDataBuffer = new (std::nothrow) BYTE[FrameInfo.TotalMetadataBufferSize];		// std::nothrow, newʧ��ʱ����NULL�������׳��쳣
            if (!m_MetaDataBuffer)
            {
                m_MetaDataSize = 0;
                Data->MoveCount = 0;
                Data->DirtyCount = 0;
                return ProcessFailure(nullptr, L"Failed to allocate memory for metadata in DUPLICATIONMANAGER", L"Error", E_OUTOFMEMORY);
            }
            m_MetaDataSize = FrameInfo.TotalMetadataBufferSize;
        }


		// 2 - ��ȡ�ƶ�����(move rectangles) 
        UINT BufSize = FrameInfo.TotalMetadataBufferSize;

        // Get move rectangles
        hr = m_DeskDupl->GetFrameMoveRects(BufSize, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(m_MetaDataBuffer), &BufSize);	// IDXGIOutputDuplication::GetFrameMoveRects(), ��ȡ�ƶ�����
																																																						// --- ԭ��:	_in_		UINT											...,		---- ��������С
																																																						//					_out_	DXGI_OUTDUPL_MOVE_RECT*		...,		---- DXGI�ƶ����λ������׵�ַ, ����SourcePoint��DestinationRect
																																																						//					_out_	UINT*											...		---- DXGI�ƶ����λ�������С
        if (FAILED(hr))
        {
            Data->MoveCount = 0;
            Data->DirtyCount = 0;
            return ProcessFailure(nullptr, L"Failed to get frame move rects in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
        }
        Data->MoveCount = BufSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);		// �ƶ����θ��� = ���º�Ļ�������СBufSize / һ���ƶ����λ������Ĵ�С

		// 3 - ��ȡ�����(dirty rectangles)
        BYTE* DirtyRects = m_MetaDataBuffer + BufSize;				// ������׵�ַ = m_MetaDataBuffer�׵�ַ + BufSize(�ƶ����λ������ܵĴ�С)
        BufSize = FrameInfo.TotalMetadataBufferSize - BufSize;		// ����λ�������С = FrameInfo.TotalMetadataBufferSize - BufSize(�ƶ����λ������ܵĴ�С)

        // Get dirty rectangles
        hr = m_DeskDupl->GetFrameDirtyRects(BufSize, reinterpret_cast<RECT*>(DirtyRects), &BufSize);		// IDXGIOutputDuplication::GetFrameDirtyRects(), ��ȡ�����, ԭ�����ƶ���������
        if (FAILED(hr))
        {
            Data->MoveCount = 0;
            Data->DirtyCount = 0;
            return ProcessFailure(nullptr, L"Failed to get frame dirty rects in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
        }
        Data->DirtyCount = BufSize / sizeof(RECT);						// ����θ��� = ���º�Ļ�������СBufSize / һ������RECT�Ĵ�С

		// 4 - ����õ�m_MetaDataBuffer�׵�ַ����Data->MetaData
        Data->MetaData = m_MetaDataBuffer;								// �����׵�ַ
    }

    Data->Frame = m_AcquiredDesktopImage;	// Frame����IDXGITexture2D֡����
    Data->FrameInfo = FrameInfo;						// FrameInfo����IDXGI֡��Ϣ(DXGI_OUTDUPL_FRAME_INFO)

    return DUPL_RETURN_SUCCESS;
}

//
// Release frame
// �ͷ�֡
//
DUPL_RETURN DUPLICATIONMANAGER::DoneWithFrame()
{
    HRESULT hr = m_DeskDupl->ReleaseFrame();
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device, L"Failed to release frame in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
    }

    if (m_AcquiredDesktopImage)
    {
        m_AcquiredDesktopImage->Release();
        m_AcquiredDesktopImage = nullptr;
    }

    return DUPL_RETURN_SUCCESS;
}

//
// Gets output desc into DescPtr
// ��m_OutputDesc��ȡDXGI_OUTPUT_DESC
//
void DUPLICATIONMANAGER::GetOutputDesc(_Out_ DXGI_OUTPUT_DESC* DescPtr)
{
    *DescPtr = m_OutputDesc;
}
