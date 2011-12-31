/* 
 * Copyright (C) 2011 Jordi Mas i Hern�ndez <jmas@softcatala.org>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  
 * 02111-1307, USA.
 */

#include "stdafx.h"
#include "ApplicationsPropertyPageUI.h"
#include "Actions.h"
#include "Action.h"

#include <vector>
using namespace std;

ApplicationsPropertyPageUI::ApplicationsPropertyPageUI()
{
	m_hFont = NULL;
	m_hImageList = NULL;
}

ApplicationsPropertyPageUI::~ApplicationsPropertyPageUI()
{
	if (m_hFont)
	{
		DeleteObject(m_hFont);
		m_hFont = NULL;
	}	
}

void ApplicationsPropertyPageUI::_processDependantItem(Action* action)
{
	Action* dependant = action->AnotherActionDependsOnMe(m_availableActions);

	if (dependant == NULL)
		return;
	
	ActionStatus prevStatus = dependant->GetStatus();
	dependant->CheckPrerequirements(action);

	if (prevStatus == dependant->GetStatus())
		return;
	
	int items = ListView_GetItemCount(m_hList);

	for (int i = 0; i < items; ++i)
	{		
		LVITEM item;
		memset(&item,0,sizeof(item));
		item.iItem = i;
		item.mask = LVIF_PARAM;

		ListView_GetItem(m_hList, &item);
		Action* itemAction = (Action *) item.lParam;		
		if (itemAction == dependant)
		{
			item.mask = LVIF_IMAGE;
			item.iImage = CheckedListView::GetImageIndex(itemAction->GetStatus());
			ListView_SetItem(m_hList, &item);
			break;
		}		
	}	
}

void ApplicationsPropertyPageUI::_processClickOnItem(int nItem)
{	
	LVITEM item;
	memset(&item,0,sizeof(item));
	item.iItem = nItem;
	item.mask = LVIF_IMAGE | LVIF_PARAM;
	ListView_GetItem(m_hList, &item);

	Action* action = (Action *) item.lParam;

	switch (action->GetStatus()) {
	case NotSelected:
		action->SetStatus(Selected);
		break;
	case Selected:
		action->SetStatus(NotSelected);
		break;
	default:
		return; // Non selectable item
	}

	_processDependantItem(action);

	item.mask = LVIF_IMAGE;
	item.iImage = CheckedListView::GetImageIndex(action->GetStatus());
	ListView_SetItem(m_hList, &item);
}

LRESULT ApplicationsPropertyPageUI::_listViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{	
	ApplicationsPropertyPageUI *pThis = (ApplicationsPropertyPageUI *)GetWindowLong(hWnd,GWL_USERDATA);

	switch (uMsg)
	{
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONDOWN:
		{
			LVHITTESTINFO lvHitTestInfo;
			lvHitTestInfo.pt.x = LOWORD(lParam);
			lvHitTestInfo.pt.y = HIWORD(lParam);
			int nItem = ListView_HitTest(hWnd, &lvHitTestInfo);

			if (lvHitTestInfo.flags & LVHT_ONITEMICON)
			{
				pThis->_processClickOnItem(lvHitTestInfo.iItem);
			}
		}
		
		case WM_KEYDOWN:
		{
			if (wParam == VK_SPACE)
			{
				pThis->_processClickOnItem(ListView_GetSelectionMark(hWnd));
			}
		}

		default:
			return CallWindowProc(pThis->PreviousProc, hWnd, uMsg, wParam, lParam);
	}
}

void ApplicationsPropertyPageUI::_setBoldControls()
{
	int boldControls [] = {IDC_APPLICATION_DESCRIPTION_CAPTION, IDC_APPLICATION_CANNOTBEAPPLIED_CAPTION,
		IDC_APPLICATION_LEGEND_CAPTION};

	m_hFont = Window::CreateBoldFont(getHandle());

	for (int b = 0; b < sizeof(boldControls) / sizeof(boldControls[0]); b++)
	{
		SendMessage(GetDlgItem (getHandle(), boldControls[b]),
			WM_SETFONT, (WPARAM) m_hFont, TRUE);
	}
}

void ApplicationsPropertyPageUI::_setLegendControl()
{
	ActionStatus statuses [] = {Selected, AlreadyApplied, CannotBeApplied};
	int resources [] = {IDS_LEGEND_SELECTED, IDS_LEGEND_ALREADYAPPLIED, IDS_LEGEND_CANNOT};
	HWND hList;

	hList = GetDlgItem(getHandle(), IDC_APPLICATIONSLEGEND);
	LVITEM item;
	memset(&item,0,sizeof(item));
	item.mask = LVIF_TEXT | LVIF_IMAGE;

	for (int l = 0; l <sizeof(statuses) / sizeof(statuses[0]); l++)
	{
		wchar_t szString [MAX_LOADSTRING];

		HINSTANCE hInstance = GetModuleHandle(NULL);
		LoadString(hInstance, resources[l], szString, MAX_LOADSTRING);

		item.iItem= l;
		item.pszText= szString;
		item.iImage = CheckedListView::GetImageIndex(statuses[l]);
		ListView_InsertItem(hList, &item);		
	}
	
	ListView_SetImageList(hList, m_hImageList, LVSIL_SMALL);	
}
 
