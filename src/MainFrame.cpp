#include "pch.h"
#include "MainFrame.h"
#include "MidiParser.h"
#include "resource.h"
#include <vector>

// ===========================================================================
// CTrackSelectDlg — modal chooser shown when a file has more than one track.
// ===========================================================================
class CTrackSelectDlg : public CDialog
{
public:
    CTrackSelectDlg(const std::vector<CString>& names, int initial, CWnd* pParent)
        : CDialog(IDD_SELECT_TRACK, pParent), m_names(names), m_sel(initial) {}

    int GetSelected() const { return m_sel; }

protected:
    virtual BOOL OnInitDialog() override
    {
        CDialog::OnInitDialog();
        CListBox* lb = static_cast<CListBox*>(GetDlgItem(IDC_TRACK_LIST));
        if (lb)
        {
            for (const auto& n : m_names) lb->AddString(n);
            lb->SetCurSel(m_sel >= 0 && m_sel < (int)m_names.size() ? m_sel : 0);
        }
        return TRUE;
    }

    virtual void OnOK() override
    {
        CListBox* lb = static_cast<CListBox*>(GetDlgItem(IDC_TRACK_LIST));
        if (lb)
        {
            int s = lb->GetCurSel();
            if (s != LB_ERR) m_sel = s;
        }
        CDialog::OnOK();
    }

    afx_msg void OnDblClk() { OnOK(); }   // double-click a row = Open

    DECLARE_MESSAGE_MAP()

private:
    std::vector<CString> m_names;
    int                  m_sel;
};

BEGIN_MESSAGE_MAP(CTrackSelectDlg, CDialog)
    ON_LBN_DBLCLK(IDC_TRACK_LIST, &CTrackSelectDlg::OnDblClk)
END_MESSAGE_MAP()

// ===========================================================================
// CPanelFloatFrame — top-level window that hosts CNoteListPanel when floating.
// Right-click title bar → context menu to dock back.
// Close button → dock right.
// ===========================================================================
class CPanelFloatFrame : public CWnd
{
public:
    // Dock-mode commands added to the system menu
    static constexpr UINT CMD_DOCK_RIGHT  = 0x0100;
    static constexpr UINT CMD_DOCK_BOTTOM = 0x0101;

