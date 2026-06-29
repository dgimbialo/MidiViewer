#include "pch.h"
#include "App.h"
#include "MainFrame.h"
#include "resource.h"

CSimpleMidiApp theApp;

BEGIN_MESSAGE_MAP(CSimpleMidiApp, CWinApp)
END_MESSAGE_MAP()

CSimpleMidiApp::CSimpleMidiApp()
{
}

BOOL CSimpleMidiApp::InitInstance()
{
    CWinApp::InitInstance();

    // Enable common controls (for status bar, etc.)
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    CMainFrame* pFrame = new CMainFrame();
    if (!pFrame->LoadFrame(IDR_MAINMENU))
        return FALSE;

    // Apply the application icon
    HICON hIcon = ::LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_MAINICON));
    if (hIcon)
    {
        pFrame->SetIcon(hIcon, TRUE);
        pFrame->SetIcon(hIcon, FALSE);
    }

    m_pMainWnd = pFrame;
    pFrame->ShowWindow(m_nCmdShow);
    pFrame->UpdateWindow();

    return TRUE;
}

int CSimpleMidiApp::ExitInstance()
{
    return CWinApp::ExitInstance();
}
