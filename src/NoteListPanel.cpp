#include "pch.h"
#include "NoteListPanel.h"
#include <algorithm>

BEGIN_MESSAGE_MAP(CNoteListPanel, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_NOTIFY(LVN_ITEMCHANGED, 1, &CNoteListPanel::OnListItemChanged)
    ON_NOTIFY(NM_CUSTOMDRAW,   1, &CNoteListPanel::OnCustomDraw)
END_MESSAGE_MAP()

// ---------------------------------------------------------------------------

CNoteListPanel::CNoteListPanel()
{
}

CNoteListPanel::~CNoteListPanel()
{
}

// ---------------------------------------------------------------------------

BOOL CNoteListPanel::Create(CWnd* pParent, UINT nID)
{
    LPCTSTR cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
                                      ::LoadCursor(nullptr, IDC_ARROW),
                                      (HBRUSH)(COLOR_3DFACE + 1));
    CRect rect(0, 0, 340, 400);
    return CWnd::Create(cls, L"NoteList",
                        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                        rect, pParent, nID);
}

// ---------------------------------------------------------------------------

int CNoteListPanel::OnCreate(LPCREATESTRUCT lp)
{
    if (CWnd::OnCreate(lp) == -1) return -1;

    CRect rc;
    GetClientRect(&rc);

    if (!m_list.Create(WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS |
                       LVS_SINGLESEL | LVS_NOSORTHEADER,
                       rc, this, 1))
        return -1;

    // Cleaner, modern look: no vertical gridlines, full-row select, buffered.
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    m_list.SetBkColor(RGB(255, 255, 255));
    m_list.SetTextBkColor(RGB(255, 255, 255));

    SetupColumns();
    return 0;
}

// ---------------------------------------------------------------------------

void CNoteListPanel::SetupColumns()
{
    // Order: №, Start Tick, Start ms, Note, Pitch, Dur Ticks, Dur ms, Vel, Ch
    struct ColDef { const wchar_t* title; int width; int fmt; };
    static const ColDef cols[] = {
        { L"\u2116",      46, LVCFMT_RIGHT  },  // №
        { L"Start Tick",  80, LVCFMT_RIGHT  },
        { L"Start ms",    72, LVCFMT_RIGHT  },
        { L"Note",        46, LVCFMT_CENTER },
        { L"Pitch",       44, LVCFMT_RIGHT  },
        { L"Dur Ticks",   76, LVCFMT_RIGHT  },
        { L"Dur ms",      68, LVCFMT_RIGHT  },
        { L"Vel",         38, LVCFMT_RIGHT  },
        { L"Ch",          34, LVCFMT_RIGHT  },
    };
    for (int i = 0; i < (int)(sizeof(cols) / sizeof(cols[0])); ++i)
        m_list.InsertColumn(i, cols[i].title, cols[i].fmt, cols[i].width);
}

// ---------------------------------------------------------------------------

void CNoteListPanel::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    if (::IsWindow(m_list.m_hWnd))
        m_list.SetWindowPos(nullptr, 0, 0, cx, cy,
                            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
}

// The (double-buffered) list control covers the whole client, so erasing the
// panel background underneath only causes flicker during resize.
BOOL CNoteListPanel::OnEraseBkgnd(CDC* pDC)
{
    if (::IsWindow(m_list.m_hWnd))
        return TRUE;
    return CWnd::OnEraseBkgnd(pDC);
}

// ---------------------------------------------------------------------------

void CNoteListPanel::LoadDocument(const MidiDocument* pDoc)
{
    m_pDoc = pDoc;
    RebuildList();
}