    BOOL Create(CWnd* pPanel, const CRect& rect)
    {
        LPCTSTR cls = AfxRegisterWndClass(
            CS_HREDRAW | CS_VREDRAW,
            ::LoadCursor(nullptr, IDC_ARROW),
            (HBRUSH)(COLOR_3DFACE + 1));

        if (!CWnd::CreateEx(
                WS_EX_TOOLWINDOW,
                cls, L"Note List",
                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                rect, nullptr, 0))
            return FALSE;

        // Extend system menu
        CMenu* pSys = GetSystemMenu(FALSE);
        if (pSys)
        {
            pSys->AppendMenu(MF_SEPARATOR);
            pSys->AppendMenu(MF_STRING, CMD_DOCK_RIGHT,  L"Dock Right");
            pSys->AppendMenu(MF_STRING, CMD_DOCK_BOTTOM, L"Dock Bottom");
        }

        // Reparent panel as our child
        ::SetParent(pPanel->m_hWnd, m_hWnd);
        CRect rc;
        GetClientRect(&rc);
        pPanel->SetWindowPos(nullptr, 0, 0, rc.Width(), rc.Height(),
                             SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

        ShowWindow(SW_SHOW);
        return TRUE;
    }

protected:
    afx_msg void OnSize(UINT nType, int cx, int cy)
    {
        CWnd::OnSize(nType, cx, cy);
        CWnd* pChild = GetWindow(GW_CHILD);
        if (pChild && ::IsWindow(pChild->m_hWnd))
            pChild->SetWindowPos(nullptr, 0, 0, cx, cy,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
    }

    afx_msg void OnClose()
    {
        // Close = dock right (do NOT call base; DockPanelRight destroys us)
        CMainFrame* pMain = static_cast<CMainFrame*>(AfxGetMainWnd());
        if (pMain) pMain->DockPanelRight();
    }

    afx_msg void OnSysCommand(UINT nID, LPARAM lParam)
    {
        CMainFrame* pMain = static_cast<CMainFrame*>(AfxGetMainWnd());
        if (pMain)
        {
            if (nID == CMD_DOCK_RIGHT)  { pMain->DockPanelRight();  return; }
            if (nID == CMD_DOCK_BOTTOM) { pMain->DockPanelBottom(); return; }
        }
        CWnd::OnSysCommand(nID, lParam);
    }

    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CPanelFloatFrame, CWnd)
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

// ===========================================================================
// CMainFrame
// ===========================================================================

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_COMMAND(IDC_FILE_OPEN, &CMainFrame::OnFileOpen)
    ON_COMMAND(IDC_FILE_EXIT, &CMainFrame::OnFileExit)
    ON_COMMAND(IDC_HELP_ABOUT, &CMainFrame::OnHelpAbout)
    ON_COMMAND_RANGE(IDC_TRACK_FIRST, IDC_TRACK_LAST, &CMainFrame::OnSelectTrack)
    ON_MESSAGE(WM_SCORE_NOTE_SELECTED, &CMainFrame::OnScoreNoteSelected)
    ON_MESSAGE(WM_LIST_NOTE_SELECTED,  &CMainFrame::OnListNoteSelected)
    ON_MESSAGE(WM_SCORE_NOTE_HOVER,    &CMainFrame::OnScoreNoteHover)
    ON_MESSAGE(WM_SIZER_DRAG,          &CMainFrame::OnSizerDrag)
    ON_MESSAGE(WM_SIZER_RCLICK,        &CMainFrame::OnSizerRClick)
END_MESSAGE_MAP()

static UINT indicators[] = { ID_SEPARATOR, ID_SEPARATOR };

// ---------------------------------------------------------------------------

CMainFrame::CMainFrame()
{
}

CMainFrame::~CMainFrame()
{
    if (m_floatFrame)
    {
        if (::IsWindow(m_floatFrame->m_hWnd))
            m_floatFrame->DestroyWindow();
        delete m_floatFrame;
        m_floatFrame = nullptr;
    }
}

// ---------------------------------------------------------------------------

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CFrameWnd::PreCreateWindow(cs))
        return FALSE;
    cs.style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    cs.cx    = 1280;
    cs.cy    = 720;
    return TRUE;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    CRect rect;
    GetClientRect(&rect);

    // Score header bar
    if (!m_scoreHeader.Create(
            AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
                                ::LoadCursor(nullptr, IDC_ARROW),
                                (HBRUSH)(COLOR_WINDOW + 1), nullptr),
            L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            CRect(0, 0, rect.Width(), SCORE_HEADER_H), this, AFX_IDW_PANE_FIRST + 4))
        return -1;

    // Score view
    if (!m_view.Create(nullptr, nullptr,
                       WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_HSCROLL | WS_VSCROLL,
                       rect, this, AFX_IDW_PANE_FIRST))
        return -1;

    // Note list panel
    if (!m_notePanel.Create(this, AFX_IDW_PANE_FIRST + 2))
        return -1;

    // Sizer bar (vertical — separates score from right-docked panel)
    if (!m_sizer.Create(this, AFX_IDW_PANE_FIRST + 3, CSizerBar::Orientation::Vertical))
        return -1;

    // Status bar
    if (!m_statusBar.Create(this) ||
        !m_statusBar.SetIndicators(indicators, sizeof(indicators) / sizeof(UINT)))
        return -1;

    // Pane 0 stretches (file info); pane 1 is a fixed-width note-detail readout.
    m_statusBar.SetPaneInfo(0, ID_SEPARATOR, SBPS_STRETCH, 0);
    m_statusBar.SetPaneInfo(1, ID_SEPARATOR, SBPS_NORMAL, 420);
    m_statusBar.SetPaneText(0, L"Ready - open a MIDI file to begin  (Ctrl+O)");

    SetWindowText(L"Simple MIDI Viewer");
    return 0;
}

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT, CCreateContext*)
{
    return TRUE;
}

// ---------------------------------------------------------------------------
// File commands
// ---------------------------------------------------------------------------

