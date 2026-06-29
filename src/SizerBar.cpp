#include "pch.h"
#include "SizerBar.h"

BEGIN_MESSAGE_MAP(CSizerBar, CWnd)
    ON_WM_SETCURSOR()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_PAINT()
    ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

// ---------------------------------------------------------------------------

BOOL CSizerBar::Create(CWnd* pParent, UINT nID, Orientation orient)
{
    m_orient = orient;
    LPCTSTR cls = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(nullptr, IDC_ARROW),  // overridden in OnSetCursor
        (HBRUSH)(COLOR_3DFACE + 1));

    return CWnd::Create(cls, nullptr,
                        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                        CRect(0, 0, 6, 400), pParent, nID);
}

// ---------------------------------------------------------------------------

BOOL CSizerBar::OnSetCursor(CWnd* /*pWnd*/, UINT /*nHitTest*/, UINT /*message*/)
{
    LPCTSTR cur = (m_orient == Orientation::Vertical) ? IDC_SIZEWE : IDC_SIZENS;
    ::SetCursor(::LoadCursor(nullptr, cur));
    return TRUE;
}

// ---------------------------------------------------------------------------

void CSizerBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    m_ptCapture = point;
    ClientToScreen(&m_ptCapture);
    m_dragging = true;
    SetCapture();
    CWnd::OnLButtonDown(nFlags, point);
}

void CSizerBar::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_dragging)
    {
        CWnd::OnMouseMove(nFlags, point);
        return;
    }

    CPoint ptScreen = point;
    ClientToScreen(&ptScreen);

    int delta = (m_orient == Orientation::Vertical)
                    ? ptScreen.x - m_ptCapture.x
                    : ptScreen.y - m_ptCapture.y;

    if (delta != 0)
    {
        GetParent()->SendMessage(WM_SIZER_DRAG, (WPARAM)(INT_PTR)delta, 0);
        m_ptCapture = ptScreen;
    }

    CWnd::OnMouseMove(nFlags, point);
}

void CSizerBar::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_dragging)
    {
        m_dragging = false;
        ReleaseCapture();
    }
    CWnd::OnLButtonUp(nFlags, point);
}

// ---------------------------------------------------------------------------

void CSizerBar::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);

    dc.FillSolidRect(&rc, ::GetSysColor(COLOR_3DFACE));

    // Subtle groove in the centre
    CPen penShadow (PS_SOLID, 1, ::GetSysColor(COLOR_3DSHADOW));
    CPen penHilight(PS_SOLID, 1, ::GetSysColor(COLOR_3DHILIGHT));

    if (m_orient == Orientation::Vertical)
    {
        int x = rc.Width() / 2;
        dc.SelectObject(&penShadow);
        dc.MoveTo(x,     rc.top); dc.LineTo(x,     rc.bottom);
        dc.SelectObject(&penHilight);
        dc.MoveTo(x + 1, rc.top); dc.LineTo(x + 1, rc.bottom);
    }
    else
    {
        int y = rc.Height() / 2;
        dc.SelectObject(&penShadow);
        dc.MoveTo(rc.left, y);     dc.LineTo(rc.right, y);
        dc.SelectObject(&penHilight);
        dc.MoveTo(rc.left, y + 1); dc.LineTo(rc.right, y + 1);
    }

    dc.SelectObject(GetStockObject(BLACK_PEN));
}

// ---------------------------------------------------------------------------

void CSizerBar::OnRButtonUp(UINT nFlags, CPoint point)
{
    GetParent()->SendMessage(WM_SIZER_RCLICK, 0, 0);
    CWnd::OnRButtonUp(nFlags, point);
}
