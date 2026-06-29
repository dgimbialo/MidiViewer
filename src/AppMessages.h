#pragma once

// Sent by CScoreView to parent CMainFrame when user clicks a note.
// WPARAM = trackIdx, LPARAM = noteIdx  (-1/-1 = deselect)
#define WM_SCORE_NOTE_SELECTED   (WM_USER + 100)

// Sent by CNoteListPanel to parent CMainFrame when user clicks a list item.
// WPARAM = trackIdx, LPARAM = noteIdx
#define WM_LIST_NOTE_SELECTED    (WM_USER + 101)

// Sent by CSizerBar to parent CMainFrame during drag.
// WPARAM = delta in pixels (int), LPARAM = 0
#define WM_SIZER_DRAG            (WM_USER + 102)

// Sent by CSizerBar to parent CMainFrame on right-click.
#define WM_SIZER_RCLICK          (WM_USER + 103)

// Sent by CScoreView to parent CMainFrame when the mouse hovers a note.
// WPARAM = trackIdx, LPARAM = noteIdx  (-1/-1 = not hovering any note)
#define WM_SCORE_NOTE_HOVER      (WM_USER + 104)
