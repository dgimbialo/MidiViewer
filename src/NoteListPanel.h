#pragma once
#include "pch.h"
#include "MidiDocument.h"
#include "AppMessages.h"

// ---------------------------------------------------------------------------
// CNoteListPanel — docked right-side panel showing all MIDI NoteOn/NoteOff
// messages grouped by bar.
//
// Columns: Note | Pitch | Track | Ch | Vel | Start Tick | Start ms | Dur Ticks | Dur ms
// Bar boundaries are shown as separator rows (non-selectable, colored headers).
//
// Two-way sync with CScoreView via WM_LIST_NOTE_SELECTED / WM_SCORE_NOTE_SELECTED
// messages handled by CMainFrame.
// ---------------------------------------------------------------------------
class CNoteListPanel : public CWnd
{
public:
    CNoteListPanel();
    virtual ~CNoteListPanel();

    BOOL Create(CWnd* pParent, UINT nID);

    // Called by MainFrame after a new document is loaded
    void LoadDocument(const MidiDocument* pDoc);

    // Called by MainFrame to sync selection coming from CScoreView
    void SetSelectedNote(NoteRef ref);

private:
    CListCtrl            m_list;
    const MidiDocument*  m_pDoc     = nullptr;
    std::vector<NoteRef> m_itemMap;   // list item index → NoteRef (invalid = bar header)
    bool                 m_bSyncing  = false; // prevent re-entrant selection loop

    void RebuildList();
    void SetupColumns();

    afx_msg int  OnCreate(LPCREATESTRUCT lp);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()
};
