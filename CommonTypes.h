// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

// --- ���������Ŀⶼ����������
// --- ����ͨ�õ����Ͷ��ڴ�#define����typedef
// --- ��������һЩ�쳣�����õ�������

#ifndef _COMMONTYPES_H_
#define _COMMONTYPES_H_

#include <windows.h>		// Windows�ر���
#include <d3d11.h>			// Direct3D 11
#include <dxgi1_2.h>			// DirectX ͼ�λ����ܹ�
#include <sal.h>					// ΢����Ū�����ı�׼ע������
#include <new>					// newһ������
#include <warning.h>			// ����һЩ���õ�#pragma (warning)
#include <DirectXMath.h>	// ͼ��Ӧ�ó�����ѧ��

#include "PixelShader.h"		// ���ػ�ͼͷ�ļ�
#include "VertexShader.h"	// �����ͼͷ�ļ�

#define NUMVERTICES 6		// Ϊʲôƽ�����6�����㣿������õ�3Dģ��
#define BPP         4				// �������: ÿ�������õ�λ��

#define OCCLUSION_STATUS_MSG WM_USER		// ���ڷ�ֹ�û�ID��ϵͳID���͵���Ϣ��ͻ��΢����ǳ�ϲ�������Լ��ĺ�

extern HRESULT SystemTransitionsExpectedErrors[];		/* �����쳣�����5������ */
extern HRESULT CreateDuplicationExpectedErrors[];
extern HRESULT FrameInfoExpectedErrors[];
extern HRESULT AcquireFrameExpectedError[];
extern HRESULT EnumOutputsExpectedErrors[];

typedef _Return_type_success_(return == DUPL_RETURN_SUCCESS) enum		// DUPL_RETURN �൱�� HRESULT����ʾһ��������ִ�н��(�ɹ���Ԥ�����쳣�������쳣)
{
    DUPL_RETURN_SUCCESS             = 0,
    DUPL_RETURN_ERROR_EXPECTED      = 1,
    DUPL_RETURN_ERROR_UNEXPECTED    = 2
}DUPL_RETURN;

_Post_satisfies_(return != DUPL_RETURN_SUCCESS)
DUPL_RETURN ProcessFailure(_In_opt_ ID3D11Device* Device, _In_ LPCWSTR Str, _In_ LPCWSTR Title, HRESULT hr, _In_opt_z_ HRESULT* ExpectedErrors = nullptr);	// ������ʾ�׳��쳣�ĺ���

void DisplayMsg(_In_ LPCWSTR Str, _In_ LPCWSTR Title, HRESULT hr);		// ��ʾ��Ϣ

//
// Holds info about the pointer/cursor
// �������ָ����Ϣ
//
typedef struct _PTR_INFO
{
    _Field_size_bytes_(BufferSize) BYTE* PtrShapeBuffer;	// �Զ��壬�����״������
    DXGI_OUTDUPL_POINTER_SHAPE_INFO ShapeInfo;	// dxgi1_2.h��������״����ߡ��Ƕȡ�����
    POINT Position;															// ���λ��
    bool Visible;																	// ����Ƿ�ɼ�
    UINT BufferSize;															// ��������С
    UINT WhoUpdatedPositionLast;									// ˭���������λ�ã�
    LARGE_INTEGER LastTimeStamp;								// ʱ���
} PTR_INFO;

//
// Structure that holds D3D resources not directly tied to any one thread
// ����D3D��Դ���(�����߳�)
//
typedef struct _DX_RESOURCES
{
    ID3D11Device* Device;							// �豸
    ID3D11DeviceContext* Context;				// �豸������
    ID3D11VertexShader* VertexShader;		// ������ɫ��
    ID3D11PixelShader* PixelShader;				// ������ɫ��
    ID3D11InputLayout* InputLayout;			// ���벼��
    ID3D11SamplerState* SamplerLinear;		// ������
} DX_RESOURCES;

//
// Structure to pass to a new thread
// ���ݸ��̵߳Ľṹ
//
typedef struct _THREAD_DATA
{
    // Used to indicate abnormal error condition
	// ��Ԥ���쳣
    HANDLE UnexpectedErrorEvent;

    // Used to indicate a transition event occurred e.g. PnpStop, PnpStart, mode change, TDR, desktop switch and the application needs to recreate the duplication interface
	// Ԥ���쳣
    HANDLE ExpectedErrorEvent;

    // Used by WinProc to signal to threads to exit
	// WinProc��Windows���ڵĻص�������TerminateThreadsEvent������WinProc��������ֹ�߳�
    HANDLE TerminateThreadsEvent;

    HANDLE TexSharedHandle;		// ������ľ����
    UINT Output;							// �����
    INT OffsetX;								// XУ��ֵ
    INT OffsetY;								// YУ��ֵ
    PTR_INFO* PtrInfo;					// ǰ�涨�壬���ָ����Ϣ
    DX_RESOURCES DxRes;			// ǰ�涨�壬����һЩD3D��Դ��صı���
} THREAD_DATA;

//
// FRAME_DATA holds information about an acquired frame
// FRAME_DATA֡��Ϣ������һ�Ų��񵽵�֡
//
typedef struct _FRAME_DATA
{
    ID3D11Texture2D* Frame;																																							// d3d11.h, D3D��2D�����������Ϊһ��ͼ
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;																																	// dxgi1_2.h, �����ϴγ��ֵ�ʱ�䡢�����µ�ʱ�䡢���ָ��λ�á���������С����Ϣ
    _Field_size_bytes_((MoveCount * sizeof(DXGI_OUTDUPL_MOVE_RECT)) + (DirtyCount * sizeof(RECT))) BYTE* MetaData;			// Ԫ����MetaData��ռ���ڴ��С=�ƶ����εĴ�С+����εĴ�С
    UINT DirtyCount;																																											// �������
    UINT MoveCount;																																										// �ƶ�������
} FRAME_DATA;

//
// A vertex with a position and texture coordinate
// ����Vertex, ����3Dλ�ú�2D��������
//
typedef struct _VERTEX
{
    DirectX::XMFLOAT3 Pos;					// 3D���꣬�������ǰ��궨�嶨����Ϊ6��ԭ��
    DirectX::XMFLOAT2 TexCoord;		// 2D�������꣬Ҳ�����������surface
} VERTEX;

#endif