// ---------------------------------------------------------------------------
// Build a flat sorted list of all notes with bar-header separator rows
// ---------------------------------------------------------------------------
void CNoteListPanel::RebuildList()
{
    m_list.SetRedraw(FALSE);
    m_list.DeleteAllItems();
    m_itemMap.clear();

    if (!m_pDoc || m_pDoc->tracks.empty())
    {
        m_list.SetRedraw(TRUE);
        return;
    }

    // Collect all notes with their bar number
    struct Item { int barNum; int trackIdx; int noteIdx; };
    std::vector<Item> items;
    for (int t = 0; t < (int)m_pDoc->tracks.size(); ++t)
        for (int n = 0; n < (int)m_pDoc->tracks[t].notes.size(); ++n)
            items.push_back({ m_pDoc->TickToBar(m_pDoc->tracks[t].notes[n].startTick), t, n });

    // Sort: bar → track → startTick
    std::sort(items.begin(), items.end(), [this](const Item& a, const Item& b) {
        if (a.barNum   != b.barNum)   return a.barNum   < b.barNum;
        if (a.trackIdx != b.trackIdx) return a.trackIdx < b.trackIdx;
        return m_pDoc->tracks[a.trackIdx].notes[a.noteIdx].startTick
             < m_pDoc->tracks[b.trackIdx].notes[b.noteIdx].startTick;
    });

    // Pre-compute bar start ticks for bar-header duration display
    int maxBarNum = items.empty() ? 0 : items.back().barNum;
    std::vector<long> barStartTicks(maxBarNum + 2, 0); // +2: need end of last bar
    {
        long   tick   = 0;
        size_t tsIdx  = 0;
        int    curNum = 4, curDen = 4;
        if (!m_pDoc->timeSigs.empty())
        {
            curNum = m_pDoc->timeSigs[0].numerator;
            curDen = m_pDoc->timeSigs[0].denominator;
            tsIdx  = 1;
        }
        for (int b = 0; b <= maxBarNum + 1; ++b)
        {
            while (tsIdx < m_pDoc->timeSigs.size() &&
                   m_pDoc->timeSigs[tsIdx].tick <= tick)
            {
                curNum = m_pDoc->timeSigs[tsIdx].numerator;
                curDen = m_pDoc->timeSigs[tsIdx].denominator;
                ++tsIdx;
            }
            barStartTicks[b] = tick;
            tick += MidiDocument::BarLengthTicks(curNum, curDen, m_pDoc->tpqn);
        }
    }

    int listIdx = 0;
    int prevBar = -1;
    int noteSeq = 1;   // global sequential note number

    for (const auto& item : items)
    {
        // Bar separator row
        if (item.barNum != prevBar)
        {
            CString barLabel;
            barLabel.Format(L"Bar %d", item.barNum + 1);
            m_list.InsertItem(listIdx, barLabel);
            for (int c = 1; c <= 8; ++c)
                m_list.SetItemText(listIdx, c, L"");

            // Fill duration columns for this bar
            long barStart   = barStartTicks[item.barNum];
            long barEnd     = barStartTicks[item.barNum + 1];
            long durTicks   = barEnd - barStart;
            long durMs      = m_pDoc->TickToMs(barEnd) - m_pDoc->TickToMs(barStart);
            CString sDur;
            sDur.Format(L"%ld", durTicks);
            m_list.SetItemText(listIdx, 5, sDur);
            sDur.Format(L"%ld", durMs);
            m_list.SetItemText(listIdx, 6, sDur);

            m_itemMap.push_back({ -1, -1 });
            ++listIdx;
            prevBar = item.barNum;
        }

        const MidiNote& note      = m_pDoc->tracks[item.trackIdx].notes[item.noteIdx];
        long            startMs   = m_pDoc->TickToMs(note.startTick);
        long            durTicks  = note.endTick - note.startTick;
        long            durMs     = m_pDoc->TickToMs(note.endTick) - startMs;

        CString s;

        // Col 0: №
        s.Format(L"%d", noteSeq++);
        m_list.InsertItem(listIdx, s);

        // Col 1: Start Tick
        s.Format(L"%ld", note.startTick);
        m_list.SetItemText(listIdx, 1, s);

        // Col 2: Start ms
        s.Format(L"%ld", startMs);
        m_list.SetItemText(listIdx, 2, s);

        // Col 3: Note name
        m_list.SetItemText(listIdx, 3,
            CString(MidiDocument::MidiPitchName(note.pitch).c_str()));

        // Col 4: Pitch number
        s.Format(L"%d", note.pitch);
        m_list.SetItemText(listIdx, 4, s);

        // Col 5: Dur Ticks
        s.Format(L"%ld", durTicks);
        m_list.SetItemText(listIdx, 5, s);

        // Col 6: Dur ms
        s.Format(L"%ld", durMs);
        m_list.SetItemText(listIdx, 6, s);

        // Col 7: Vel
        s.Format(L"%d", note.velocity);
        m_list.SetItemText(listIdx, 7, s);

        // Col 8: Ch (1-based)
        s.Format(L"%d", note.channel + 1);
        m_list.SetItemText(listIdx, 8, s);

        m_itemMap.push_back({ item.trackIdx, item.noteIdx });
        ++listIdx;
    }

    m_list.SetRedraw(TRUE);
    m_list.Invalidate();
}

