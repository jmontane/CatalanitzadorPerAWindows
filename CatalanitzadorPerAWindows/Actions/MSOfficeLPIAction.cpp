﻿/* 
 * Copyright (C) 2011-2012 Jordi Mas i Hernàndez <jmas@softcatala.org>
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
#include <stdio.h>
#include <Shlobj.h>

#include "MSOfficeLPIAction.h"
#include "Runner.h"
#include "Url.h"


RegKeyVersion RegKeys2003 = 
{
	L"11.0", 
	L"SOFTWARE\\Microsoft\\Office\\11.0\\Common\\LanguageResources\\ParentFallback",	
	true
};

RegKeyVersion RegKeys2007 = 
{
	L"12.0",
	L"SOFTWARE\\Microsoft\\Office\\12.0\\Common\\LanguageResources\\InstalledUIs",
	false
};

RegKeyVersion RegKeys2010 = 
{
	L"14.0",
	L"SOFTWARE\\Microsoft\\Office\\14.0\\Common\\LanguageResources\\InstalledUIs",
	false
};

MSOfficeLPIAction::MSOfficeLPIAction(IRegistry* registry, IRunner* runner)
{
	m_registry = registry;	
	m_runner = runner;
	m_MSVersion = MSOfficeUnKnown;
	m_szFullFilename[0] = NULL;
	m_szTempPath2003[0] = NULL;
	m_szFilename[0] = NULL;
	m_executionStep = ExecutionStepNone;
	GetTempPath(MAX_PATH, m_szTempPath);
}

MSOfficeLPIAction::~MSOfficeLPIAction()
{
	if (m_szFullFilename[0] != NULL  && GetFileAttributes(m_szFullFilename) != INVALID_FILE_ATTRIBUTES)
	{
		DeleteFile(m_szFullFilename);
	}

	if (m_connectorFile.size() > 0  && GetFileAttributes(m_connectorFile.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
		DeleteFile(m_connectorFile.c_str());
	}

	_removeOffice2003TempFiles();
}

wchar_t* MSOfficeLPIAction::GetName()
{
	return _getStringFromResourceIDName(IDS_MSOFFICEACTION_NAME, szName);
}

wchar_t* MSOfficeLPIAction::GetDescription()
{
	return _getStringFromResourceIDName(IDS_MSOFFICEACTION_DESCRIPTION, szDescription);
}

LPCWSTR MSOfficeLPIAction::GetLicenseID()
{
	switch (_getVersionInstalled())
	{
		case MSOffice2003:
			return MAKEINTRESOURCE(IDR_LICENSE_OFFICE2003);
		case MSOffice2007:
			return MAKEINTRESOURCE(IDR_LICENSE_OFFICE2007);
		case MSOffice2010:
			return MAKEINTRESOURCE(IDR_LICENSE_OFFICE2010);
		default:
			break;		
	}		
	return NULL;
}

const wchar_t* MSOfficeLPIAction::GetVersion()
{
	switch (_getVersionInstalled())
	{
		case MSOffice2003:
			return L"2003";
		case MSOffice2007:
			return L"2007";
		case MSOffice2010:
			return L"2010";
		case MSOffice2010_64:
			return L"2010_64bits";
		default:			
			return L"";
	}
}

// This deletes the contents of the extracted CAB file for MS Office 2003
void MSOfficeLPIAction::_removeOffice2003TempFiles()
{
	if (_getVersionInstalled() != MSOffice2003 || m_szTempPath2003[0] == NULL)
		return;
	
	wchar_t szFile[MAX_PATH];
	wchar_t* files[] = {L"DW20.ADM_1027", L"DWINTL20.DLL_0001_1027", L"LIP.MSI",
		L"lip.xml", L"OSE.EXE", L"SETUP.CHM_1027", L"SETUP.EXE", L"SETUP.INI", L"lip1027.cab",
		NULL};

	for(int i = 0; files[i] != NULL; i++)
	{
		wcscpy_s(szFile, m_szTempPath2003);
		wcscat_s(szFile, L"\\");
		wcscat_s(szFile, files[i]);

		if (DeleteFile(szFile) == FALSE)
		{
			g_log.Log(L"MSOfficeLPIAction::_removeOffice2003TempFiles. Cannot delete '%s'", (wchar_t *) szFile);
		}		
	}

	RemoveDirectory(m_szTempPath2003);
}

bool MSOfficeLPIAction::_isVersionInstalled(RegKeyVersion regkeys, bool b64bits)
{
	wchar_t szValue[1024];
	wchar_t szKey[1024];
	bool Installed = false;

	swprintf_s(szKey, L"SOFTWARE\\Microsoft\\Office\\%s\\Common\\InstallRoot", regkeys.VersionNumber);

	if (b64bits ? m_registry->OpenKeyNoWOWRedirect(HKEY_LOCAL_MACHINE, szKey, false) :
		m_registry->OpenKey(HKEY_LOCAL_MACHINE, szKey, false))
	{	
		if (m_registry->GetString(L"Path", szValue, sizeof (szValue)))
		{
			if (wcslen(szValue) > 0)
				Installed = true;
		}
		m_registry->Close();
	}
	return Installed;
}

bool MSOfficeLPIAction::_isLangPackForVersionInstalled(RegKeyVersion regkeys)
{	
	bool Installed = false;

	if (m_registry->OpenKey(HKEY_LOCAL_MACHINE, regkeys.InstalledLangMapKey, false))
	{		
		if (regkeys.InstalledLangMapKeyIsDWord)
		{
			DWORD dwValue;
			if (m_registry->GetDWORD(L"1027", &dwValue))
					Installed = true;
		}		
		else
		{
			wchar_t szValue[1024];
			if (m_registry->GetString(L"1027", szValue, sizeof (szValue)))
			{
				if (wcslen (szValue) > 0)
					Installed = true;
			}
		}
		m_registry->Close();
	}
	return Installed;
}

void MSOfficeLPIAction::_readIsLangPackInstalled()
{
	switch (_getVersionInstalled())
	{
	case MSOffice2010:
		m_bLangPackInstalled = _isLangPackForVersionInstalled(RegKeys2010);
		break;
	case MSOffice2007:
		m_bLangPackInstalled = _isLangPackForVersionInstalled(RegKeys2007);
		break;
	case MSOffice2003:
		m_bLangPackInstalled = _isLangPackForVersionInstalled(RegKeys2003);
		break;
	default:
		assert(false);
		break;
	}

	g_log.Log(L"MSOfficeLPIAction::_readIsLangPackInstalled returns '%s'", m_bLangPackInstalled.ToString());
}

bool MSOfficeLPIAction::_isLangPackInstalled()
{
	if (m_bLangPackInstalled.IsUndefined())
	{
		_readIsLangPackInstalled();
	}
	return m_bLangPackInstalled == true;
}

MSOfficeVersion MSOfficeLPIAction::_getVersionInstalled()
{
	if (m_MSVersion == MSOfficeUnKnown)
	{
		_readVersionInstalled();
	}
	return m_MSVersion;
}

void MSOfficeLPIAction::_readVersionInstalled()
{
	wstring version;

	if (_isVersionInstalled(RegKeys2010, false))
	{
		m_MSVersion = MSOffice2010;
	} else if (_isVersionInstalled(RegKeys2007, false))
	{
		m_MSVersion = MSOffice2007;
	} else if (_isVersionInstalled(RegKeys2003, false))
	{
		m_MSVersion = MSOffice2003;
	}else if (_isVersionInstalled(RegKeys2010, true))
	{
		m_MSVersion = MSOffice2010_64;
	} else
	{
		m_MSVersion = NoMSOffice;
	}

	g_log.Log(L"MSOfficeLPIAction::_readVersionInstalled '%s'", (wchar_t*) GetVersion());
}

DownloadID  MSOfficeLPIAction::_getDownloadID()
{
	switch (_getVersionInstalled())
	{
		case MSOffice2010:
			return DI_MSOFFICEACTION_2010;
		case MSOffice2007:
			return DI_MSOFFICEACTION_2007;
		case MSOffice2003:
			return DI_MSOFFICEACTION_2003;
		default:
			assert(false);
			break;
	}
	return DI_UNKNOWN;
}

bool MSOfficeLPIAction::IsNeed()
{
	bool bNeed;

	switch(GetStatus())
	{		
		case NotInstalled:
		case AlreadyApplied:
		case CannotBeApplied:
			bNeed = false;
			break;
		default:
			bNeed = true;
			break;
	}
	g_log.Log(L"MSOfficeLPIAction::IsNeed returns %u (status %u)", (wchar_t *) bNeed, (wchar_t*) GetStatus());
	return bNeed;
}

#define CONNECTOR_REGKEY L"SOFTWARE\\Microsoft\\Office\\Outlook\\Addins\\MSNCON.Addin.1"

bool MSOfficeLPIAction::_needsInstallConnector()
{
	bool bNeed = false;
	MSOfficeVersion officeVersion;

	officeVersion = _getVersionInstalled();
	if (officeVersion == MSOffice2003 || officeVersion == MSOffice2007 || officeVersion == MSOffice2010)
	{		
		wstring path;
		wchar_t szPath[MAX_PATH];

		// Connector installed
		if (m_registry->OpenKey(HKEY_LOCAL_MACHINE, CONNECTOR_REGKEY, false) == true)
		{
			m_registry->Close();			
			SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES|CSIDL_FLAG_CREATE,  NULL, 0, szPath);
			path=szPath;

			// Is the Catalan version?
			path+=L"\\Common Files\\System\\MSMAPI\\1027\\MSNCON32.DLL";
			bNeed = GetFileAttributes(path.c_str()) == INVALID_FILE_ATTRIBUTES;
		}
	}
	g_log.Log(L"MSOfficeLPIAction::_needsInstallConnector returns %u", (wchar_t *) bNeed);
	return bNeed;	
}

bool MSOfficeLPIAction::Download(ProgressStatus progress, void *data)
{
	bool bFile1, bFile2;

	if (_getDownloadID() != DI_UNKNOWN)
	{
		Url url(m_actionDownload.GetFileName(_getDownloadID()));
		wcscpy_s(m_szFilename, url.GetFileName());
	}

	wcscpy_s(m_szFullFilename, m_szTempPath);
	wcscat_s(m_szFullFilename, m_szFilename);
	bFile1 = _getFile(_getDownloadID(), m_szFullFilename, progress, data);

	if (_needsInstallConnector())
	{
		m_connectorFile = m_szTempPath;
		m_connectorFile += L"OutlookConnector-cat.exe";
		bFile2 = _getFile(DI_MSOFFICEACTION_OUTLOOK_CONNECTOR, m_connectorFile.c_str(), progress, data);		
		return bFile1 == true && bFile2 == true;
	}
	else
	{
		return bFile1;
	}
}

bool MSOfficeLPIAction::_extractCabFile(wchar_t * file, wchar_t * path)
{
	Runner runnerCab;
	wchar_t szParams[MAX_PATH];
	wchar_t szApp[MAX_PATH];
	
	GetSystemDirectory(szApp, MAX_PATH);
	wcscat_s(szApp, L"\\expand.exe ");

	swprintf_s (szParams, L" %s %s -f:*", file, path);
	g_log.Log(L"MSOfficeLPIAction::_extractCabFile '%s' with params '%s'", szApp, szParams);
	runnerCab.Execute(szApp, szParams);
	runnerCab.WaitUntilFinished();
	return true;
}

void MSOfficeLPIAction::Execute()
{
	wchar_t szParams[MAX_PATH] = L"";
	wchar_t szApp[MAX_PATH] = L"";

	switch (_getVersionInstalled())
	{
		case MSOffice2010:
		{
			wcscpy_s(szApp, m_szFullFilename);
			wcscpy_s(szParams, L" /passive /norestart /quiet");
			break;
		}
		case MSOffice2007:
		{
			wcscpy_s(szApp, m_szFullFilename);
			wcscpy_s(szParams, L" /quiet");
			break;
		}
		case MSOffice2003:
		{
			wchar_t szMSI[MAX_PATH];

			// Unique temporary file (needs to create it)
			GetTempFileName(m_szTempPath, L"CAT", 0, m_szTempPath2003);
			DeleteFile(m_szTempPath2003);

			if (CreateDirectory(m_szTempPath2003, NULL) == FALSE)
			{
				g_log.Log(L"MSOfficeLPIAction::Execute. Cannot create temp directory '%s'", m_szTempPath2003);
				break;
			}
		
			_extractCabFile(m_szFullFilename, m_szTempPath2003);

			GetSystemDirectory(szApp, MAX_PATH);
			wcscat_s(szApp, L"\\msiexec.exe ");

			wcscpy_s(szMSI, m_szTempPath2003);
			wcscat_s(szMSI, L"\\");
			wcscat_s(szMSI, L"lip.msi");

			wcscpy_s(szParams, L" /i ");
			wcscat_s(szParams, szMSI);
			wcscat_s(szParams, L" /qn");
			break;
		}
	default:
		break;
	}

	SetStatus(InProgress);
	m_executionStep = ExecutionStep1;
	g_log.Log(L"MSOfficeLPIAction::Execute '%s' with params '%s'", szApp, szParams);
	m_runner->Execute(szApp, szParams);
}

RegKeyVersion MSOfficeLPIAction::_getRegKeys()
{
	switch (_getVersionInstalled())
	{
		case MSOffice2003:
			return RegKeys2003;
		case MSOffice2007:
			return RegKeys2007;
		case MSOffice2010:
		default:
			return RegKeys2010;
	}
}

void MSOfficeLPIAction::_setDefaultLanguage()
{
	BOOL bSetKey = FALSE;
	wchar_t szKeyName [1024];

	swprintf_s(szKeyName, L"Software\\Microsoft\\Office\\%s\\Common\\LanguageResources", _getRegKeys().VersionNumber);
	if (m_registry->OpenKey(HKEY_CURRENT_USER, szKeyName, true))
	{
		bSetKey = m_registry->SetDWORD(L"UILanguage", 1027);

		// This key setting tells Office do not use the same language that the Windows UI to determine the Office Language
		// and use the specified language instead
		if (_getVersionInstalled() == MSOffice2010 || _getVersionInstalled() == MSOffice2007)
		{
			BOOL bSetFollowKey = m_registry->SetString(L"FollowSystemUI", L"Off");
			g_log.Log(L"MSOfficeLPIAction::_setDefaultLanguage, set FollowSystemUI %u", (wchar_t *) bSetFollowKey);	
		}		
		m_registry->Close();
	}
	g_log.Log(L"MSOfficeLPIAction::_setDefaultLanguage, set UILanguage %u", (wchar_t *) bSetKey);	
}

bool MSOfficeLPIAction::_executeInstallConnector()
{
	if (_needsInstallConnector() == false)
		return false;

	wchar_t szParams[MAX_PATH];

	wcscpy_s(szParams, L" /quiet");
	m_runner->Execute((wchar_t *)m_connectorFile.c_str(), szParams);
	g_log.Log(L"MSOfficeLPIAction::_executeInstallConnector. Microsoft Office Connector '%s' with params '%s'", (wchar_t *)m_connectorFile.c_str(), szParams);
	return true;
}

ActionStatus MSOfficeLPIAction::GetStatus()
{
	if (status == InProgress)
	{
		if (m_runner->IsRunning())
			return InProgress;

		switch (m_executionStep)
		{
			case ExecutionStepNone:
				break;
			case ExecutionStep1:
			{
				if (_executeInstallConnector() == true)
				{
					m_executionStep = ExecutionStep2;
					return InProgress;
				}
				break;
			}				
			case ExecutionStep2:
				break;
			default:
				assert(false);
				break;
		}

		if (_isLangPackForVersionInstalled(_getRegKeys()))
		{
			SetStatus(Successful);
			_setDefaultLanguage();
		}
		else
		{
			SetStatus(FinishedWithError);
		}
		g_log.Log(L"MSOfficeLPIAction::GetStatus is '%s'", status == Successful ? L"Successful" : L"FinishedWithError");
	}
	return status;
}
	
void MSOfficeLPIAction::CheckPrerequirements(Action * action) 
{
	if (_getVersionInstalled() == NoMSOffice)
	{
		_setStatusNotInstalled();
		return;
	}

	if (_getVersionInstalled() == MSOffice2010_64)
	{		
		SetStatus(CannotBeApplied);
		_getStringFromResourceIDName(IDS_NOTSUPPORTEDVERSION, szCannotBeApplied);
		g_log.Log(L"MSOfficeLPIAction::CheckPrerequirements. Version not supported");		
		return;
	}

	if (_isLangPackInstalled())
	{
		SetStatus(AlreadyApplied);
		return;
	}
}