void ApplicationsPropertyPageUI::_onInitDialog()
{
	int nItemId = 0;
	m_hList = GetDlgItem(getHandle(), IDC_APPLICATIONSLIST);	

	LVITEM item;
	memset(&item,0,sizeof(item));
	item.mask=LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;

	if (m_availableActions->size() == 0)
		return;

	_setBoldControls();

	m_hImageList = m_listview.CreateCheckBoxImageList(m_hList);
	ListView_SetImageList(m_hList, m_hImageList, LVSIL_SMALL);
	
	// Enabled items
	for (unsigned int i = 0; i < m_availableActions->size (); i++)
	{		
		Action* action = m_availableActions->at(i);
		bool needed = action->IsNeed();

		m_disabledActions.insert(ActionBool_Pair(action, needed));

		if (needed == false)
			continue;

		action->SetStatus(Selected);
		item.iItem = nItemId;
		item.pszText = action->GetName();
		item.lParam = (LPARAM) action;		
		item.iImage = CheckedListView::GetImageIndex(action->GetStatus());
		ListView_InsertItem (m_hList, &item);		
		nItemId++;
	}
	
	// Disabled items
	for (unsigned int i = 0; i < m_availableActions->size (); i++)
	{		
		Action* action = m_availableActions->at(i);
		map <Action *, bool>::iterator disabled_item;

		disabled_item = m_disabledActions.find((Action * const &)action);

		if (disabled_item->second == true)
			continue;
		
		item.iItem=nItemId;
		item.pszText= action->GetName();
		item.lParam = (LPARAM) action;		
		item.iImage = CheckedListView::GetImageIndex(action->GetStatus());
		ListView_InsertItem(m_hList, &item);
		nItemId++;
	}
		
	ListView_SetItemState(m_hList, 0, LVIS_FOCUSED | LVIS_SELECTED, 0x000F);
	SetWindowLongPtr(m_hList, GWL_USERDATA, (LONG) this);
	PreviousProc = (WNDPROC)SetWindowLongPtr(m_hList, GWLP_WNDPROC, (LONG_PTR) _listViewSubclassProc);
		
	_setLegendControl();
}

NotificationResult ApplicationsPropertyPageUI::_onNotify(LPNMHDR hdr, int iCtrlID)
{
	if(hdr->idFrom != IDC_APPLICATIONSLIST)
		return ReturnFalse;

	if (hdr->code == NM_CUSTOMDRAW)
	{
		LPNMLVCUSTOMDRAW lpNMLVCD = (LPNMLVCUSTOMDRAW)hdr;

		if (lpNMLVCD->nmcd.dwDrawStage == CDDS_PREPAINT)
		{
			SetWindowLong(getHandle(), DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
			return ReturnTrue;
		}

		if (lpNMLVCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
		{
			Action* action = (Action *)  lpNMLVCD->nmcd.lItemlParam;
			map <Action *, bool>::iterator item;

			item = m_disabledActions.find((Action * const &)action);

			if (item->second == false)
			{
				DWORD color = GetSysColor(COLOR_GRAYTEXT);
				lpNMLVCD->clrText = color;
			}
			SetWindowLong(getHandle(), DWLP_MSGRESULT, CDRF_DODEFAULT);
			return ReturnTrue;
		}
		
		return CallDefProc;
	}

	NMLISTVIEW *pListView = (NMLISTVIEW *)hdr;
	    
	if(pListView->hdr.code != LVN_ITEMCHANGED)
		return ReturnFalse;

	_updateActionDescriptionAndReq((Action *)  pListView->lParam);
	return ReturnTrue;
}

void ApplicationsPropertyPageUI::_updateActionDescriptionAndReq(Action* action)
{
	int show;

	SendDlgItemMessage (getHandle(), IDC_APPLICATION_DESCRIPTION,
		WM_SETTEXT, (WPARAM) 0, 
		(LPARAM) action->GetDescription());

	SendDlgItemMessage (getHandle(), IDC_APPLICATION_MISSINGREQ,
		WM_SETTEXT, (WPARAM) 0, 
		(LPARAM) action->GetCannotNotBeApplied());

	show = *action->GetCannotNotBeApplied() != NULL ? SW_SHOWNORMAL: SW_HIDE;
	ShowWindow(GetDlgItem(getHandle(), IDC_APPLICATION_CANNOTBEAPPLIED_CAPTION), show);
	ShowWindow(GetDlgItem(getHandle(), IDC_APPLICATION_MISSINGREQ), show);	
}

bool ApplicationsPropertyPageUI::_onNext()
{
	int items = ListView_GetItemCount(m_hList);
	bool needInet = false;

	for (int i = 0; i < items; ++i)
	{
		bool bSelected;
		LVITEM item;
		memset(&item,0,sizeof(item));
		item.iItem = i;
		item.mask = LVIF_PARAM;

		ListView_GetItem(m_hList, &item);
		Action* action = (Action *) item.lParam;		
		bSelected = action->GetStatus() == Selected;
		g_log.Log(L"ApplicationsPropertyPageUI::_onNext. Action '%s', selected %u", action->GetName(), (wchar_t *)bSelected);

		if (bSelected && action->IsDownloadNeed())
			needInet = true;
	}

	if (needInet && InternetAccess::IsThereConnection() == false)
	{
		_noInternetConnection();
		return FALSE;
	}
	return TRUE;
}

void ApplicationsPropertyPageUI::_noInternetConnection()
{
	wchar_t szMessage [MAX_LOADSTRING];
	wchar_t szCaption [MAX_LOADSTRING];

	LoadString(GetModuleHandle(NULL), IDS_NOINETACCESS, szMessage, MAX_LOADSTRING);
	LoadString(GetModuleHandle(NULL), IDS_MSGBOX_CAPTION, szCaption, MAX_LOADSTRING);

	MessageBox(getHandle(), szMessage, szCaption, MB_ICONWARNING | MB_OK);	
}