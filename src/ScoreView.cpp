#include "pch.h"
#include "ScoreView.h"
#include "resource.h"
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// CScoreHeaderBar
// ---------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CScoreHeaderBar, CWnd)
    ON_WM_ERASEBKGND()
    ON_WM_PAINT()
END_MESSAGE_MAP()

void CScoreHeaderBar::OnPaint()
{
    CPaintDC paintDC(this);
    CRect rc;
    GetClientRect(&rc);
    if (rc.IsRectEmpty()) return;

    // Render off-screen to avoid flicker, then blit once.
    CDC memDC;
    memDC.CreateCompatibleDC(&paintDC);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&paintDC, rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    CDC& dc = memDC;   // draw everything below into the off-screen DC

    // Subtle vertical gradient background
    TRIVERTEX v[2] = {};
    v[0].x = rc.left;  v[0].y = rc.top;
    v[0].Red = 247 << 8; v[0].Green = 249 << 8; v[0].Blue = 253 << 8;
    v[1].x = rc.right; v[1].y = rc.bottom;
    v[1].Red = 232 << 8; v[1].Green = 237 << 8; v[1].Blue = 246 << 8;
    GRADIENT_RECT gr = { 0, 1 };
    GradientFill(dc.GetSafeHdc(), v, 2, &gr, 1, GRADIENT_FILL_RECT_V);

    // Bottom separator
    CPen penSep(PS_SOLID, 1, RGB(196, 204, 220));
    CPen* oldPen = dc.SelectObject(&penSep);
    dc.MoveTo(rc.left,  rc.bottom - 1);
    dc.LineTo(rc.right, rc.bottom - 1);
    dc.SelectObject(oldPen);

    dc.SetBkMode(TRANSPARENT);

    HFONT hFont = CreateFont(
        -14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT hOld = (HFONT)dc.SelectObject(hFont);
    int textY = rc.top + (rc.Height() - 14) / 2;

    // Tempo + TimeSig (left)
    CString text;
    text.Format(L"Tempo - %d BPM        Time Signature - %d/%d", m_bpm, m_tsNum, m_tsDen);
    dc.SetTextColor(RGB(40, 40, 80));
    dc.TextOut(rc.left + 64, textY, text);

    // Stats (right): bars, notes, duration - only when a file is loaded
    if (m_numBars > 0 || m_numNotes > 0)
    {
        long totalSec = m_durationMs / 1000;
        CString stats;
        stats.Format(L"%d bars    |    %d notes    |    %ld:%02ld",
                     m_numBars, m_numNotes, totalSec / 60, totalSec % 60);

        CSize sz = dc.GetTextExtent(stats);
        dc.SetTextColor(RGB(90, 100, 120));
        dc.TextOut(rc.right - sz.cx - 14, textY, stats);
    }

    dc.SelectObject(hOld);
    DeleteObject(hFont);

    // Blit the finished off-screen image to the window
    paintDC.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

// ---------------------------------------------------------------------------
// CScoreView
// ---------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CScoreView, CScrollView)
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEWHEEL()
    ON_WM_KEYDOWN()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_MESSAGE(WM_MOUSELEAVE, &CScoreView::OnMouseLeave)
    ON_COMMAND(ID_VIEW_ZOOM_IN,    &CScoreView::OnVZoomIn)
    ON_COMMAND(ID_VIEW_ZOOM_OUT,   &CScoreView::OnVZoomOut)
    ON_COMMAND(ID_VIEW_ZOOM_RESET, &CScoreView::OnVZoomReset)
END_MESSAGE_MAP()

// ---------------------------------------------------------------------------
// Treble clef pitch mapping
// Staff lines from bottom to top: E4=64, G4=67, B4=71, D5=74, F5=77
// Each semitone moves up by LINE_SPACING/2 pixels
// staffBottom = staffTopY + STAFF_HEIGHT  (bottom staff line)
// noteY = staffBottom - (pitch - 64) * halfStep
// ---------------------------------------------------------------------------

CScoreView::CScoreView()
{
}

CScoreView::~CScoreView()
{
}

// ---------------------------------------------------------------------------

void CScoreView::LoadDocument(MidiDocument&& doc)
{
    m_doc    = std::move(doc);
    m_hasDoc = true;
    RecalcScrollSizes();
    Invalidate();
}

// ---------------------------------------------------------------------------

BOOL CScoreView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CScoreView::OnInitialUpdate()
{
    CScrollView::OnInitialUpdate();
    SetScrollSizes(MM_TEXT, CSize(800, 600));
}

// ---------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------

int CScoreView::TickToPixelX(long tick) const
{
    if (m_doc.tpqn <= 0) return LEFT_MARGIN;
    return LEFT_MARGIN + MidiDocument::TickToPixel(tick, m_pixelsPerBeat, m_doc.tpqn);
}

int CScoreView::StaffTopY(int trackIndex) const
{
    return m_staffTop + trackIndex * (STAFF_HEIGHT + TRACK_SPACING);
}

// Convert MIDI pitch to pixel Y within a staff block
// staffTopY = top of the first (highest) staff line
// staffBottom = staffTopY + STAFF_HEIGHT (bottom = E4=64)
int CScoreView::PitchToPixelY(int pitch, int staffTopY) const
{
    int staffBottom = staffTopY + STAFF_HEIGHT;
    int halfStep    = LINE_SPACING / 2;  // 5px per semitone
    // E4 = MIDI 64 is on the bottom staff line
    return staffBottom - (pitch - 64) * halfStep;
}

long CScoreView::BarLengthTicks(int /*trackIndex*/) const
{
    // Use first time sig for simplicity (changes handled in BuildBarList)
    int num = 4, den = 4;
    if (!m_doc.timeSigs.empty())
    {
        num = m_doc.timeSigs[0].numerator;
        den = m_doc.timeSigs[0].denominator;
    }
    return MidiDocument::BarLengthTicks(num, den, m_doc.tpqn);
}

// ---------------------------------------------------------------------------

std::vector<CScoreView::BarInfo> CScoreView::BuildBarList() const
{
    std::vector<BarInfo> bars;
    if (m_doc.tpqn <= 0 || m_doc.totalTicks <= 0) return bars;

    long   tick       = 0;
    int    barNum     = 0;
    size_t tsIdx      = 0;

    int curNum = 4, curDen = 4;
    if (!m_doc.timeSigs.empty())
    {
        curNum = m_doc.timeSigs[0].numerator;
        curDen = m_doc.timeSigs[0].denominator;
        tsIdx  = 1;
    }

    long endLimit = m_doc.totalTicks + MidiDocument::BarLengthTicks(curNum, curDen, m_doc.tpqn);

    while (tick <= endLimit)
    {
        // Advance time sig if needed
        while (tsIdx < m_doc.timeSigs.size() && m_doc.timeSigs[tsIdx].tick <= tick)
        {
            curNum = m_doc.timeSigs[tsIdx].numerator;
            curDen = m_doc.timeSigs[tsIdx].denominator;
            ++tsIdx;
        }

        long barLen = MidiDocument::BarLengthTicks(curNum, curDen, m_doc.tpqn);
        if (barLen <= 0) break;

        BarInfo bi;
        bi.barNum    = barNum;
        bi.startTick = tick;
        bi.endTick   = tick + barLen;
        bi.x1        = TickToPixelX(tick);
        bi.x2        = TickToPixelX(tick + barLen);
        bars.push_back(bi);

        tick += barLen;
        ++barNum;
    }
    return bars;
}

// ---------------------------------------------------------------------------

void CScoreView::RecalcScrollSizes()
{
    int totalWidth = TickToPixelX(m_doc.totalTicks) + RIGHT_MARGIN;
    if (totalWidth < 200) totalWidth = 200;

    // ---- Vertical layout: centre the (single) track's note band in the window.
    const int PAD        = 10;   // matches GetTrackYBounds padding
    const int TOP_MARGIN = 76;   // room above for bar numbers + tick ruler
    const int BOT_MARGIN = 40;
    const int halfStep   = LINE_SPACING / 2;

    int minPitch = 64, maxPitch = 64;
    if (!m_doc.tracks.empty() && !m_doc.tracks[0].notes.empty())
    {
        minPitch = 127; maxPitch = 0;
        for (const auto& n : m_doc.tracks[0].notes)
        {
            minPitch = std::min(minPitch, n.pitch);
            maxPitch = std::max(maxPitch, n.pitch);
        }
    }
    int bandH = (maxPitch - minPitch) * halfStep + 2 * PAD;

    CRect rc;
    GetClientRect(&rc);
    int clientH = rc.Height();
    int needed  = bandH + TOP_MARGIN + BOT_MARGIN;

    int bandTopY, totalHeight;
    if (needed <= clientH)
    {
        bandTopY    = (clientH - bandH) / 2;   // band centred vertically
        totalHeight = clientH;                 // fits — no vertical scrolling
    }
    else
    {
        bandTopY    = TOP_MARGIN;              // taller than window — scroll
        totalHeight = needed;
    }

    // Choose m_staffTop so that GetTrackYBounds(track 0).top == bandTopY:
    //   PitchToPixelY(maxPitch) - PAD == bandTopY
    m_staffTop = bandTopY + PAD - STAFF_HEIGHT + (maxPitch - 64) * halfStep;

    SetScrollSizes(MM_TEXT, CSize(totalWidth, totalHeight));
}

// ---------------------------------------------------------------------------
// Re-centre vertically whenever the view is resized.
// ---------------------------------------------------------------------------
void CScoreView::OnSize(UINT nType, int cx, int cy)
{
    CScrollView::OnSize(nType, cx, cy);
    if (m_hasDoc && !m_recalcGuard)
    {
        m_recalcGuard = true;
        RecalcScrollSizes();
        Invalidate(FALSE);
        m_recalcGuard = false;
    }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Double-buffered paint: render the visible region into an off-screen bitmap,
// then blit it in one go. Eliminates flicker during resize / sizer drag.
// ---------------------------------------------------------------------------
void CScoreView::OnPaint()
{
    CPaintDC dc(this);

    CRect rcClient;
    GetClientRect(&rcClient);
    if (rcClient.IsRectEmpty()) return;

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, rcClient.Width(), rcClient.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    // Apply the same scroll mapping CScrollView would, so OnDraw's logical
    // coordinates land correctly inside the client-sized bitmap.
    OnPrepareDC(&memDC);
    OnDraw(&memDC);

    // Reset mapping and copy the rendered pixels to the screen.
    memDC.SetViewportOrg(0, 0);
    dc.BitBlt(0, 0, rcClient.Width(), rcClient.Height(), &memDC, 0, 0, SRCCOPY);

    memDC.SelectObject(pOldBmp);
}

void CScoreView::OnDraw(CDC* pDC)
{
    CRect clipRect;
    pDC->GetClipBox(&clipRect);

    // White background
    pDC->FillSolidRect(&clipRect, RGB(255, 255, 255));

    if (!m_hasDoc || m_doc.tracks.empty())
    {
        // Friendly empty state - large title + hint, centred in the viewport
        CRect rc;
        GetClientRect(&rc);
        pDC->SetBkMode(TRANSPARENT);

        HFONT hBig = CreateFont(-40, 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT hOld = (HFONT)pDC->SelectObject(hBig);
        pDC->SetTextColor(RGB(205, 213, 226));
        CRect rcGlyph = rc; rcGlyph.bottom = rc.CenterPoint().y;
        pDC->DrawText(L"Simple MIDI Viewer", &rcGlyph, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
        pDC->SelectObject(hOld);
        DeleteObject(hBig);

        HFONT hTxt = CreateFont(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        hOld = (HFONT)pDC->SelectObject(hTxt);
        pDC->SetTextColor(RGB(150, 160, 175));
        CRect rcTxt = rc; rcTxt.top = rc.CenterPoint().y + 12;
        pDC->DrawText(L"Open a MIDI file to display the score   (File > Open,  Ctrl+O)",
                      &rcTxt, DT_CENTER | DT_TOP | DT_SINGLELINE);
        pDC->SelectObject(hOld);
        DeleteObject(hTxt);
        return;
    }

    int numTracks  = (int)m_doc.tracks.size();
    int totalWidth = TickToPixelX(m_doc.totalTicks) + RIGHT_MARGIN;

    for (int t = 0; t < numTracks; ++t)
    {
        DrawTrackBand(pDC, t, totalWidth);
        DrawTrackBoundaries(pDC, t, totalWidth);
        DrawBarLines(pDC, t, totalWidth);
        if (t == 0) DrawBarNumbers(pDC, t);
        DrawTrackLabel(pDC, t);
        DrawNotes(pDC, t, clipRect);
    }

    // Tick ruler for the bar under the mouse — drawn last so it sits on top.
    DrawTickRuler(pDC);
}

// ---------------------------------------------------------------------------
// Light tinted band behind each track's note region, so stacked tracks read
// as distinct horizontal lanes.
// ---------------------------------------------------------------------------
void CScoreView::DrawTrackBand(CDC* pDC, int trackIndex, int totalWidth)
{
    int topY, bottomY;
    GetTrackYBounds(trackIndex, topY, bottomY);

    const int PAD = 6;
    CRect band(LEFT_MARGIN, topY - PAD, totalWidth, bottomY + PAD);

    // Rounded, very light tinted lane
    CBrush br(RGB(247, 249, 253));
    CPen   pen(PS_SOLID, 1, RGB(228, 233, 242));
    CBrush* oldBr = pDC->SelectObject(&br);
    CPen*   oldPen = pDC->SelectObject(&pen);
    pDC->RoundRect(band.left, band.top, band.right, band.bottom, 10, 10);
    pDC->SelectObject(oldBr);
    pDC->SelectObject(oldPen);
}

// ---------------------------------------------------------------------------
// Compute top/bottom Y pixel bounds that enclose all notes in a track.
// Adds padding so the boundary lines sit a little outside the extreme notes.
// ---------------------------------------------------------------------------
void CScoreView::GetTrackYBounds(int trackIndex, int& outTop, int& outBottom) const
{
    const int PAD       = 10;  // px above/below extreme notes
    int       staffTop  = StaffTopY(trackIndex);

    const MidiTrack& track = m_doc.tracks[trackIndex];
    if (track.notes.empty())
    {
        outTop    = staffTop;
        outBottom = staffTop + STAFF_HEIGHT;
        return;
    }

    int minPitch = 127, maxPitch = 0;
    for (const auto& n : track.notes)
    {
        minPitch = std::min(minPitch, n.pitch);
        maxPitch = std::max(maxPitch, n.pitch);
    }

    // Higher pitch → smaller Y (drawn higher on screen)
    outTop    = PitchToPixelY(maxPitch, staffTop) - PAD;
    outBottom = PitchToPixelY(minPitch, staffTop) + PAD;
}

// ---------------------------------------------------------------------------
// Draw two horizontal boundary lines that bracket all notes in this track.
// (Replaces the old 5-line staff.)
// ---------------------------------------------------------------------------
void CScoreView::DrawTrackBoundaries(CDC* pDC, int trackIndex, int totalWidth)
{
    int topY, bottomY;
    GetTrackYBounds(trackIndex, topY, bottomY);

    CPen  pen(PS_SOLID, 1, RGB(140, 140, 140));
    CPen* oldPen = pDC->SelectObject(&pen);

    pDC->MoveTo(LEFT_MARGIN, topY);    pDC->LineTo(totalWidth, topY);
    pDC->MoveTo(LEFT_MARGIN, bottomY); pDC->LineTo(totalWidth, bottomY);

    pDC->SelectObject(oldPen);
}

// ---------------------------------------------------------------------------

void CScoreView::DrawBarLines(CDC* pDC, int trackIndex, int totalWidth)
{
    int lineTop, lineBottom;
    GetTrackYBounds(trackIndex, lineTop, lineBottom);

    CPen pen(PS_SOLID, 1, RGB(90, 90, 90));   // darker = clearly visible
    CPen* oldPen = pDC->SelectObject(&pen);

    auto bars = BuildBarList();
    for (const auto& b : bars)
    {
        pDC->MoveTo(b.x1, lineTop);
        pDC->LineTo(b.x1, lineBottom);
    }
    // Final double barline
    if (!bars.empty())
    {
        CPen penFinal(PS_SOLID, 2, RGB(40, 40, 40));
        pDC->SelectObject(&penFinal);
        pDC->MoveTo(bars.back().x2, lineTop);
        pDC->LineTo(bars.back().x2, lineBottom);
    }

    pDC->SelectObject(oldPen);
}

// ---------------------------------------------------------------------------

void CScoreView::DrawBarNumbers(CDC* pDC, int trackIndex)
{
    // Place bar numbers just above the dynamic top boundary of this track
    int topY, bottomY;
    GetTrackYBounds(trackIndex, topY, bottomY);

    const int FONT_H  = 15;   // font height (px)
    const int GAP     = 3;    // px between number bottom and boundary line
    int y = topY - FONT_H - GAP;

    pDC->SetBkMode(TRANSPARENT);
    pDC->SetTextColor(RGB(60, 60, 60));

    HFONT hFont = CreateFont(-FONT_H, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldFont = (HFONT)pDC->SelectObject(hFont);

    CRect clipRect;
    pDC->GetClipBox(&clipRect);

    auto bars = BuildBarList();
    for (const auto& b : bars)
    {
        if (b.x1 > clipRect.right)  break;
        if (b.x2 < clipRect.left)   continue;

        CString num;
        num.Format(L"%d", b.barNum + 1);
        pDC->TextOut(b.x1 + 3, y, num);
    }

    pDC->SelectObject(oldFont);
    DeleteObject(hFont);
}

// ---------------------------------------------------------------------------

void CScoreView::DrawTrackLabel(CDC* pDC, int trackIndex)
{
    // Vertical centre of the track's note region
    int topY, bottomY;
    GetTrackYBounds(trackIndex, topY, bottomY);
    int centerY = (topY + bottomY) / 2;

    pDC->SetBkMode(TRANSPARENT);
    pDC->SetTextColor(RGB(60, 60, 180));

    // Font 1pt larger than before (13 -> 14)
    HFONT hFont = CreateFont(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldFont = (HFONT)pDC->SelectObject(hFont);

    // Track name (ToWide handles UTF-8 / Latin-1 encoding)
    const auto& track = m_doc.tracks[trackIndex];
    CString name(MidiDocument::ToWide(track.name).c_str());
    if (name.IsEmpty())
        name.Format(L"Track %d", trackIndex + 1);

    // Wrap the name to fit inside the left margin (multi-line). DT_WORDBREAK
    // also splits a single word that is wider than the box, so long names like
    // "PiobMasterPro recording" no longer spill over onto the track.
    const int LABEL_X = 6;
    const int LABEL_W = LEFT_MARGIN - LABEL_X - 8;   // keep a gap before notes

    // Measure wrapped height, then position the block centred on the track.
    CRect rcCalc(0, 0, LABEL_W, 0);
    pDC->DrawText(name, &rcCalc, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);

    CRect rcText(LABEL_X, centerY - rcCalc.Height() / 2,
                 LABEL_X + LABEL_W, centerY + rcCalc.Height() / 2 + 1);
    // DT_NOCLIP guarantees the full text is painted even if the block is taller
    // than the computed rect by a pixel.
    pDC->DrawText(name, &rcText, DT_LEFT | DT_WORDBREAK | DT_TOP | DT_NOCLIP);

    pDC->SelectObject(oldFont);
    DeleteObject(hFont);
}

// ---------------------------------------------------------------------------

void CScoreView::DrawNotes(CDC* pDC, int trackIndex, const CRect& clipRect)
{
    const MidiTrack& track    = m_doc.tracks[trackIndex];
    int              staffTop = StaffTopY(trackIndex);

    // Tie note height to the per-semitone lane height, leaving a small gap so
    // notes one semitone apart never overlap vertically.
    const int halfStep = LINE_SPACING / 2;            // px per semitone
    const int NOTE_H   = std::max(3, halfStep - 2);   // < lane height → gap
    const int half     = NOTE_H / 2;

    auto lighten = [](COLORREF c, double f) -> COLORREF {
        int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
        r = (int)(r + (255 - r) * f);
        g = (int)(g + (255 - g) * f);
        b = (int)(b + (255 - b) * f);
        return RGB(r, g, b);
    };

    for (int n = 0; n < (int)track.notes.size(); ++n)
    {
        const auto& note = track.notes[n];

        int x1 = TickToPixelX(note.startTick);
        int x2 = TickToPixelX(note.endTick);

        if (x2 < clipRect.left || x1 > clipRect.right) continue;
        if (x2 <= x1 + 3) x2 = x1 + 3;     // keep very short notes visible

        int y = PitchToPixelY(note.pitch, staffTop);

        bool isSelected = (m_selectedNote == NoteRef{ trackIndex, n });
        bool isHover    = (m_hoverNote    == NoteRef{ trackIndex, n });

        // Fill colour: indigo shaded by velocity; amber on hover; red on select
        COLORREF fill, border;
        if (isSelected)
        {
            fill   = RGB(229, 57, 53);
            border = RGB(160, 20, 18);
        }
        else if (isHover)
        {
            fill   = RGB(255, 167, 38);
            border = RGB(204, 110, 0);
        }
        else
        {
            // velocity 1..127 -> brightness blend (light indigo -> deep indigo)
            double t = note.velocity / 127.0;
            if (t < 0.0) t = 0.0;
            if (t > 1.0) t = 1.0;
            int r = (int)(150 + (57  - 150) * t);
            int g = (int)(170 + (86  - 170) * t);
            int b = (int)(224 + (192 - 224) * t);
            fill   = RGB(r, g, b);
            border = RGB(r * 65 / 100, g * 65 / 100, b * 80 / 100);
        }

        CRect rcNote(x1, y - half, x2, y + half + 1);

        // Soft drop shadow underneath for depth (kept within the lane gap)
        {
            CRect rcSh = rcNote;  rcSh.OffsetRect(1, 1);
            pDC->FillSolidRect(&rcSh, RGB(214, 220, 232));
        }

        CBrush brFill(fill);
        CPen   penBorder(PS_SOLID, 1, border);
        CBrush* oldBr  = pDC->SelectObject(&brFill);
        CPen*   oldPen = pDC->SelectObject(&penBorder);

        // Straight (rectangular) note body
        pDC->Rectangle(rcNote.left, rcNote.top, rcNote.right, rcNote.bottom);

        // Glossy highlight: a lighter line just inside the top edge
        if (rcNote.Width() > NOTE_H + 2)
        {
            CPen penHi(PS_SOLID, 1, lighten(fill, 0.55));
            pDC->SelectObject(&penHi);
            int hy = rcNote.top + 2;
            pDC->MoveTo(rcNote.left + 2, hy);
            pDC->LineTo(rcNote.right - 2, hy);
            pDC->SelectObject(&penBorder);
        }

        pDC->SelectObject(oldBr);
        pDC->SelectObject(oldPen);
    }

    // Restore default pen
    pDC->SelectObject(GetStockObject(BLACK_PEN));
}

// ---------------------------------------------------------------------------
// Tick ruler — shown only inside the bar currently under the mouse cursor.
// Marks each beat (with absolute tick labels) and sub-beats, plus a live
// cursor line and the exact tick value at the pointer.
// ---------------------------------------------------------------------------
void CScoreView::DrawTickRuler(CDC* pDC)
{
    if (!m_hasMouse || !m_hasDoc || m_doc.tracks.empty()) return;
    if (m_mouseDocX < LEFT_MARGIN || m_doc.tpqn <= 0)      return;

    auto bars = BuildBarList();
    if (bars.empty()) return;

    const BarInfo* bar = nullptr;
    for (const auto& b : bars)
        if (m_mouseDocX >= b.x1 && m_mouseDocX < b.x2) { bar = &b; break; }
    if (!bar) return;

    int topY, botY;
    GetTrackYBounds(0, topY, botY);

    int num = 4, den = 4;
    m_doc.GetTimeSigAt(bar->startTick, num, den);
    if (num < 1) num = 1;
    long barLen       = bar->endTick - bar->startTick;
    long ticksPerBeat = barLen / num;
    if (ticksPerBeat <= 0) return;

    // Ruler axis raised well above the bar.
    int rulerY = topY - 34;
    if (rulerY < 16) rulerY = 16;

    HFONT hFont = CreateFont(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT hOldFont = (HFONT)pDC->SelectObject(hFont);
    pDC->SetBkMode(TRANSPARENT);

    // Ruler axis across the bar
    CPen penAxis(PS_SOLID, 1, RGB(150, 162, 184));
    CPen* oldPen = pDC->SelectObject(&penAxis);
    pDC->MoveTo(bar->x1, rulerY);
    pDC->LineTo(bar->x2, rulerY);

    // Vertical beat-division lines through the note band, so the user can see
    // exactly where each beat falls. Interior beats only (the bar edges are
    // already drawn as solid bar lines). Dashed + light so notes stay readable.
    {
        LOGBRUSH lb{ BS_SOLID, RGB(170, 184, 212), 0 };
        HPEN penBeat = ExtCreatePen(PS_COSMETIC | PS_DOT, 1, &lb, 0, nullptr);
        HPEN oldBeat = (HPEN)pDC->SelectObject(penBeat);
        for (int b = 1; b < num; ++b)
        {
            int x = TickToPixelX(bar->startTick + (long)b * ticksPerBeat);
            pDC->MoveTo(x, topY);
            pDC->LineTo(x, botY);
        }
        pDC->SelectObject(oldBeat);
        DeleteObject(penBeat);
    }

    // Beat marks + labels — RELATIVE to the bar (start at 0).
    pDC->SetTextColor(RGB(105, 116, 140));
    for (int b = 0; b <= num; ++b)
    {
        long relTick = (long)b * ticksPerBeat;          // 0, tpb, 2*tpb, ...
        int  x       = TickToPixelX(bar->startTick + relTick);

        pDC->MoveTo(x, rulerY - 6);
        pDC->LineTo(x, rulerY);

        CString s; s.Format(L"%ld", relTick);
        pDC->TextOut(x + 2, rulerY - 17, s);

        // sub-beat ticks (quarter subdivisions)
        if (b < num)
            for (int sd = 1; sd < 4; ++sd)
            {
                int xs = TickToPixelX(bar->startTick + relTick + sd * ticksPerBeat / 4);
                pDC->MoveTo(xs, rulerY - 3);
                pDC->LineTo(xs, rulerY);
            }
    }

    // Live cursor line through the band
    CPen penCur(PS_SOLID, 1, RGB(255, 120, 0));
    pDC->SelectObject(&penCur);
    pDC->MoveTo(m_mouseDocX, rulerY);
    pDC->LineTo(m_mouseDocX, botY + 6);

    // Cursor position, RELATIVE to the bar, decomposed into beat + tick-in-beat.
    long curAbs = (long)((double)(m_mouseDocX - LEFT_MARGIN) * m_doc.tpqn / m_pixelsPerBeat);
    long curRel = curAbs - bar->startTick;
    if (curRel < 0)        curRel = 0;
    if (curRel > barLen)   curRel = barLen;
    int  beatNo = (int)(curRel / ticksPerBeat) + 1;     // 1-based beat
    long tickOf = curRel % ticksPerBeat;                // tick within the beat

    // Readout pill: full within-bar tick (0..barLen) + beat : tick-in-beat.
    // The within-bar tick makes the scale unambiguous (reaches the bar length).
    CString pillText;
    pillText.Format(L"tick %ld / %ld    beat %d : %ld",
                    curRel, barLen, beatNo, tickOf);
    CSize sz = pDC->GetTextExtent(pillText);
    int pillH   = sz.cy + 4;
    int pillTop = rulerY - 16 - pillH;
    if (pillTop < 1) pillTop = 1;

    // Anchor left of the cursor if the pill would overflow the bar's right edge.
    int pillLeft = m_mouseDocX + 4;
    int pillW    = sz.cx + 12;
    if (pillLeft + pillW > bar->x2) pillLeft = m_mouseDocX - 4 - pillW;
    CRect pill(pillLeft, pillTop, pillLeft + pillW, pillTop + pillH);

    CBrush brPill(RGB(255, 140, 0));
    CBrush* ob = pDC->SelectObject(&brPill);
    pDC->SelectObject(&penCur);
    pDC->Rectangle(pill.left, pill.top, pill.right, pill.bottom);
    pDC->SetTextColor(RGB(255, 255, 255));
    pDC->DrawText(pillText, &pill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    pDC->SelectObject(ob);

    pDC->SelectObject(oldPen);
    pDC->SelectObject(hOldFont);
    DeleteObject(hFont);
}

// ---------------------------------------------------------------------------
// Draw ledger lines for notes outside the staff (below E4=64 or above F5=77)
// ---------------------------------------------------------------------------
void CScoreView::DrawLedgerLines(CDC* pDC, int pitch, int x1, int x2,
                                  int staffTopY)
{
    CPen ledgerPen(PS_SOLID, 1, RGB(0, 0, 0));
    CPen* oldPen = pDC->SelectObject(&ledgerPen);

    int staffBottom = staffTopY + STAFF_HEIGHT;
    int halfStep    = LINE_SPACING / 2;
    int cx          = (x1 + x2) / 2;
    int ledgerHalf  = 6; // half-width of ledger line

    auto drawLedger = [&](int py) {
        pDC->MoveTo(cx - ledgerHalf, py);
        pDC->LineTo(cx + ledgerHalf, py);
    };

    if (pitch < 64)
    {
        // Below bottom staff line (E4=64): add ledger lines at D4(62), C4(60), B3(59)...
        // Lines appear at even semitones below E4: 62(D4), 60(C4/middle C), 58, ...
        for (int p = 62; p >= pitch - 1; p -= 2)
        {
            int py = staffBottom - (p - 64) * halfStep;
            drawLedger(py);
        }
    }
    else if (pitch > 77)
    {
        // Above top staff line (F5=77): add ledger lines at G5(79), A5(81)...
        // Lines at odd pitches above F5: 79(G5), 81(A5), ...
        for (int p = 79; p <= pitch + 1; p += 2)
        {
            int py = staffBottom - (p - 64) * halfStep;
            drawLedger(py);
        }
    }

    pDC->SelectObject(oldPen);
}

// ---------------------------------------------------------------------------
// Zoom
// ---------------------------------------------------------------------------

BOOL CScoreView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (nFlags & MK_CONTROL)
    {
        if (zDelta > 0)
            OnVZoomIn();
        else
            OnVZoomOut();
        return TRUE;
    }
    return CScrollView::OnMouseWheel(nFlags, zDelta, pt);
}

// ---------------------------------------------------------------------------
// Change zoom while keeping the tick currently at the viewport centre fixed,
// so the view doesn't jump away as you zoom in/out.
// ---------------------------------------------------------------------------
void CScoreView::ApplyZoom(int newPixelsPerBeat)
{
    newPixelsPerBeat = std::max(ZOOM_MIN, std::min(newPixelsPerBeat, ZOOM_MAX));
    if (newPixelsPerBeat == m_pixelsPerBeat) return;

    long tpqn = (m_doc.tpqn > 0) ? m_doc.tpqn : 480;

    CRect rc;
    GetClientRect(&rc);
    CPoint scroll = GetScrollPosition();

    // Tick currently shown at the horizontal centre of the viewport
    int    centerDocX = scroll.x + rc.Width() / 2;
    double centerTick = double(centerDocX - LEFT_MARGIN) * tpqn / m_pixelsPerBeat;

    m_pixelsPerBeat = newPixelsPerBeat;
    if (m_hasDoc) RecalcScrollSizes();

    // Scroll so that same tick lands back at the centre
    int newCenterDocX = LEFT_MARGIN + (int)(centerTick * m_pixelsPerBeat / tpqn);
    int newScrollX    = std::max(0, newCenterDocX - rc.Width() / 2);
    ScrollToPosition(CPoint(newScrollX, GetScrollPosition().y));
    Invalidate();
}

void CScoreView::OnVZoomIn()  { ApplyZoom(m_pixelsPerBeat + ZOOM_STEP); }
void CScoreView::OnVZoomOut() { ApplyZoom(m_pixelsPerBeat - ZOOM_STEP); }
void CScoreView::OnVZoomReset() { ApplyZoom(80); }

// ---------------------------------------------------------------------------

void CScoreView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    const int SCROLL_STEP = 40;
    SCROLLINFO si{};
    si.cbSize = sizeof(si);

    switch (nChar)
    {
    case VK_LEFT:
        GetScrollInfo(SB_HORZ, &si);
        SetScrollPos(SB_HORZ, std::max(0, si.nPos - SCROLL_STEP));
        ScrollToPosition(CPoint(std::max(0, si.nPos - SCROLL_STEP), GetScrollPosition().y));
        break;
    case VK_RIGHT:
        GetScrollInfo(SB_HORZ, &si, SIF_ALL);
        SetScrollPos(SB_HORZ, std::min(si.nMax, si.nPos + SCROLL_STEP));
        ScrollToPosition(CPoint(std::min(si.nMax, si.nPos + SCROLL_STEP), GetScrollPosition().y));
        break;
    case VK_HOME:
        ScrollToPosition(CPoint(0, GetScrollPosition().y));
        break;
    case VK_END:
        GetScrollInfo(SB_HORZ, &si, SIF_ALL);
        ScrollToPosition(CPoint(si.nMax, GetScrollPosition().y));
        break;
    default:
        CScrollView::OnKeyDown(nChar, nRepCnt, nFlags);
        break;
    }
}

// ---------------------------------------------------------------------------
// Note selection — hit test
// ---------------------------------------------------------------------------
NoteRef CScoreView::HitTestNote(CPoint ptDoc) const
{
    if (!m_hasDoc) return {};

    for (int t = 0; t < (int)m_doc.tracks.size(); ++t)
    {
        int staffTop = StaffTopY(t);
        const auto& track = m_doc.tracks[t];
        for (int n = 0; n < (int)track.notes.size(); ++n)
        {
            const auto& note = track.notes[n];
            int x1 = TickToPixelX(note.startTick);
            int x2 = TickToPixelX(note.endTick);
            if (x2 <= x1) x2 = x1 + 2;
            int y = PitchToPixelY(note.pitch, staffTop);

            if (ptDoc.x >= x1 && ptDoc.x <= x2 &&
                ptDoc.y >= y - HIT_RADIUS && ptDoc.y <= y + HIT_RADIUS)
                return { t, n };
        }
    }
    return {};
}

void CScoreView::OnLButtonDown(UINT nFlags, CPoint point)
{
    SetFocus();

    if (!m_hasDoc)
    {
        CScrollView::OnLButtonDown(nFlags, point);
        return;
    }

    // Convert client coords → document coords (add scroll offset)
    CPoint ptDoc = point + CSize(GetScrollPosition());

    NoteRef hit = HitTestNote(ptDoc);
    if (hit != m_selectedNote)
    {
        m_selectedNote = hit;
        Invalidate();
        // Notify MainFrame
        GetParent()->PostMessage(WM_SCORE_NOTE_SELECTED,
                                 (WPARAM)hit.trackIdx, (LPARAM)hit.noteIdx);
    }

    CScrollView::OnLButtonDown(nFlags, point);
}

// ---------------------------------------------------------------------------
// Invalidate just the client area covered by one note (avoids full redraw)
// ---------------------------------------------------------------------------
void CScoreView::InvalidateNote(NoteRef ref)
{
    if (!ref.IsValid() || !m_hasDoc) return;
    if (ref.trackIdx >= (int)m_doc.tracks.size()) return;
    const auto& track = m_doc.tracks[ref.trackIdx];
    if (ref.noteIdx >= (int)track.notes.size()) return;

    const auto& note = track.notes[ref.noteIdx];
    int staffTop = StaffTopY(ref.trackIdx);
    int x1 = TickToPixelX(note.startTick);
    int x2 = TickToPixelX(note.endTick);
    if (x2 <= x1 + 3) x2 = x1 + 3;
    int y = PitchToPixelY(note.pitch, staffTop);

    // doc coords → client coords (subtract scroll offset), then pad generously
    CPoint scroll = GetScrollPosition();
    CRect rc(x1, y, x2, y);
    rc.OffsetRect(-scroll.x, -scroll.y);
    rc.InflateRect(3, 8);
    InvalidateRect(&rc, FALSE);
}

// ---------------------------------------------------------------------------
// Hover tracking: highlight the note under the cursor and notify MainFrame so
// the status bar can show its details.
// ---------------------------------------------------------------------------
// Invalidate the horizontal band strip (full visible width) so the tick ruler,
// the cursor line and the hover highlight all repaint cleanly.
static void InvalidateBandStrip(CScrollView* pView, int topYDoc, int botYDoc)
{
    CRect rc;  pView->GetClientRect(&rc);
    CPoint scroll = pView->GetScrollPosition();
    CRect strip(rc.left, topYDoc - 74 - scroll.y, rc.right, botYDoc + 14 - scroll.y);
    pView->InvalidateRect(&strip, FALSE);
}

void CScoreView::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_hasDoc)
    {
        // Arm WM_MOUSELEAVE so we can clear the hover when the cursor exits
        if (!m_trackingLeave)
        {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, m_hWnd, 0 };
            TrackMouseEvent(&tme);
            m_trackingLeave = true;
        }

        CPoint ptDoc = point + CSize(GetScrollPosition());
        m_mouseDocX = ptDoc.x;
        m_mouseDocY = ptDoc.y;
        m_hasMouse  = true;

        NoteRef hit = HitTestNote(ptDoc);
        if (hit != m_hoverNote)
        {
            m_hoverNote = hit;
            ::SetCursor(::LoadCursor(nullptr, hit.IsValid() ? IDC_HAND : IDC_ARROW));
            GetParent()->PostMessage(WM_SCORE_NOTE_HOVER,
                                     (WPARAM)hit.trackIdx, (LPARAM)hit.noteIdx);
        }

        int topY, botY;
        GetTrackYBounds(0, topY, botY);
        InvalidateBandStrip(this, topY, botY);
    }

    CScrollView::OnMouseMove(nFlags, point);
}

LRESULT CScoreView::OnMouseLeave(WPARAM, LPARAM)
{
    m_trackingLeave = false;
    m_hasMouse      = false;
    if (m_hoverNote.IsValid())
    {
        m_hoverNote = NoteRef{};
        GetParent()->PostMessage(WM_SCORE_NOTE_HOVER, (WPARAM)-1, (LPARAM)-1);
    }
    if (m_hasDoc && !m_doc.tracks.empty())
    {
        int topY, botY;
        GetTrackYBounds(0, topY, botY);
        InvalidateBandStrip(this, topY, botY);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Called by MainFrame to sync selection from NoteListPanel → ScoreView
// ---------------------------------------------------------------------------
void CScoreView::SetSelectedNote(NoteRef ref)
{
    if (m_selectedNote == ref) return;
    m_selectedNote = ref;
    Invalidate();
}

// ---------------------------------------------------------------------------
// Scroll the score horizontally so the selected note is visible
// ---------------------------------------------------------------------------
void CScoreView::ScrollToNote(NoteRef ref)
{
    if (!ref.IsValid() || !m_hasDoc) return;
    if (ref.trackIdx >= (int)m_doc.tracks.size()) return;
    const auto& note = m_doc.tracks[ref.trackIdx].notes[ref.noteIdx];

    int x = TickToPixelX(note.startTick);

    CRect clientRect;
    GetClientRect(&clientRect);
    CPoint scrollPos = GetScrollPosition();

    // If note is not in visible horizontal range, scroll to center it
    if (x < scrollPos.x || x > scrollPos.x + clientRect.Width() - 80)
    {
        int newX = std::max(0, x - clientRect.Width() / 3);
        ScrollToPosition(CPoint(newX, scrollPos.y));
    }
}
