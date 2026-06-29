#pragma once
#include "pch.h"
#include "AppMessages.h"

// ---------------------------------------------------------------------------
// CSizerBar — a thin draggable bar used to resize the note list panel.
//
// Posts WM_SIZER_DRAG (wParam = int delta px) and WM_SIZER_RCLICK to parent.
// Drag: positive delta = bar moved right (Vertical) or down (Horizontal).
// ---------------------------------------------------------------------------
class CSizerBar : public CWnd
{
public:
    enum class Orientation { Vertical, Horizontal };

    BOOL Create(CWnd* pParent, UINT nID,
                Orientation orient = Orientation::Vertical);

    void SetOrientation(Orientation orient) { m_orient = orient; }

private:
    Orientation m_orient    = Orientation::Vertical;
    bool        m_dragging  = false;
    CPoint      m_ptCapture;           // screen coords at drag start

    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnPaint();
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);

    DECLARE_MESSAGE_MAP()
};
