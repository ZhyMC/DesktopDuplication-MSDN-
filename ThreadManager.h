// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

// --- �̹߳����ߣ������̳߳�
// --- ������ʼ��D3D��Դ����ȡ���ָ����Ϣ����ʼ��ֹͣ�̵߳ȷ���

#ifndef _THREADMANAGER_H_
#define _THREADMANAGER_H_

#include "CommonTypes.h"

class THREADMANAGER
{
    public:
        THREADMANAGER();
        ~THREADMANAGER();
        void Clean();											// ����̣߳�
        DUPL_RETURN Initialize(INT SingleOutput, UINT OutputCount, HANDLE UnexpectedErrorEvent, HANDLE ExpectedErrorEvent, HANDLE TerminateThreadsEvent, HANDLE SharedHandle, _In_ RECT* DesktopDim);		// �̹߳�����(�̳߳�)�ĳ�ʼ��
        PTR_INFO* GetPointerInfo();				// ��ȡ���ָ����Ϣ
        void WaitForThreadTermination();		// �ȴ��߳���ֹ

    private:
        DUPL_RETURN InitializeDx(_Out_ DX_RESOURCES* Data);			// ��ʼ��D3D��Դ
        void CleanDx(_Inout_ DX_RESOURCES* Data);								// ���D3D��Դ

        PTR_INFO m_PtrInfo;																	// ��Ա�����ָ����Ϣ
        UINT m_ThreadCount;																	// ��Ա���߳���
        _Field_size_(m_ThreadCount) HANDLE* m_ThreadHandles;			// ��Ա���߳̾�����飬Ԫ��Ϊ���
        _Field_size_(m_ThreadCount) THREAD_DATA* m_ThreadData;	// ��Ա���߳��������飬Ԫ��Ϊ�߳����ݣ��߳�����THREAD_DATA��<CommonTypes.h>����
};

#endif
