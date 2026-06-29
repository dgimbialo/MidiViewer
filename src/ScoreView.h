#pragma once
#include "pch.h"
#include "MidiDocument.h"
#include "AppMessages.h"

// Global constant shared between CScoreHeaderBar and CMainFrame layout
static constexpr int SCORE_HEADER_H = 28;

// ---------------------------------------------------------------------------
// CScoreHeaderBar — docked child window of MainFrame showing Tempo + TimeSig.
// Lives above CScoreView; not affected by scroll at all.
// ---------------------------------------------------------------------------
class CScoreHeaderBar : public CWnd
{
public:
    void SetInfo(int bpm, int tsNum, int tsDen)
    {
        m_bpm = bpm; m_tsNum = tsNum; m_tsDen = tsDen;
        if (m_hWnd) Invalidate(FALSE);
    }

    // Extra read-out shown on the right of the header (bars / duration)
    void SetStats(int numBars, long durationMs, int numNotes)
    {
        m_numBars = numBars; m_durationMs = durationMs; m_numNotes = numNotes;
        if (m_hWnd) Invalidate(FALSE);
    }

protected:
    afx_msg BOOL OnEraseBkgnd(CDC*) { return TRUE; }
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()

private:
    int  m_bpm = 120, m_tsNum = 4, m_tsDen = 4;
    int  m_numBars   = 0;
    long m_durationMs = 0;
    int  m_numNotes  = 0;
};

// ---------------------------------------------------------------------------
// CScoreView — CScrollView that renders the MIDI score
//
// Layout:
//   - Each MIDI track occupies one "staff block" stacked vertically
//   - Each staff block: 5 horizontal staff lines (treble clef)
//   - Notes are thick horizontal lines positioned by pitch and tick time
//   - X axis: ticks → pixels (zoom = m_pixelsPerBeat)
//   - Y axis: MIDI pitch → staff position (treble clef mapping)
// ---------------------------------------------------------------------------
class CScoreView : public CScrollView
{
public:
    CScoreView();
    virtual ~CScoreView();

    void LoadDocument(MidiDocument&& doc);

    // Returns pointer to the owned document (for NoteListPanel)
    const MidiDocument* GetDocument() const { return m_hasDoc ? &m_doc : nullptr; }

    // Called by MainFrame to sync selection coming from NoteListPanel
    void SetSelectedNote(NoteRef ref);
    void ScrollToNote(NoteRef ref);

protected:
    virtual void OnDraw(CDC* pDC) override;
    virtual void OnInitialUpdate() override;
    // CView::PostNcDestroy() does 'delete this'. This view is an embedded
    // member of CMainFrame, so deleting it would corrupt the heap on close.
    virtual void PostNcDestroy() override {}

    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg LRESULT OnMouseLeave(WPARAM wParam, LPARAM lParam);
    afx_msg void OnVZoomIn();
    afx_msg void OnVZoomOut();
    afx_msg void OnVZoomReset();

    DECLARE_MESSAGE_MAP()

private:
    // ---- Data ----
    MidiDocument m_doc;
    bool         m_hasDoc      = false;
    NoteRef      m_selectedNote;          // currently selected note (-1/-1 = none)
    NoteRef      m_hoverNote;             // note under the mouse cursor (-1/-1 = none)
    bool         m_trackingLeave = false; // TrackMouseEvent armed for WM_MOUSELEAVE

    // Mouse position (document coords) for the per-bar tick ruler
    bool         m_hasMouse  = false;
    int          m_mouseDocX = 0;
    int          m_mouseDocY = 0;

    // ---- Layout constants ----
    static constexpr int LINE_SPACING     = 16;  // 8px per semitone — note lanes
    static constexpr int STAFF_HEIGHT     = LINE_SPACING * 4;
    static constexpr int TRACK_MARGIN_TOP = 115;

    // Dynamic top of the (single) track's staff, recomputed so the note band is
    // centred vertically in the window. Replaces the fixed TRACK_MARGIN_TOP.
    int  m_staffTop    = TRACK_MARGIN_TOP;
    bool m_recalcGuard = false;   // guards against OnSize/SetScrollSizes recursion
    static constexpr int TRACK_SPACING    = 80;
    static constexpr int LEFT_MARGIN      = 128;  // room for wrapped track names
    static constexpr int RIGHT_MARGIN     = 80;
    static constexpr int NOTE_THICKNESS   = 3;

    // ---- Zoom ----
    int m_pixelsPerBeat = 80;  // default zoom: 80px per quarter note
    static constexpr int ZOOM_MIN  = 20;
    static constexpr int ZOOM_MAX  = 400;
    static constexpr int ZOOM_STEP = 10;

    // ---- Helpers ----
    void  RecalcScrollSizes();
    int   TickToPixelX(long tick) const;
    int   PitchToPixelY(int pitch, int staffTopY) const;
    int   StaffTopY(int trackIndex) const;
    long  BarLengthTicks(int trackIndex) const;

    void DrawTrackBand(CDC* pDC, int trackIndex, int totalWidth);
    void DrawTrackBoundaries(CDC* pDC, int trackIndex, int totalWidth);
    void DrawTickRuler(CDC* pDC);   // ruler for the bar under the mouse cursor

    // Zoom keeping the viewport centre anchored on the same tick
    void ApplyZoom(int newPixelsPerBeat);
    void DrawBarLines(CDC* pDC, int trackIndex, int totalWidth);
    void DrawBarNumbers(CDC* pDC, int trackIndex);
    void DrawTrackLabel(CDC* pDC, int trackIndex);
    void DrawNotes(CDC* pDC, int trackIndex, const CRect& clipRect);
    void DrawLedgerLines(CDC* pDC, int pitch, int x1, int x2, int staffTopY);
    void GetTrackYBounds(int trackIndex, int& outTop, int& outBottom) const;

    // Hit-test a document-coordinate point against all notes
    NoteRef HitTestNote(CPoint ptDoc) const;

    // Invalidate just the client rect occupied by one note (cheap repaint)
    void InvalidateNote(NoteRef ref);

    static constexpr int HIT_RADIUS = NOTE_THICKNESS + 5; // px tolerance for click

    // Returns bar number (0-based) and pixel X for each bar
    struct BarInfo { int barNum; long startTick; long endTick; int x1; int x2; };
    std::vector<BarInfo> BuildBarList() const;
};
