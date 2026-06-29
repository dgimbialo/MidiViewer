#pragma once
#include "pch.h"

class CMainFrame;

class CSimpleMidiApp : public CWinApp
{
public:
    CSimpleMidiApp();

    virtual BOOL InitInstance() override;
    virtual int  ExitInstance()  override;

    DECLARE_MESSAGE_MAP()
};

extern CSimpleMidiApp theApp;