void CMainFrame::OnFileOpen()
{
    CFileDialog dlg(TRUE, L"mid", nullptr,
                    OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                    L"MIDI Files (*.mid;*.midi)|*.mid;*.midi|All Files (*.*)|*.*||",
                    this);
    if (dlg.DoModal() != IDOK) return;

    std::wstring path     = dlg.GetPathName().GetString();
    CString      fileName = dlg.GetFileName();

    MidiDocument doc;
    std::wstring error;
    if (!MidiParser::Parse(path, doc, error))
    {
        MessageBox(error.c_str(), L"Error opening MIDI file", MB_ICONERROR | MB_OK);
        return;
    }

    if (doc.tracks.empty())
    {
        MessageBox(L"This MIDI file contains no playable note tracks.",
                   L"Nothing to display", MB_ICONINFORMATION | MB_OK);
        return;
    }

    m_doc      = std::move(doc);
    m_fileName = fileName;

    // If there is more than one track, let the user choose which to open.
    int sel = 0;
    if (m_doc.tracks.size() > 1)
    {
        std::vector<CString> names;
        for (int i = 0; i < (int)m_doc.tracks.size(); ++i)
        {
            CString name(MidiDocument::ToWide(m_doc.tracks[i].name).c_str());
            CString label;
            label.Format(L"%d.  %s   (%d notes)",
                         i + 1, (LPCWSTR)name, (int)m_doc.tracks[i].notes.size());
            names.push_back(label);
        }

        CTrackSelectDlg chooser(names, 0, this);
        if (chooser.DoModal() != IDOK) return;   // cancelled — leave current view
        sel = chooser.GetSelected();
    }

    PopulateTrackMenu();
    ShowTrack(sel);
}

// ---------------------------------------------------------------------------
// Track selection / switching
// ---------------------------------------------------------------------------

CMenu* CMainFrame::GetTrackMenu() const
{
    CMenu* bar = GetMenu();
    if (!bar) return nullptr;
    // Menu order: File(0) View(1) Track(2) Help(3)
    return bar->GetSubMenu(2);
}

void CMainFrame::PopulateTrackMenu()
{
    CMenu* tm = GetTrackMenu();
    if (!tm) return;

    while (tm->GetMenuItemCount() > 0)
        tm->DeleteMenu(0, MF_BYPOSITION);

    for (int i = 0; i < (int)m_doc.tracks.size(); ++i)
    {
        CString name(MidiDocument::ToWide(m_doc.tracks[i].name).c_str());
        CString label;
        label.Format(L"&%d  %s   (%d notes)",
                     i + 1, (LPCWSTR)name, (int)m_doc.tracks[i].notes.size());
        tm->AppendMenu(MF_STRING, IDC_TRACK_FIRST + i, label);
    }
    DrawMenuBar();
}

void CMainFrame::OnSelectTrack(UINT nID)
{
    int idx = (int)(nID - IDC_TRACK_FIRST);
    if (idx != m_curTrack)
        ShowTrack(idx);
}

