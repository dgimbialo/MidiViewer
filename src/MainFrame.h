#pragma once
#include "pch.h"
#include "ScoreView.h"
#include "NoteListPanel.h"
#include "AppMessages.h"
#include "SizerBar.h"

class CMainFrame : public CFrameWnd
{
public:
    CMainFrame();
    virtual ~CMainFrame();

    // ---------------------------------------------------------------------------
    // Dock / float control — also called by CPanelFloatFrame (defined in .cpp)
    // ---------------------------------------------------------------------------
    enum class DockMode { Right, Bottom, Float };

    void DockPanelRight();
    void DockPanelBottom();
    void FloatPanel();

protected:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs,
                                CCreateContext* pContext) override;
    virtual void RecalcLayout(BOOL bNotify = TRUE) override;

    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnFileOpen();
    afx_msg void OnFileExit();
    afx_msg void OnHelpAbout();
    afx_msg void OnSelectTrack(UINT nID);
    afx_msg LRESULT OnScoreNoteSelected(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnListNoteSelected(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnScoreNoteHover(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSizerDrag(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSizerRClick(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    CScoreView      m_view;
    CScoreHeaderBar m_scoreHeader;
    CNoteListPanel  m_notePanel;
    CSizerBar       m_sizer;
    CStatusBar      m_statusBar;

    DockMode        m_dockMode    = DockMode::Right;
    int             m_panelWidth  = 340;   // used in Right dock mode
    int             m_panelHeight = 220;   // used in Bottom dock mode
    CWnd*           m_floatFrame  = nullptr; // heap-allocated CPanelFloatFrame

    static constexpr int SIZER_W = 6;     // sizer bar thickness in pixels

    NoteRef m_selInfoRef;                 // note currently shown in info pane

    MidiDocument m_doc;                   // full parsed document (all tracks)
    CString      m_fileName;              // current file name (for titles)
    int          m_curTrack = -1;         // index of the track being displayed

    void    RepositionViews();
    void    ShowNoteInfo(NoteRef ref);    // update status bar detail pane
    CString BuildNoteInfo(NoteRef ref) const;

    void    ShowTrack(int trackIdx);      // (re)load one track into view + list
    void    PopulateTrackMenu();          // fill the Track menu from m_doc
    CMenu*  GetTrackMenu() const;         // the "Track" popup, or nullptr
};
