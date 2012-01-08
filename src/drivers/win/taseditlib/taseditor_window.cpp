//Implementation file of TASEDITOR_WINDOW class
#include "taseditor_project.h"
#include "../main.h"
#include "../Win32InputBox.h"
#include "../tasedit.h"
#include <htmlhelp.h>

extern TASEDITOR_CONFIG taseditor_config;
extern PLAYBACK playback;
extern GREENZONE greenzone;
extern RECORDER recorder;
extern TASEDITOR_PROJECT project;
extern TASEDITOR_LIST list;
extern TASEDITOR_SELECTION selection;
extern MARKERS current_markers;
extern BOOKMARKS bookmarks;
extern INPUT_HISTORY history;
extern POPUP_DISPLAY popup_display;

extern bool turbo;
extern bool muteTurbo;
extern int marker_note_edit;
extern int search_similar_marker;
extern bool must_call_manual_lua_function;

BOOL CALLBACK WndprocTasEditor(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK FindNoteProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
extern BOOL CALLBACK AboutProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

// Recent Menu
HMENU recent_projects_menu;
char* recent_projects[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
const unsigned int MENU_FIRST_RECENT_PROJECT = 55000;
const unsigned int MAX_NUMBER_OF_RECENT_PROJECTS = sizeof(recent_projects)/sizeof(*recent_projects);

// resources
char windowCaptioBase[] = "TAS Editor";
char taseditor_help_filename[] = "\\taseditor.chm";

TASEDITOR_WINDOW::TASEDITOR_WINDOW()
{
	hwndTasEditor = 0;
	hwndFindNote = 0;
	hTaseditorIcon = 0;
	TASEditor_focus = false;

}

void TASEDITOR_WINDOW::init()
{
	hTaseditorIcon = (HICON)LoadImage(fceu_hInstance, MAKEINTRESOURCE(IDI_ICON3), IMAGE_ICON, 16, 16, LR_DEFAULTSIZE);
	hwndTasEditor = CreateDialog(fceu_hInstance, "TASEDIT", hAppWnd, WndprocTasEditor);
	hmenu = GetMenu(hwndTasEditor);
	hrmenu = LoadMenu(fceu_hInstance,"TASEDITCONTEXTMENUS");

	UpdateCheckedItems();
	SetWindowPos(hwndTasEditor, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE|SWP_NOOWNERZORDER);

	recent_projects_menu = CreateMenu();
	UpdateRecentProjectsMenu();

	reset();
}
void TASEDITOR_WINDOW::exit()
{
	if (hwndFindNote)
	{
		DestroyWindow(hwndFindNote);
		hwndFindNote = 0;
	}
	if (hwndTasEditor)
	{
		DestroyWindow(hwndTasEditor);
		hwndTasEditor = 0;
		TASEditor_focus = false;
	}
	if (hTaseditorIcon)
	{
		DestroyIcon(hTaseditorIcon);
		hTaseditorIcon = 0;
	}
}
void TASEDITOR_WINDOW::reset()
{

}
void TASEDITOR_WINDOW::update()
{

}
// --------------------------------------------------------------------------------
void TASEDITOR_WINDOW::RedrawCaption()
{
	char new_caption[300];
	strcpy(new_caption, windowCaptioBase);
	if (!movie_readonly)
		strcat(new_caption, recorder.GetRecordingCaption());
	// add project name
	std::string projectname = project.GetProjectName();
	if (!projectname.empty())
	{
		strcat(new_caption, " - ");
		strcat(new_caption, projectname.c_str());
	}
	// and * if project has unsaved changes
	if (project.GetProjectChanged())
		strcat(new_caption, "*");
	SetWindowText(hwndTasEditor, new_caption);
}
void TASEDITOR_WINDOW::RedrawWindow()
{
	InvalidateRect(hwndTasEditor, 0, FALSE);
}

void TASEDITOR_WINDOW::RightClick(LPNMITEMACTIVATE info)
{
	int index = info->iItem;
	if(index == -1)
		StrayClickMenu(info);
	else if (selection.CheckFrameSelected(index))
		RightClickMenu(info);
}
void TASEDITOR_WINDOW::StrayClickMenu(LPNMITEMACTIVATE info)
{
	POINT pt = info->ptAction;
	ClientToScreen(list.hwndList, &pt);
	HMENU sub = GetSubMenu(hrmenu, CONTEXTMENU_STRAY);
	TrackPopupMenu(sub, 0, pt.x, pt.y, 0, hwndTasEditor, 0);
}
void TASEDITOR_WINDOW::RightClickMenu(LPNMITEMACTIVATE info)
{
	POINT pt = info->ptAction;
	ClientToScreen(list.hwndList, &pt);

	SelectionFrames* current_selection = selection.MakeStrobe();
	if (current_selection->size() == 0)
	{
		StrayClickMenu(info);
		return;
	}
	HMENU sub = GetSubMenu(hrmenu, CONTEXTMENU_SELECTED);
	// inspect current selection and disable inappropriate menu items
	SelectionFrames::iterator current_selection_begin(current_selection->begin());
	SelectionFrames::iterator current_selection_end(current_selection->end());
	bool set_found = false, unset_found = false;
	for(SelectionFrames::iterator it(current_selection_begin); it != current_selection_end; it++)
	{
		if(current_markers.GetMarker(*it))
			set_found = true;
		else 
			unset_found = true;
	}
	if (set_found)
		EnableMenuItem(sub, ID_SELECTED_REMOVEMARKER, MF_BYCOMMAND | MF_ENABLED);
	else
		EnableMenuItem(sub, ID_SELECTED_REMOVEMARKER, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
	if (unset_found)
		EnableMenuItem(sub, ID_SELECTED_SETMARKER, MF_BYCOMMAND | MF_ENABLED);
	else
		EnableMenuItem(sub, ID_SELECTED_SETMARKER, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

	TrackPopupMenu(sub, 0, pt.x, pt.y, 0, hwndTasEditor, 0);
}

void TASEDITOR_WINDOW::UpdateCheckedItems()
{
	// check option ticks
	CheckDlgButton(hwndTasEditor, CHECK_FOLLOW_CURSOR, taseditor_config.follow_playback?MF_CHECKED : MF_UNCHECKED);
	CheckDlgButton(hwndTasEditor,CHECK_AUTORESTORE_PLAYBACK,taseditor_config.restore_position?BST_CHECKED:BST_UNCHECKED);
	CheckDlgButton(hwndTasEditor, IDC_SUPERIMPOSE, taseditor_config.superimpose);
	CheckDlgButton(hwndTasEditor, IDC_RUN_AUTO, taseditor_config.enable_auto_function);
	CheckDlgButton(hwndTasEditor, CHECK_TURBO_SEEK, taseditor_config.turbo_seek?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_VIEW_SHOW_LAG_FRAMES, taseditor_config.show_lag_frames?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_VIEW_SHOW_MARKERS, taseditor_config.show_markers?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_VIEW_SHOWBRANCHSCREENSHOTS, taseditor_config.show_branch_screenshots?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_VIEW_SHOWBRANCHTOOLTIPS, taseditor_config.show_branch_tooltips?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_VIEW_JUMPWHENMAKINGUNDO, taseditor_config.jump_to_undo?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_VIEW_FOLLOWMARKERNOTECONTEXT, taseditor_config.follow_note_context?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_VIEW_ENABLEHOTCHANGES, taseditor_config.enable_hot_changes?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_CONFIG_BRANCHESRESTOREFULLMOVIE, taseditor_config.branch_full_movie?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_CONFIG_BRANCHESWORKONLYWHENRECORDING, taseditor_config.branch_only_when_rec?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_CONFIG_HUDINBRANCHSCREENSHOTS, taseditor_config.branch_scr_hud?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_CONFIG_BINDMARKERSTOINPUT, taseditor_config.bind_markers?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_CONFIG_EMPTYNEWMARKERNOTES, taseditor_config.empty_marker_notes?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_CONFIG_COMBINECONSECUTIVERECORDINGS, taseditor_config.combine_consecutive_rec?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_CONFIG_USE1PFORRECORDING, taseditor_config.use_1p_rec?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_CONFIG_USEINPUTKEYSFORCOLUMNSET, taseditor_config.columnset_by_keys?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_CONFIG_KEYBOARDCONTROLSINLISTVIEW, taseditor_config.keyboard_for_listview?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_CONFIG_SUPERIMPOSE_AFFECTS_PASTE, taseditor_config.superimpose_affects_paste?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_CONFIG_MUTETURBO, muteTurbo?MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hmenu, ID_CONFIG_SILENTAUTOSAVE, taseditor_config.silent_autosave?MF_CHECKED : MF_UNCHECKED);
	
}



// --------------------------------------------------------------------------------------------
void TASEDITOR_WINDOW::UpdateRecentProjectsMenu()
{
	MENUITEMINFO moo;
	int x;
	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_SUBMENU | MIIM_STATE;
	GetMenuItemInfo(GetSubMenu(hmenu, 0), ID_TASEDIT_FILE_RECENT, FALSE, &moo);
	moo.hSubMenu = recent_projects_menu;
	moo.fState = recent_projects[0] ? MFS_ENABLED : MFS_GRAYED;
	SetMenuItemInfo(GetSubMenu(hmenu, 0), ID_TASEDIT_FILE_RECENT, FALSE, &moo);

	// Remove all recent files submenus
	for(x = 0; x < MAX_NUMBER_OF_RECENT_PROJECTS; x++)
	{
		RemoveMenu(recent_projects_menu, MENU_FIRST_RECENT_PROJECT + x, MF_BYCOMMAND);
	}
	// Recreate the menus
	for(x = MAX_NUMBER_OF_RECENT_PROJECTS - 1; x >= 0; x--)
	{  
		// Skip empty strings
		if(!recent_projects[x]) continue;

		std::string tmp = recent_projects[x];
		// clamp this string to 128 chars
		if(tmp.size() > 128)
			tmp = tmp.substr(0, 128);

		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_TYPE;
		// Insert the menu item
		moo.cch = tmp.size();
		moo.fType = 0;
		moo.wID = MENU_FIRST_RECENT_PROJECT + x;
		moo.dwTypeData = (LPSTR)tmp.c_str();
		InsertMenuItem(recent_projects_menu, 0, 1, &moo);
	}

	// if recent_projects is empty, "Recent" manu should be grayed
	int i;
	for (i = 0; i < MAX_NUMBER_OF_RECENT_PROJECTS; ++i)
		if (recent_projects[i]) break;
	if (i < MAX_NUMBER_OF_RECENT_PROJECTS)
		EnableMenuItem(hmenu, ID_TASEDIT_FILE_RECENT, MF_ENABLED);
	else
		EnableMenuItem(hmenu, ID_TASEDIT_FILE_RECENT, MF_GRAYED);

	DrawMenuBar(hwndTasEditor);
}
void TASEDITOR_WINDOW::UpdateRecentProjectsArray(const char* addString)
{
	// find out if the filename is already in the recent files list
	for(unsigned int x = 0; x < MAX_NUMBER_OF_RECENT_PROJECTS; x++)
	{
		if(recent_projects[x])
		{
			if(!strcmp(recent_projects[x], addString))    // Item is already in list
			{
				// If the filename is in the file list don't add it again, move it up in the list instead
				char* tmp = recent_projects[x];			// save pointer
				for(int y = x; y; y--)
					// Move items down.
					recent_projects[y] = recent_projects[y - 1];
				// Put item on top.
				recent_projects[0] = tmp;
				UpdateRecentProjectsMenu();
				return;
			}
		}
	}
	// The filename wasn't found in the list. That means we need to add it.
	// If there's no space left in the recent files list, get rid of the last item in the list
	if(recent_projects[MAX_NUMBER_OF_RECENT_PROJECTS-1])
		free(recent_projects[MAX_NUMBER_OF_RECENT_PROJECTS-1]);
	// Move other items down
	for(unsigned int x = MAX_NUMBER_OF_RECENT_PROJECTS-1; x; x--)
		recent_projects[x] = recent_projects[x-1];
	// Add new item
	recent_projects[0] = (char*)malloc(strlen(addString) + 1);
	strcpy(recent_projects[0], addString);

	UpdateRecentProjectsMenu();
}
void TASEDITOR_WINDOW::RemoveRecentProject(unsigned int which)
{
	if (which >= MAX_NUMBER_OF_RECENT_PROJECTS) return;
	// Remove the item
	if(recent_projects[which])
		free(recent_projects[which]);
	// If the item is not the last one in the list, shift the remaining ones up
	if (which < MAX_NUMBER_OF_RECENT_PROJECTS-1)
	{
		// Move the remaining items up
		for(unsigned int x = which+1; x < MAX_NUMBER_OF_RECENT_PROJECTS; ++x)
		{
			recent_projects[x-1] = recent_projects[x];	// Shift each remaining item up by 1
		}
	}
	recent_projects[MAX_NUMBER_OF_RECENT_PROJECTS-1] = 0;	// Clear out the last item since it is empty now

	UpdateRecentProjectsMenu();
}
void TASEDITOR_WINDOW::LoadRecentProject(int slot)
{
	char*& fname = recent_projects[slot];
	if(fname && AskSaveProject())
	{
		if (!LoadProject(fname))
		{
			int result = MessageBox(hwndTasEditor, "Remove from list?", "Could Not Open Recent Project", MB_YESNO);
			if (result == IDYES)
				RemoveRecentProject(slot);
		}
	}
}


// ====================================================================================================
BOOL CALLBACK WndprocTasEditor(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	extern TASEDITOR_WINDOW taseditor_window;
	switch(uMsg)
	{
		case WM_PAINT:
			break;
		case WM_INITDIALOG:
		{
			if (taseditor_config.wndx == -32000) taseditor_config.wndx = 0;	//Just in case
			if (taseditor_config.wndy == -32000) taseditor_config.wndy = 0;
			SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)taseditor_window.hTaseditorIcon);
			SetWindowPos(hwndDlg, 0, taseditor_config.wndx, taseditor_config.wndy, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
			break;
		}
		case WM_MOVE:
			{
				if (!IsIconic(hwndDlg))
				{
					RECT wrect;
					GetWindowRect(hwndDlg, &wrect);
					taseditor_config.wndx = wrect.left;
					taseditor_config.wndy = wrect.top;
					WindowBoundsCheckNoResize(taseditor_config.wndx, taseditor_config.wndy, wrect.right);
					// also move popup display if it's open
					popup_display.ParentWindowMoved();
				}
				break;
			}
		case WM_NOTIFY:
			switch(wParam)
			{
			case IDC_LIST1:
				switch(((LPNMHDR)lParam)->code)
				{
				case NM_CUSTOMDRAW:
					SetWindowLong(hwndDlg, DWL_MSGRESULT, list.CustomDraw((NMLVCUSTOMDRAW*)lParam));
					return TRUE;
				case LVN_GETDISPINFO:
					list.GetDispInfo((NMLVDISPINFO*)lParam);
					break;
				case NM_CLICK:
					SingleClick((LPNMITEMACTIVATE)lParam);
					break;
				case NM_DBLCLK:
					DoubleClick((LPNMITEMACTIVATE)lParam);
					break;
				case NM_RCLICK:
					taseditor_window.RightClick((LPNMITEMACTIVATE)lParam);
					break;
				case LVN_ITEMCHANGED:
					selection.ItemChanged((LPNMLISTVIEW) lParam);
					break;
				case LVN_ODSTATECHANGED:
					selection.ItemRangeChanged((LPNMLVODSTATECHANGE) lParam);
					break;
					/*
				case LVN_ENDSCROLL:
					// redraw upper and lower list rows (fix for known WinXP bug)
					int start = ListView_GetTopIndex(hwndList);
					ListView_RedrawItems(hwndList,start,start);
					int end = start + listItems - 1;
					ListView_RedrawItems(hwndList,end,end);
					break;
					*/
				}
				break;
			case IDC_BOOKMARKSLIST:
				switch(((LPNMHDR)lParam)->code)
				{
				case NM_CUSTOMDRAW:
					SetWindowLong(hwndDlg, DWL_MSGRESULT, bookmarks.CustomDraw((NMLVCUSTOMDRAW*)lParam));
					return TRUE;
				case LVN_GETDISPINFO:
					bookmarks.GetDispInfo((NMLVDISPINFO*)lParam);
					break;
				case NM_CLICK:
				case NM_DBLCLK:
					bookmarks.LeftClick((LPNMITEMACTIVATE)lParam);
					break;
				case NM_RCLICK:
					bookmarks.RightClick((LPNMITEMACTIVATE)lParam);
					break;
				}
				break;
			case IDC_HISTORYLIST:
				switch(((LPNMHDR)lParam)->code)
				{
				case NM_CUSTOMDRAW:
					SetWindowLong(hwndDlg, DWL_MSGRESULT, history.CustomDraw((NMLVCUSTOMDRAW*)lParam));
					return TRUE;
				case LVN_GETDISPINFO:
					history.GetDispInfo((NMLVDISPINFO*)lParam);
					break;
				case NM_CLICK:
				case NM_DBLCLK:
				case NM_RCLICK:
					history.Click((LPNMITEMACTIVATE)lParam);
					break;
				}
				break;
			case TASEDIT_PLAYSTOP:
				switch(((LPNMHDR)lParam)->code)
				{
				case NM_CLICK:
				case NM_DBLCLK:
					playback.ToggleEmulationPause();
					break;
				}
				break;
			}
			break;
		case WM_CLOSE:
		case WM_QUIT:
			ExitTasEdit();
			break;
		case WM_ACTIVATE:
			if(LOWORD(wParam))
			{
				taseditor_window.TASEditor_focus = true;
				SetTaseditInput();
			} else
			{
				taseditor_window.TASEditor_focus = false;
				ClearTaseditInput();
			}
			break;
		case WM_CTLCOLORSTATIC:
			if ((HWND)lParam == playback.hwndPlaybackMarker)
			{
				SetTextColor((HDC)wParam, PLAYBACK_MARKER_COLOR);
				SetBkMode((HDC)wParam, TRANSPARENT);
				return (BOOL)(list.bg_brush);
			} else if ((HWND)lParam == selection.hwndSelectionMarker)
			{
				SetTextColor((HDC)wParam, GetSysColor(COLOR_HIGHLIGHT));
				SetBkMode((HDC)wParam, TRANSPARENT);
				return (BOOL)list.bg_brush;
			}

			break;
		case WM_COMMAND:
			{
				unsigned int loword_wparam = LOWORD(wParam);
				// first check clicking Recent submenu item
				if (loword_wparam >= MENU_FIRST_RECENT_PROJECT && loword_wparam < MENU_FIRST_RECENT_PROJECT + MAX_NUMBER_OF_RECENT_PROJECTS)
				{
					taseditor_window.LoadRecentProject(loword_wparam - MENU_FIRST_RECENT_PROJECT);
					break;
				}
				// finally check all other commands
				switch(loword_wparam)
				{
				case IDC_PLAYBACK_MARKER_EDIT:
					{
						switch (HIWORD(wParam))
						{
						case EN_SETFOCUS:
							{
								marker_note_edit = MARKER_NOTE_EDIT_UPPER;
								// enable editing
								SendMessage(playback.hwndPlaybackMarkerEdit, EM_SETREADONLY, false, 0);
								// disable FCEUX keyboard
								ClearTaseditInput();
								if (taseditor_config.follow_note_context)
									list.FollowPlayback();
								break;
							}
						case EN_KILLFOCUS:
							{
								if (marker_note_edit == MARKER_NOTE_EDIT_UPPER)
								{
									UpdateMarkerNote();
									marker_note_edit = MARKER_NOTE_EDIT_NONE;
								}
								// disable editing (make it grayed)
								SendMessage(playback.hwndPlaybackMarkerEdit, EM_SETREADONLY, true, 0);
								// enable FCEUX keyboard
								if (taseditor_window.TASEditor_focus)
									SetTaseditInput();
								break;
							}
						}
						break;
					}
				case IDC_SELECTION_MARKER_EDIT:
					{
						switch (HIWORD(wParam))
						{
						case EN_SETFOCUS:
							{
								marker_note_edit = MARKER_NOTE_EDIT_LOWER;
								// enable editing
								SendMessage(selection.hwndSelectionMarkerEdit, EM_SETREADONLY, false, 0); 
								// disable FCEUX keyboard
								ClearTaseditInput();
								if (taseditor_config.follow_note_context)
									list.FollowSelection();
								break;
							}
						case EN_KILLFOCUS:
							{
								if (marker_note_edit == MARKER_NOTE_EDIT_LOWER)
								{
									UpdateMarkerNote();
									marker_note_edit = MARKER_NOTE_EDIT_NONE;
								}
								// disable editing (make it grayed)
								SendMessage(selection.hwndSelectionMarkerEdit, EM_SETREADONLY, true, 0); 
								// enable FCEUX keyboard
								if (taseditor_window.TASEditor_focus)
									SetTaseditInput();
								break;
							}
						}
						break;
					}
				case ID_FILE_OPENPROJECT:
					OpenProject();
					break;
				case ACCEL_CTRL_S:
				case ID_FILE_SAVEPROJECT:
					SaveProject();
					break;
				case ID_FILE_SAVEPROJECTAS:
					SaveProjectAs();
					break;
				case ID_FILE_SAVECOMPACT:
					SaveCompact();
					break;
				case ID_FILE_IMPORT:
					Import();
					break;
				case ID_FILE_EXPORTFM2:
						Export();
					break;
				case ID_TASEDIT_FILE_CLOSE:
					ExitTasEdit();
					break;
				case ID_EDIT_SELECTALL:
					if (marker_note_edit == MARKER_NOTE_EDIT_UPPER)
						SendMessage(playback.hwndPlaybackMarkerEdit, EM_SETSEL, 0, -1); 
					else if (marker_note_edit == MARKER_NOTE_EDIT_LOWER)
						SendMessage(selection.hwndSelectionMarkerEdit, EM_SETSEL, 0, -1); 
					else
						selection.SelectAll();
					break;
				case ACCEL_CTRL_X:
				case ID_TASEDIT_CUT:
					if (marker_note_edit == MARKER_NOTE_EDIT_UPPER)
						SendMessage(playback.hwndPlaybackMarkerEdit, WM_CUT, 0, 0); 
					else if (marker_note_edit == MARKER_NOTE_EDIT_LOWER)
						SendMessage(selection.hwndSelectionMarkerEdit, WM_CUT, 0, 0); 
					else
						Cut();
					break;
				case ACCEL_CTRL_C:
				case ID_TASEDIT_COPY:
					if (marker_note_edit == MARKER_NOTE_EDIT_UPPER)
						SendMessage(playback.hwndPlaybackMarkerEdit, WM_COPY, 0, 0); 
					else if (marker_note_edit == MARKER_NOTE_EDIT_LOWER)
						SendMessage(selection.hwndSelectionMarkerEdit, WM_COPY, 0, 0); 
					else
						Copy();
					break;
				case ACCEL_CTRL_V:
				case ID_TASEDIT_PASTE:
					if (marker_note_edit == MARKER_NOTE_EDIT_UPPER)
						SendMessage(playback.hwndPlaybackMarkerEdit, WM_PASTE, 0, 0); 
					else if (marker_note_edit == MARKER_NOTE_EDIT_LOWER)
						SendMessage(selection.hwndSelectionMarkerEdit, WM_PASTE, 0, 0); 
					else
						Paste();
					break;
				case ACCEL_SHIFT_V:
					{
						// hack to allow entering Shift-V into edit control even though accelerator steals the input message
						char insert_v[] = "v";
						char insert_V[] = "V";
						if (marker_note_edit == MARKER_NOTE_EDIT_UPPER)
						{
							if (GetKeyState(VK_CAPITAL) & 1)
								SendMessage(playback.hwndPlaybackMarkerEdit, EM_REPLACESEL, true, (LPARAM)insert_v);
							else
								SendMessage(playback.hwndPlaybackMarkerEdit, EM_REPLACESEL, true, (LPARAM)insert_V);
						} else if (marker_note_edit == MARKER_NOTE_EDIT_LOWER)
						{
							if (GetKeyState(VK_CAPITAL) & 1)
								SendMessage(selection.hwndSelectionMarkerEdit, EM_REPLACESEL, true, (LPARAM)insert_v);
							else
								SendMessage(selection.hwndSelectionMarkerEdit, EM_REPLACESEL, true, (LPARAM)insert_V);
						} else
							PasteInsert();
						break;
					}
				case ID_EDIT_PASTEINSERT:
					if (marker_note_edit == MARKER_NOTE_EDIT_UPPER)
						SendMessage(playback.hwndPlaybackMarkerEdit, WM_PASTE, 0, 0); 
					else if (marker_note_edit == MARKER_NOTE_EDIT_LOWER)
						SendMessage(selection.hwndSelectionMarkerEdit, WM_PASTE, 0, 0); 
					else
						PasteInsert();
					break;
				case ACCEL_CTRL_DELETE:
				case ID_TASEDIT_DELETE:
				case ID_CONTEXT_SELECTED_DELETEFRAMES:
					DeleteFrames();
					break;
				case ACCEL_CTRL_T:
				case ID_EDIT_TRUNCATE:
				case ID_CONTEXT_SELECTED_TRUNCATE:
				case ID_CONTEXT_STRAY_TRUNCATE:
					Truncate();
					break;
				case ID_HELP_TASEDITHELP:
					{
						//OpenHelpWindow(tasedithelp);
						std::string helpFileName = BaseDirectory;
						helpFileName.append(taseditor_help_filename);
						HtmlHelp(GetDesktopWindow(), helpFileName.c_str(), HH_DISPLAY_TOPIC, (DWORD)NULL);
						break;
					}
				case ACCEL_INS:
				case ID_EDIT_INSERT:
				case MENU_CONTEXT_STRAY_INSERTFRAMES:
				case ID_CONTEXT_SELECTED_INSERTFRAMES2:
					InsertNumFrames();
					break;
				case ACCEL_SHIFT_INS:
				case ID_EDIT_INSERTFRAMES:
				case ID_CONTEXT_SELECTED_INSERTFRAMES:
					InsertFrames();
					break;
				case ACCEL_DEL:
				case ID_EDIT_CLEAR:
				case ID_CONTEXT_SELECTED_CLEARFRAMES:
					if (marker_note_edit == MARKER_NOTE_EDIT_UPPER)
					{
						DWORD sel_start, sel_end;
						SendMessage(playback.hwndPlaybackMarkerEdit, EM_GETSEL, (WPARAM)&sel_start, (LPARAM)&sel_end);
						if (sel_start == sel_end)
							SendMessage(playback.hwndPlaybackMarkerEdit, EM_SETSEL, sel_start, sel_start + 1);
						SendMessage(playback.hwndPlaybackMarkerEdit, WM_CLEAR, 0, 0); 
					} else if (marker_note_edit == MARKER_NOTE_EDIT_LOWER)
					{
						DWORD sel_start, sel_end;
						SendMessage(selection.hwndSelectionMarkerEdit, EM_GETSEL, (WPARAM)&sel_start, (LPARAM)&sel_end);
						if (sel_start == sel_end)
							SendMessage(selection.hwndSelectionMarkerEdit, EM_SETSEL, sel_start, sel_start + 1);
						SendMessage(selection.hwndSelectionMarkerEdit, WM_CLEAR, 0, 0); 
					} else
						ClearFrames();
					break;
				case TASEDIT_PLAYSTOP:
					playback.ToggleEmulationPause();
					break;
				case CHECK_FOLLOW_CURSOR:
					//switch "Follow playback" flag
					taseditor_config.follow_playback ^= 1;
					taseditor_window.UpdateCheckedItems();
					// if switched off then maybe jump to target frame
					if (!taseditor_config.follow_playback && playback.GetPauseFrame())
						list.FollowPauseframe();
					break;
				case CHECK_TURBO_SEEK:
					//switch "Turbo seek" flag
					taseditor_config.turbo_seek ^= 1;
					taseditor_window.UpdateCheckedItems();
					// if currently seeking, apply this option immediately
					if (playback.pause_frame)
						turbo = taseditor_config.turbo_seek;
					break;
				case ID_VIEW_SHOW_LAG_FRAMES:
					taseditor_config.show_lag_frames ^= 1;
					taseditor_window.UpdateCheckedItems();
					list.RedrawList();
					bookmarks.RedrawBookmarksList();
					break;
				case ID_VIEW_SHOW_MARKERS:
					taseditor_config.show_markers ^= 1;
					taseditor_window.UpdateCheckedItems();
					list.RedrawList();		// no need to redraw Bookmarks, as Markers are only shown in main list
					break;
				case ID_VIEW_SHOWBRANCHSCREENSHOTS:
					//switch "Show Branch Screenshots" flag
					taseditor_config.show_branch_screenshots ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ID_VIEW_SHOWBRANCHTOOLTIPS:
					//switch "Show Branch Screenshots" flag
					taseditor_config.show_branch_tooltips ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ID_VIEW_ENABLEHOTCHANGES:
					taseditor_config.enable_hot_changes ^= 1;
					taseditor_window.UpdateCheckedItems();
					list.RedrawList();		// redraw buttons text
					break;
				case ID_VIEW_JUMPWHENMAKINGUNDO:
					taseditor_config.jump_to_undo ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ID_VIEW_FOLLOWMARKERNOTECONTEXT:
					taseditor_config.follow_note_context ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ACCEL_CTRL_P:
				case CHECK_AUTORESTORE_PLAYBACK:
					//switch "Auto-restore last playback position" flag
					taseditor_config.restore_position ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ID_CONFIG_SETGREENZONECAPACITY:
					{
						int new_capacity = taseditor_config.greenzone_capacity;
						if(CWin32InputBox::GetInteger("Greenzone capacity", "Keep savestates for how many frames?\n(actual limit of savestates can be 5 times more than the number provided)", new_capacity, hwndDlg) == IDOK)
						{
							if (new_capacity < GREENZONE_CAPACITY_MIN)
								new_capacity = GREENZONE_CAPACITY_MIN;
							else if (new_capacity > GREENZONE_CAPACITY_MAX)
								new_capacity = GREENZONE_CAPACITY_MAX;
							if (new_capacity < taseditor_config.greenzone_capacity)
							{
								taseditor_config.greenzone_capacity = new_capacity;
								greenzone.GreenzoneCleaning();
							} else taseditor_config.greenzone_capacity = new_capacity;
						}
						break;
					}
				case ID_CONFIG_SETMAXUNDOLEVELS:
					{
						int new_size = taseditor_config.undo_levels;
						if(CWin32InputBox::GetInteger("Max undo levels", "Keep history of how many changes?", new_size, hwndDlg) == IDOK)
						{
							if (new_size < UNDO_LEVELS_MIN)
								new_size = UNDO_LEVELS_MIN;
							else if (new_size > UNDO_LEVELS_MAX)
								new_size = UNDO_LEVELS_MAX;
							if (new_size != taseditor_config.undo_levels)
							{
								taseditor_config.undo_levels = new_size;
								history.reset();
								selection.reset();
								// hot changes were cleared, so update list
								list.RedrawList();
							}
						}
						break;
					}
				case ID_CONFIG_SETAUTOSAVEPERIOD:
					{
						int new_period = taseditor_config.autosave_period;
						if(CWin32InputBox::GetInteger("Autosave period", "How many minutes may the project stay not saved after being changed?\n(0 = no autosaves)", new_period, hwndDlg) == IDOK)
						{
							if (new_period < AUTOSAVE_PERIOD_MIN)
								new_period = AUTOSAVE_PERIOD_MIN;
							else if (new_period > AUTOSAVE_PERIOD_MAX)
								new_period = AUTOSAVE_PERIOD_MAX;
							taseditor_config.autosave_period = new_period;
							project.SheduleNextAutosave();	
						}
						break;
					}
				case ID_CONFIG_BRANCHESRESTOREFULLMOVIE:
					//switch "Branches restore entire Movie" flag
					taseditor_config.branch_full_movie ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ID_CONFIG_BRANCHESWORKONLYWHENRECORDING:
					//switch "Branches work only when Recording" flag
					taseditor_config.branch_only_when_rec ^= 1;
					taseditor_window.UpdateCheckedItems();
					bookmarks.RedrawBookmarksCaption();
					break;
				case ID_CONFIG_HUDINBRANCHSCREENSHOTS:
					//switch "HUD in Branch screenshots" flag
					taseditor_config.branch_scr_hud ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ID_CONFIG_BINDMARKERSTOINPUT:
					taseditor_config.bind_markers ^= 1;
					taseditor_window.UpdateCheckedItems();
					list.RedrawList();
					break;
				case ID_CONFIG_EMPTYNEWMARKERNOTES:
					taseditor_config.empty_marker_notes ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ID_CONFIG_COMBINECONSECUTIVERECORDINGS:
					//switch "Combine consecutive Recordings" flag
					taseditor_config.combine_consecutive_rec ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ID_CONFIG_USE1PFORRECORDING:
					//switch "Use 1P keys for single Recordings" flag
					taseditor_config.use_1p_rec ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ID_CONFIG_USEINPUTKEYSFORCOLUMNSET:
					taseditor_config.columnset_by_keys ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ID_CONFIG_KEYBOARDCONTROLSINLISTVIEW:
					taseditor_config.keyboard_for_listview ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ID_CONFIG_SUPERIMPOSE_AFFECTS_PASTE:
					taseditor_config.superimpose_affects_paste ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ID_CONFIG_MUTETURBO:
					muteTurbo ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case ID_CONFIG_SILENTAUTOSAVE:
					taseditor_config.silent_autosave ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;
				case IDC_PROGRESS_BUTTON:
					// click on progressbar - stop seeking
					if (playback.GetPauseFrame()) playback.SeekingStop();
					break;
				case IDC_BRANCHES_BUTTON:
					// click on "Bookmarks/Branches" - switch "View Tree of branches"
					taseditor_config.view_branches_tree ^= 1;
					bookmarks.RedrawBookmarksCaption();
					break;
				case IDC_RECORDING:
					// toggle readonly, no need to recheck radiobuttons
					FCEUI_MovieToggleReadOnly();
					CheckDlgButton(taseditor_window.hwndTasEditor, IDC_RECORDING, movie_readonly?BST_UNCHECKED : BST_CHECKED);
					break;
				case IDC_RADIO2:
					recorder.multitrack_recording_joypad = MULTITRACK_RECORDING_ALL;
					break;
				case IDC_RADIO3:
					recorder.multitrack_recording_joypad = MULTITRACK_RECORDING_1P;
					break;
				case IDC_RADIO4:
					recorder.multitrack_recording_joypad = MULTITRACK_RECORDING_2P;
					break;
				case IDC_RADIO5:
					recorder.multitrack_recording_joypad = MULTITRACK_RECORDING_3P;
					break;
				case IDC_RADIO6:
					recorder.multitrack_recording_joypad = MULTITRACK_RECORDING_4P;
					break;
				case IDC_SUPERIMPOSE:
					// 3 states of "Superimpose" checkbox
					if (taseditor_config.superimpose == BST_UNCHECKED)
						taseditor_config.superimpose = BST_CHECKED;
					else if (taseditor_config.superimpose == BST_CHECKED)
						taseditor_config.superimpose = BST_INDETERMINATE;
					else taseditor_config.superimpose = BST_UNCHECKED;
					taseditor_window.UpdateCheckedItems();
					break;
				case ACCEL_CTRL_A:
					if (marker_note_edit == MARKER_NOTE_EDIT_UPPER)
						SendMessage(playback.hwndPlaybackMarkerEdit, EM_SETSEL, 0, -1); 
					else if (marker_note_edit == MARKER_NOTE_EDIT_LOWER)
						SendMessage(selection.hwndSelectionMarkerEdit, EM_SETSEL, 0, -1); 
					else
						selection.SelectMidMarkers();
					break;
				case ID_EDIT_SELECTMIDMARKERS:
				case ID_SELECTED_SELECTMIDMARKERS:
					selection.SelectMidMarkers();
					break;
				case ACCEL_CTRL_INSERT:
				case ID_EDIT_CLONEFRAMES:
				case ID_SELECTED_CLONE:
					CloneFrames();
					break;
				case ACCEL_CTRL_Z:
				case ID_EDIT_UNDO:
					{
						if (marker_note_edit == MARKER_NOTE_EDIT_UPPER)
						{
							SendMessage(playback.hwndPlaybackMarkerEdit, WM_UNDO, 0, 0); 
						} else if (marker_note_edit == MARKER_NOTE_EDIT_LOWER)
						{
							SendMessage(selection.hwndSelectionMarkerEdit, WM_UNDO, 0, 0); 
						} else
						{
							int result = history.undo();
							if (result >= 0)
							{
								list.update();
								list.FollowUndo();
								greenzone.InvalidateAndCheck(result);
							}
						}
						break;
					}
				case ACCEL_CTRL_Y:
				case ID_EDIT_REDO:
					{
						int result = history.redo();
						if (result >= 0)
						{
							list.update();
							list.FollowUndo();
							greenzone.InvalidateAndCheck(result);
						}
						break;
					}
				case ID_EDIT_SELECTIONUNDO:
				case ACCEL_CTRL_Q:
					{
						selection.undo();
						list.FollowSelection();
						break;
					}
				case ID_EDIT_SELECTIONREDO:
				case ACCEL_CTRL_W:
					{
						selection.redo();
						list.FollowSelection();
						break;
					}
				case ID_EDIT_RESELECTCLIPBOARD:
				case ACCEL_CTRL_B:
					{
						selection.ReselectClipboard();
						list.FollowSelection();
						break;
					}
				case IDC_JUMP_PLAYBACK_BUTTON:
					{
						list.FollowPlayback();
						break;
					}
				case IDC_JUMP_SELECTION_BUTTON:
					{
						list.FollowSelection();
						break;
					}
				case ID_SELECTED_SETMARKER:
					{
						SelectionFrames* current_selection = selection.MakeStrobe();
						if (current_selection->size())
						{
							SelectionFrames::iterator current_selection_begin(current_selection->begin());
							SelectionFrames::iterator current_selection_end(current_selection->end());
							bool changes_made = false;
							for(SelectionFrames::iterator it(current_selection_begin); it != current_selection_end; it++)
							{
								if(!current_markers.GetMarker(*it))
								{
									if (current_markers.SetMarker(*it))
									{
										changes_made = true;
										list.RedrawRow(*it);
									}
								}
							}
							if (changes_made)
							{
								selection.must_find_current_marker = playback.must_find_current_marker = true;
								history.RegisterMarkersChange(MODTYPE_MARKER_SET, *current_selection_begin, *current_selection->rbegin());
							}
						}
						break;
					}
				case ID_SELECTED_REMOVEMARKER:
					{
						SelectionFrames* current_selection = selection.MakeStrobe();
						if (current_selection->size())
						{
							SelectionFrames::iterator current_selection_begin(current_selection->begin());
							SelectionFrames::iterator current_selection_end(current_selection->end());
							bool changes_made = false;
							for(SelectionFrames::iterator it(current_selection_begin); it != current_selection_end; it++)
							{
								if(current_markers.GetMarker(*it))
								{
									current_markers.ClearMarker(*it);
									changes_made = true;
									list.RedrawRow(*it);
								}
							}
							if (changes_made)
							{
								selection.must_find_current_marker = playback.must_find_current_marker = true;
								history.RegisterMarkersChange(MODTYPE_MARKER_UNSET, *current_selection_begin, *current_selection->rbegin());
							}
						}
						break;
					}
				case ACCEL_SHIFT_PGUP:
					if (!playback.jump_was_used_this_frame)
						playback.RewindFull();
					break;
				case ACCEL_SHIFT_PGDN:
					if (!playback.jump_was_used_this_frame)
						playback.ForwardFull();
					break;
				case ACCEL_CTRL_PGUP:
					selection.JumpPrevMarker();
					break;
				case ACCEL_CTRL_PGDN:
					selection.JumpNextMarker();
					break;
				case ACCEL_CTRL_F:
				case ID_VIEW_FINDNOTE:
					{
						if (taseditor_window.hwndFindNote)
							SetFocus(GetDlgItem(taseditor_window.hwndFindNote, IDC_NOTE_TO_FIND));
						else
							taseditor_window.hwndFindNote = CreateDialog(fceu_hInstance, MAKEINTRESOURCE(IDD_TASEDIT_FINDNOTE), taseditor_window.hwndTasEditor, FindNoteProc);
						break;
					}
				case TASEDIT_FIND_BEST_SIMILAR_MARKER:
					search_similar_marker = 0;
					current_markers.FindSimilar(search_similar_marker);
					search_similar_marker++;
					break;
				case TASEDIT_FIND_NEXT_SIMILAR_MARKER:
					current_markers.FindSimilar(search_similar_marker);
					search_similar_marker++;
					break;
				case ID_HELP_ABOUT:
					DialogBox(fceu_hInstance, MAKEINTRESOURCE(IDD_TASEDIT_ABOUT), taseditor_window.hwndTasEditor, AboutProc);
					break;
				case TASEDIT_RUN_MANUAL:
					// the function will be called in next window update
					must_call_manual_lua_function = true;
					break;
				case IDC_RUN_AUTO:
					taseditor_config.enable_auto_function ^= 1;
					taseditor_window.UpdateCheckedItems();
					break;


				}
				break;
			}
		case WM_SYSKEYDOWN:
		{
			if (wParam == VK_F10)
				return 0;
			break;
		}
		default:
			break;
	}
	return FALSE;
}


