// ---------------------------------------------------------------------------
// Sync selection from score → list
// ---------------------------------------------------------------------------
void CNoteListPanel::SetSelectedNote(NoteRef ref)
{
    if (m_bSyncing) return;
    m_bSyncing = true;

    // Deselect all
    int count = m_list.GetItemCount();
    for (int i = 0; i < count; ++i)
        m_list.SetItemState(i, 0, LVIS_SELECTED | LVIS_FOCUSED);

    if (ref.IsValid())
    {
        // Find matching item
        for (int i = 0; i < (int)m_itemMap.size(); ++i)
        {
            if (m_itemMap[i] == ref)
            {
                m_list.SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED,
                                       LVIS_SELECTED | LVIS_FOCUSED);
                m_list.EnsureVisible(i, FALSE);
                break;
            }
        }
    }

    m_bSyncing = false;

    // Full repaint (erase background) so any previous selection border —
    // including its left edge in the row's left gutter — is cleared cleanly.
    m_list.Invalidate(TRUE);
}

// ---------------------------------------------------------------------------
// List selection changed → notify parent MainFrame
// ---------------------------------------------------------------------------
void CNoteListPanel::OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    if (m_bSyncing) return;

    NMLISTVIEW* pNMLV = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    if (!(pNMLV->uChanged & LVIF_STATE)) return;

    // Repaint the whole control on any selection change so the previous row's
    // red border is fully erased. bErase=TRUE clears the ~1px left gutter that
    // default cell drawing leaves untouched (where the border's left edge sits).
    m_list.Invalidate(TRUE);

    // Only react to selection gain
    if (!(pNMLV->uNewState & LVIS_SELECTED))       return;

    int idx = pNMLV->iItem;
    if (idx < 0 || idx >= (int)m_itemMap.size())   return;

    const NoteRef& ref = m_itemMap[idx];

    // If user clicked a bar-header separator → deselect it
    if (!ref.IsValid())
    {
        m_bSyncing = true;
        m_list.SetItemState(idx, 0, LVIS_SELECTED | LVIS_FOCUSED);
        m_bSyncing = false;
        return;
    }

    // Notify MainFrame (use AfxGetMainWnd so it works whether docked or floating)
    AfxGetMainWnd()->PostMessage(WM_LIST_NOTE_SELECTED,
                                 (WPARAM)ref.trackIdx, (LPARAM)ref.noteIdx);
}

