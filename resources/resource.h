//{{NO_DEPENDENCIES}}
#pragma once

// Menu IDs
#define IDR_MAINMENU            101

// Commands
#define IDC_FILE_OPEN           40001
#define IDC_FILE_EXIT           40002
#define ID_VIEW_ZOOM_IN         40010
#define ID_VIEW_ZOOM_OUT        40011
#define ID_VIEW_ZOOM_RESET      40012

// Help menu
#define IDC_HELP_ABOUT          40020

// Track-select dialog
#define IDD_SELECT_TRACK        130
#define IDC_TRACK_LIST          1001

#ifndef IDC_STATIC
#define IDC_STATIC              (-1)
#endif

// Dynamic "Track" menu items: IDC_TRACK_FIRST + track index (supports up to 128)
#define IDC_TRACK_FIRST         41000
#define IDC_TRACK_LAST          41127

// Application icon
#define IDI_MAINICON            201

// Status bar pane
#define ID_STATUSBAR_INFO       0
