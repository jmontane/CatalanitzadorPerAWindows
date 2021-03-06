﻿/* 
 * Copyright (C) 2011 Jordi Mas i Hernàndez <jmas@softcatala.org>
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
#include "ConfigureLocaleAction.h"
#include "Registry.h"
#include "OSVersion.h"
#include "Resources.h"

ConfigureLocaleAction::ConfigureLocaleAction()
{
	szCfgFile[0] = NULL;
}

ConfigureLocaleAction::~ConfigureLocaleAction()
{
	if (szCfgFile[0] != NULL && GetFileAttributes(szCfgFile) != INVALID_FILE_ATTRIBUTES)
	{
		DeleteFile(szCfgFile);
	}	
}

wchar_t* ConfigureLocaleAction::GetName()
{
	return _getStringFromResourceIDName(IDS_CONFIGURELOCALEACTION_NAME, szName);	
}

wchar_t* ConfigureLocaleAction::GetDescription()
{
	return _getStringFromResourceIDName(IDS_CONFIGURELOCALEACTION_DESCRIPTION, szDescription);
}

bool ConfigureLocaleAction::_isCatalanLocaleActive()
{
	wchar_t szValue[1024];
	bool bCatalanActive = false;

	Registry registry;
	if (registry.OpenKey(HKEY_CURRENT_USER, L"Control Panel\\International", false))
	{
		if (registry.GetString(L"Locale", szValue, sizeof (szValue)))
		{
			// 0403 locale code for CA-ES
			if (wcsstr(szValue, L"0403") != NULL)
				bCatalanActive = true;
		}
		registry.Close();
	}
	return bCatalanActive;
}

bool ConfigureLocaleAction::IsNeed()
{	
	bool bNeed;

	bNeed = _isCatalanLocaleActive() == false;

	if (bNeed == false)
		status = AlreadyApplied;

	g_log.Log(L"ConfigureLocaleAction::IsNeed returns %u", (wchar_t *) bNeed);
	return bNeed;
}

void ConfigureLocaleAction::Execute()
{	
	wchar_t szConfigFileName[MAX_PATH];
	wchar_t szParams[MAX_PATH];
	wchar_t szApp[MAX_PATH];
	LPCWSTR resource;
	OSVersion version;

	GetTempPath(MAX_PATH, szCfgFile);

	GetSystemDirectory(szApp, MAX_PATH);
	wcscat_s(szApp, L"\\control.exe ");
	status = InProgress;

	if (version.GetVersion() == WindowsXP)
	{
		//Documentation: http://support.microsoft.com/default.aspx?scid=kb;en-us;289125
		wcscpy_s(szConfigFileName, L"regopts.txt");
		resource = (LPCWSTR)IDR_CONFIG_LOCALE_WINXP;
	}
	else // Windows Vista and 7
	{
		//Documentation: http://blogs.msdn.com/b/michkap/archive/2006/05/30/610505.aspx
		wcscpy_s(szConfigFileName, L"regopts.xml");
		resource = (LPCWSTR)IDR_CONFIG_LOCALE_WINVISTA;
	}
	wcscat_s(szCfgFile, szConfigFileName);

	Resources::DumpResource(L"CONFIG_FILES", resource, szCfgFile);
	swprintf_s(szParams, L" intl.cpl,,/f:\"%s\"", szCfgFile);

	g_log.Log(L"ConfigureLocaleAction::Execute '%s' with params '%s'", szApp, szParams);
	status = InProgress;
	m_runner.Execute(szApp, szParams);
}

ActionStatus ConfigureLocaleAction::GetStatus()
{
	if (status == InProgress)
	{
		if (m_runner.IsRunning())
			return InProgress;

		if (_isCatalanLocaleActive())
		{
			status = Successful;	
		}
		else
		{
			status = FinishedWithError;
		}
		
		g_log.Log(L"ConfigureLocaleAction::GetStatus is '%s'", status == Successful ? L"Successful" : L"FinishedWithError");
	}
	return status;
}
