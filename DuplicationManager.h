// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

// --- �����������
// --- ����ID3D11Device��IDXGIOutputDuplication��DXGI_OUTPUT_DESC��ID3D11Texture2D��Ԫ���ݵĳ�ʼ��
// --- ���л�ȡ����֡�����ָ���public����

#ifndef _DUPLICATIONMANAGER_H_
#define _DUPLICATIONMANAGER_H_

#include "CommonTypes.h"

//
// Handles the task of duplicating an output.
//
class DUPLICATIONMANAGER
{
    public:
        DUPLICATIONMANAGER();
        ~DUPLICATIONMANAGER();
        _Success_(*Timeout == false && return == DUPL_RETURN_SUCCESS) DUPL_RETURN GetFrame(_Out_ FRAME_DATA* Data, _Out_ bool* Timeout);	// _Success_()�еĲ������������سɹ�ʱ��Ӧ��ֵ
        DUPL_RETURN DoneWithFrame();	// �ͷ�֡
        DUPL_RETURN InitDupl(_In_ ID3D11Device* Device, UINT Output);	// ��ʼ��Duplication
        DUPL_RETURN GetMouse(_Inout_ PTR_INFO* PtrInfo, _In_ DXGI_OUTDUPL_FRAME_INFO* FrameInfo, INT OffsetX, INT OffsetY);	// ��ȡ�����Ϣ
        void GetOutputDesc(_Out_ DXGI_OUTPUT_DESC* DescPtr);	// ��ȡ�������

    private:

    // vars
        IDXGIOutputDuplication* m_DeskDupl;										// ���渴����, IDXGIOutputDuplication
        ID3D11Texture2D* m_AcquiredDesktopImage;							// ���������ͼƬ��Texture2D
        _Field_size_bytes_(m_MetaDataSize) BYTE* m_MetaDataBuffer;	// Ԫ���ݻ�����
        UINT m_MetaDataSize;																// Ԫ���ݴ�С
        UINT m_OutputNumber;																// �����ţ�
        DXGI_OUTPUT_DESC m_OutputDesc;											// �������, �����豸����������Ρ��������Ϣ
        ID3D11Device* m_Device;															// �豸
};

#endif