void CMainFrame::ShowTrack(int trackIdx)
{
    if (trackIdx < 0 || trackIdx >= (int)m_doc.tracks.size()) return;
    m_curTrack = trackIdx;

    const MidiTrack& track = m_doc.tracks[trackIdx];

    // Extent of just this track (so the score isn't padded with empty bars
    // belonging to other, longer tracks).
    long maxEnd = 0;
    for (const auto& n : track.notes)
        maxEnd = std::max(maxEnd, n.endTick);

    // Build a single-track document sharing the file's tempo / time-sig map.
    MidiDocument single;
    single.format     = m_doc.format;
    single.tpqn       = m_doc.tpqn;
    single.tempos     = m_doc.tempos;
    single.timeSigs   = m_doc.timeSigs;
    single.totalTicks = maxEnd;
    single.tracks.push_back(track);

    // Fully re-initialise the score view and the note list.
    m_view.LoadDocument(std::move(single));
    m_notePanel.LoadDocument(m_view.GetDocument());

    // Score header: tempo / time sig + per-track stats.
    long tempo = 500000;
    int  tsNum = 4, tsDen = 4;
    if (!m_doc.tempos.empty())   tempo = m_doc.tempos[0].microsecondsPerQN;
    if (!m_doc.timeSigs.empty()) { tsNum = m_doc.timeSigs[0].numerator;
                                   tsDen = m_doc.timeSigs[0].denominator; }
    int bpm = (tempo > 0) ? (int)(60000000L / tempo) : 0;
    m_scoreHeader.SetInfo(bpm, tsNum, tsDen);

    int  numNotes   = (int)track.notes.size();
    int  numBars    = m_doc.TickToBar(maxEnd > 0 ? maxEnd - 1 : 0) + 1;
    long durationMs = m_doc.TickToMs(maxEnd);
    m_scoreHeader.SetStats(numBars, durationMs, numNotes);

    // Title + status bar.
    CString trackName(MidiDocument::ToWide(track.name).c_str());
    SetWindowText(L"Simple MIDI Viewer - " + m_fileName + L"  [" + trackName + L"]");

    m_selInfoRef = NoteRef{};
    CString info;
    info.Format(L"%s   |   Track %d/%d: %s   |   TPQN: %ld",
                (LPCWSTR)m_fileName, trackIdx + 1, (int)m_doc.tracks.size(),
                (LPCWSTR)trackName, m_doc.tpqn);
    m_statusBar.SetPaneText(0, info);
    m_statusBar.SetPaneText(1, L"Hover or click a note to see its details");

    // Reflect the active track as a radio check in the Track menu.
    CMenu* tm = GetTrackMenu();
    if (tm && tm->GetMenuItemCount() > 0)
        tm->CheckMenuRadioItem(0, tm->GetMenuItemCount() - 1,
                               trackIdx, MF_BYPOSITION);
}

void CMainFrame::OnFileExit()
{
    PostMessage(WM_CLOSE);
}

// ---------------------------------------------------------------------------
// Help > About
// ---------------------------------------------------------------------------

void CMainFrame::OnHelpAbout()
{
    MSGBOXPARAMS mbp{};
    mbp.cbSize      = sizeof(mbp);
    mbp.hwndOwner   = m_hWnd;
    mbp.hInstance   = AfxGetInstanceHandle();
    mbp.lpszCaption = L"About Simple MIDI Viewer";
    mbp.dwStyle     = MB_OK | MB_USERICON;
    mbp.lpszIcon    = MAKEINTRESOURCE(IDI_MAINICON);
    mbp.lpszText =
        L"Simple MIDI Viewer\n"
        L"Version 1.1\n\n"
        L"A lightweight viewer for Standard MIDI Files.\n"
        L"View notes on a score, inspect every event in the list,\n"
        L"and click to keep both views in sync.\n\n"
        L"Tips:\n"
        L"  -  Ctrl + Mouse Wheel  -  zoom the score\n"
        L"  -  Hover a note  -  see its details in the status bar\n"
        L"  -  Right-click the divider  -  dock or float the note list\n\n"
        L"(c) 2026 Simple MIDI Viewer";
    MessageBoxIndirect(&mbp);
}

// ---------------------------------------------------------------------------
// Note-detail status-bar read-out
// ---------------------------------------------------------------------------

CString CMainFrame::BuildNoteInfo(NoteRef ref) const
{
    const MidiDocument* d = m_view.GetDocument();
    if (!d || !ref.IsValid() ||
        ref.trackIdx >= (int)d->tracks.size() ||
        ref.noteIdx  >= (int)d->tracks[ref.trackIdx].notes.size())
        return CString();

    const MidiNote& note = d->tracks[ref.trackIdx].notes[ref.noteIdx];
    long startMs = d->TickToMs(note.startTick);
    long durMs   = d->TickToMs(note.endTick) - startMs;

    CString s;
    s.Format(L"%s  (pitch %d)    |    start %ld ms    |    dur %ld ms    |    vel %d    |    ch %d",
             (LPCWSTR)CString(MidiDocument::MidiPitchName(note.pitch).c_str()),
             note.pitch, startMs, durMs, note.velocity, note.channel + 1);
    return s;
}