// ---------------------------------------------------------------------------
// Custom draw: color bar-header rows
// ---------------------------------------------------------------------------
void CNoteListPanel::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVCUSTOMDRAW* pCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
    *pResult = CDRF_DODEFAULT;

    switch (pCD->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        *pResult = CDRF_NOTIFYITEMDRAW;
        break;

    case CDDS_ITEMPREPAINT:
    {
        int idx = (int)pCD->nmcd.dwItemSpec;
        if (idx >= 0 && idx < (int)m_itemMap.size())
        {
            if (!m_itemMap[idx].IsValid())
            {
                // ---- Bar header: steel-blue background, white bold text ----
                HFONT hBold = CreateFont(
                    -13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                HFONT hOld = (HFONT)SelectObject(pCD->nmcd.hdc, hBold);

                CRect rc;
                m_list.GetItemRect(idx, &rc, LVIR_BOUNDS);

                // Fill entire row with steel-blue
                SetBkColor(pCD->nmcd.hdc, RGB(60, 100, 160));
                ExtTextOutW(pCD->nmcd.hdc, 0, 0,
                            ETO_OPAQUE | ETO_CLIPPED, &rc, L"", 0, nullptr);

                // Draw text vertically centred (nudged up 2px to sit on centre)
                const int TEXT_RAISE = 2;
                SetTextColor(pCD->nmcd.hdc, RGB(255, 255, 255));
                SetBkMode(pCD->nmcd.hdc, TRANSPARENT);
                CRect rcText = rc;
                rcText.left += 3;   // flush to the start of the row
                rcText.OffsetRect(0, -TEXT_RAISE);
                CString text = m_list.GetItemText(idx, 0);
                DrawTextW(pCD->nmcd.hdc, text, text.GetLength(), &rcText,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                // Draw Dur Ticks (col 5) and Dur ms (col 6) right-aligned in their columns
                for (int col : {5, 6})
                {
                    CRect rcSub;
                    m_list.GetSubItemRect(idx, col, LVIR_LABEL, rcSub);
                    if (rcSub.Width() > 4)
                    {
                        CString colText = m_list.GetItemText(idx, col);
                        CRect rcColText = rcSub;
                        rcColText.right -= 4;
                        rcColText.OffsetRect(0, -TEXT_RAISE);
                        DrawTextW(pCD->nmcd.hdc, colText, colText.GetLength(), &rcColText,
                                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    }
                }

                SelectObject(pCD->nmcd.hdc, hOld);
                DeleteObject(hBold);
                *pResult = CDRF_SKIPDEFAULT;
            }
            else
            {
                // --- Note row: check selection via GetItemState (reliable,
                //     works regardless of window focus state) ---
                bool selected = (m_list.GetItemState(idx, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                if (selected)
                {
                    pCD->clrTextBk = RGB(0, 114, 206);
                    pCD->clrText   = RGB(255, 255, 255);
                    *pResult = CDRF_NOTIFYSUBITEMDRAW | CDRF_NOTIFYPOSTPAINT;
                }
                else
                {
                    // Zebra striping for easier row scanning; per-column colours
                    // are applied in the sub-item stage below.
                    pCD->clrTextBk = (idx & 1) ? RGB(243, 246, 251)
                                               : RGB(255, 255, 255);
                    pCD->clrText   = RGB(70, 90, 140);
                    *pResult = CDRF_NOTIFYSUBITEMDRAW;
                }
            }
        }
        break;
    }

    case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
    {
        int idx = (int)pCD->nmcd.dwItemSpec;
        *pResult = CDRF_DODEFAULT;
        if (idx >= 0 && idx < (int)m_itemMap.size() && m_itemMap[idx].IsValid())
        {
            bool selected = (m_list.GetItemState(idx, LVIS_SELECTED) & LVIS_SELECTED) != 0;
            if (!selected)
            {
                // Visual hierarchy: Note/Pitch prominent, the rest muted.
                int col = pCD->iSubItem;
                COLORREF c;
                if (col == 3 || col == 4)  c = RGB(20, 45, 110);    // Note / Pitch
                else if (col == 0)         c = RGB(150, 160, 178);  // No.
                else                       c = RGB(82, 98, 136);    // values
                pCD->clrText   = c;
                pCD->clrTextBk = (idx & 1) ? RGB(243, 246, 251) : RGB(255, 255, 255);
                *pResult = CDRF_NEWFONT;
            }
        }
        break;
    }

    case CDDS_ITEMPOSTPAINT:
    {
        int idx = (int)pCD->nmcd.dwItemSpec;
        if (idx >= 0 && idx < (int)m_itemMap.size() && m_itemMap[idx].IsValid())
        {
            bool selected = (m_list.GetItemState(idx, LVIS_SELECTED) & LVIS_SELECTED) != 0;
            if (selected)
            {
                CRect rc;
                m_list.GetItemRect(idx, &rc, LVIR_BOUNDS);
                // Inset more on the sides so the border sits inside the painted
                // cell area (not the row's left/right gutter, which default cell
                // repaint leaves untouched and would otherwise keep red edges).
                rc.DeflateRect(3, 1);
                HPEN   hPen      = CreatePen(PS_SOLID, 2, RGB(220, 30, 30));
                HPEN   hOldPen   = (HPEN)SelectObject(pCD->nmcd.hdc, hPen);
                HBRUSH hNullBr   = (HBRUSH)GetStockObject(NULL_BRUSH);
                HBRUSH hOldBr    = (HBRUSH)SelectObject(pCD->nmcd.hdc, hNullBr);
                Rectangle(pCD->nmcd.hdc, rc.left, rc.top, rc.right, rc.bottom);
                SelectObject(pCD->nmcd.hdc, hOldPen);
                SelectObject(pCD->nmcd.hdc, hOldBr);
                DeleteObject(hPen);
            }
        }
        *pResult = CDRF_DODEFAULT;
        break;
    }
    }
}