void CMainFrame::ShowNoteInfo(NoteRef ref)
{
    CString info = BuildNoteInfo(ref);
    if (info.IsEmpty())
        info = m_selInfoRef.IsValid() ? BuildNoteInfo(m_selInfoRef)
                                      : CString(L"Hover or click a note to see its details");
    m_statusBar.SetPaneText(1, info);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void CMainFrame::RecalcLayout(BOOL bNotify)
{
    CFrameWnd::RecalcLayout(bNotify);
    RepositionViews();
}

void CMainFrame::RepositionViews()
{
    if (!::IsWindow(m_view.m_hWnd)) return;

    CRect cr;
    GetClientRect(&cr);

    if (::IsWindow(m_statusBar.m_hWnd))
    {
        CRect sb;
        m_statusBar.GetWindowRect(&sb);
        ScreenToClient(&sb);
        cr.bottom = sb.top;
    }

    int W = cr.Width();
    int H = cr.Height();
    int L = cr.left;
    int T = cr.top;

    // Batch all child moves into one atomic operation so they don't repaint
    // one-after-another (which causes flicker during a sizer drag).
    const UINT FLAGS  = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS;
    const UINT FLAGS_S = FLAGS | SWP_SHOWWINDOW;

    HDWP hdwp = ::BeginDeferWindowPos(4);
    if (!hdwp) return;

    switch (m_dockMode)
    {
    case DockMode::Right:
    {
        int panelW = std::max(80, std::min(m_panelWidth, W - 150));
        int scoreW = std::max(50, W - panelW - SIZER_W);

        hdwp = ::DeferWindowPos(hdwp, m_scoreHeader.m_hWnd, nullptr,
                                L, T, scoreW, SCORE_HEADER_H, FLAGS_S);
        hdwp = ::DeferWindowPos(hdwp, m_view.m_hWnd, nullptr,
                                L, T + SCORE_HEADER_H, scoreW, H - SCORE_HEADER_H, FLAGS);
        hdwp = ::DeferWindowPos(hdwp, m_sizer.m_hWnd, nullptr,
                                L + scoreW, T, SIZER_W, H, FLAGS_S);
        if (::IsWindow(m_notePanel.m_hWnd))
            hdwp = ::DeferWindowPos(hdwp, m_notePanel.m_hWnd, nullptr,
                                    L + scoreW + SIZER_W, T, panelW, H, FLAGS_S);
        break;
    }
    case DockMode::Bottom:
    {
        int panelH = std::max(80, std::min(m_panelHeight, H - 100));
        int scoreH = std::max(50, H - panelH - SIZER_W);

        hdwp = ::DeferWindowPos(hdwp, m_scoreHeader.m_hWnd, nullptr,
                                L, T, W, SCORE_HEADER_H, FLAGS_S);
        hdwp = ::DeferWindowPos(hdwp, m_view.m_hWnd, nullptr,
                                L, T + SCORE_HEADER_H, W, scoreH - SCORE_HEADER_H, FLAGS);
        hdwp = ::DeferWindowPos(hdwp, m_sizer.m_hWnd, nullptr,
                                L, T + scoreH, W, SIZER_W, FLAGS_S);
        if (::IsWindow(m_notePanel.m_hWnd))
            hdwp = ::DeferWindowPos(hdwp, m_notePanel.m_hWnd, nullptr,
                                    L, T + scoreH + SIZER_W, W, panelH, FLAGS_S);
        break;
    }
    case DockMode::Float:
    {
        m_sizer.ShowWindow(SW_HIDE);
        hdwp = ::DeferWindowPos(hdwp, m_scoreHeader.m_hWnd, nullptr,
                                L, T, W, SCORE_HEADER_H, FLAGS_S);
        hdwp = ::DeferWindowPos(hdwp, m_view.m_hWnd, nullptr,
                                L, T + SCORE_HEADER_H, W, H - SCORE_HEADER_H, FLAGS);
        break;
    }
    }

    if (hdwp) ::EndDeferWindowPos(hdwp);
}

// ---------------------------------------------------------------------------
// Sizer bar messages
// ---------------------------------------------------------------------------

LRESULT CMainFrame::OnSizerDrag(WPARAM wParam, LPARAM)
{
    int delta = (int)(INT_PTR)wParam;

    if (m_dockMode == DockMode::Right)
    {
        // Sizer moved right (+delta) → panel shrinks
        m_panelWidth = std::max(80, m_panelWidth - delta);
    }
    else if (m_dockMode == DockMode::Bottom)
    {
        // Sizer moved down (+delta) → panel shrinks
        m_panelHeight = std::max(60, m_panelHeight - delta);
    }

    RepositionViews();
    return 0;
}

LRESULT CMainFrame::OnSizerRClick(WPARAM, LPARAM)
{
    CMenu menu;
    menu.CreatePopupMenu();
    UINT cRight  = (m_dockMode == DockMode::Right)  ? MF_CHECKED : MF_UNCHECKED;
    UINT cBottom = (m_dockMode == DockMode::Bottom) ? MF_CHECKED : MF_UNCHECKED;
    UINT cFloat  = (m_dockMode == DockMode::Float)  ? MF_CHECKED : MF_UNCHECKED;
    menu.AppendMenu(MF_STRING | cRight,  1, L"Dock Right");
    menu.AppendMenu(MF_STRING | cBottom, 2, L"Dock Bottom");
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING | cFloat,  3, L"Float");

    CPoint pt;
    GetCursorPos(&pt);
    int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, this);

    switch (cmd)
    {
    case 1: DockPanelRight();  break;
    case 2: DockPanelBottom(); break;
    case 3: FloatPanel();      break;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Float / dock
// ---------------------------------------------------------------------------

void CMainFrame::FloatPanel()
{
    if (m_dockMode == DockMode::Float) return;
    if (!::IsWindow(m_notePanel.m_hWnd)) return;

    // Use current panel screen rect for float window placement
    CRect panelRect;
    m_notePanel.GetWindowRect(&panelRect);

    auto* pFloat = new CPanelFloatFrame();
    if (!pFloat->Create(&m_notePanel, panelRect))
    {
        delete pFloat;
        return;
    }

    m_floatFrame = pFloat;
    m_dockMode   = DockMode::Float;
    RepositionViews();
}

void CMainFrame::DockPanelRight()
{
    if (m_dockMode == DockMode::Float && m_floatFrame &&
        ::IsWindow(m_floatFrame->m_hWnd))
    {
        // Preserve float window width as new panel width
        CRect rc;
        m_floatFrame->GetWindowRect(&rc);
        m_panelWidth = rc.Width();

        ::SetParent(m_notePanel.m_hWnd, m_hWnd);
        m_notePanel.ShowWindow(SW_SHOW);

        m_floatFrame->DestroyWindow();
        delete m_floatFrame;
        m_floatFrame = nullptr;
    }

    m_sizer.SetOrientation(CSizerBar::Orientation::Vertical);
    m_dockMode = DockMode::Right;
    RepositionViews();
}

void CMainFrame::DockPanelBottom()
{
    if (m_dockMode == DockMode::Float && m_floatFrame &&
        ::IsWindow(m_floatFrame->m_hWnd))
    {
        CRect rc;
        m_floatFrame->GetWindowRect(&rc);
        m_panelHeight = rc.Height();

        ::SetParent(m_notePanel.m_hWnd, m_hWnd);
        m_notePanel.ShowWindow(SW_SHOW);

        m_floatFrame->DestroyWindow();
        delete m_floatFrame;
        m_floatFrame = nullptr;
    }

    m_sizer.SetOrientation(CSizerBar::Orientation::Horizontal);
    m_dockMode = DockMode::Bottom;
    RepositionViews();
}

// ---------------------------------------------------------------------------
// Note selection sync
// ---------------------------------------------------------------------------

LRESULT CMainFrame::OnScoreNoteSelected(WPARAM wParam, LPARAM lParam)
{
    NoteRef ref{ (int)wParam, (int)lParam };
    m_notePanel.SetSelectedNote(ref);
    m_selInfoRef = ref;
    ShowNoteInfo(ref);
    return 0;
}

LRESULT CMainFrame::OnListNoteSelected(WPARAM wParam, LPARAM lParam)
{
    NoteRef ref{ (int)wParam, (int)lParam };
    m_view.SetSelectedNote(ref);
    m_view.ScrollToNote(ref);
    m_selInfoRef = ref;
    ShowNoteInfo(ref);
    return 0;
}

LRESULT CMainFrame::OnScoreNoteHover(WPARAM wParam, LPARAM lParam)
{
    // While hovering show the hovered note; otherwise fall back to the selection
    ShowNoteInfo(NoteRef{ (int)wParam, (int)lParam });
    return 0;
}



