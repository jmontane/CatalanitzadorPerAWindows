/* 
 * Copyright (C) 2012 Jordi Mas i Hern�ndez <jmas@softcatala.org>
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

#include "FirefoxAction.h"
#include "OSVersion.h"

#include <fstream>
#include <cstdio>


FirefoxAction::FirefoxAction(IRegistry* registry)
{
	m_registry = registry;
	szVersionAscii[0] = NULL;
}

wchar_t* FirefoxAction::GetName()
{
	return _getStringFromResourceIDName(IDS_FIREFOXACTION_NAME, szName);
}

wchar_t* FirefoxAction::GetDescription()
{	
	return _getStringFromResourceIDName(IDS_FIREFOXACTION_DESCRIPTION, szDescription);
}

DWORD FirefoxAction::GetProcessIDForRunningApp()
{
	Runner runner;

	return runner.GetProcessID(wstring(L"firefox.exe"));
}

bool FirefoxAction::IsNeed()
{
	bool bNeed = false;
	wstring langcode, firstlang;

	if (*szVersionAscii == 0x0)
	{
		_readVersionAndLocale();
	}

	if (_readLanguageCode(langcode))
	{
		if (langcode.length() == 0)
		{
			if (m_locale == L"ca")
			{
				bNeed = false;
			}
			else
			{
				// The Firefox locale determines which languages are going to be used in the 
				// accept_languages. Since by setting to Catalan we reset the default value
				// add at the end of the locale of the langpack as secondary language, then
				// if you install Firefox in English you will have ca, es as language codes
				if (m_locale.size() > 0)
				{
					m_languages.push_back(m_locale);
				}
				bNeed = true;
			}
		}
		else
		{
			ParseLanguage(langcode);
			_getFirstLanguage(firstlang);

			bNeed = firstlang.compare(L"ca-es") != 0 && firstlang.compare(L"ca") != 0;			
		}

		if (bNeed == false)
			status = AlreadyApplied;
	}
	else
	{
		status = CannotBeApplied;
	}
	
	g_log.Log(L"FirefoxAction::IsNeed returns %u (first lang:%s)", (wchar_t *) bNeed, (wchar_t *) firstlang.c_str());
	return bNeed;
}

void FirefoxAction::_getFirstLanguage(wstring& regvalue)
{
	if (m_languages.size() > 0)
	{		
		regvalue = m_languages[0];
		std::transform(regvalue.begin(), regvalue.end(), regvalue.begin(), ::tolower);
		return;
	}
	
	regvalue.clear();
	return;
}

void FirefoxAction::_getProfilesIniLocation(wstring &location)
{	
	wchar_t szPath[MAX_PATH];
	
	SHGetFolderPath(NULL, CSIDL_APPDATA|CSIDL_FLAG_CREATE,  NULL, 0, szPath);
	location = szPath;
	location += L"\\Mozilla\\Firefox\\profiles.ini";
}

#define	PATHKEY L"Path="

bool FirefoxAction::_getProfileLocationFromProfilesIni(wstring file, wstring &profileLocation)
{
	wifstream reader;
	wstring line;
	const int pathLen = wcslen(PATHKEY);

	reader.open(file.c_str());

	if (!reader.is_open())	
		return false;

	while(!(getline(reader, line)).eof())
	{
		if (_wcsnicmp(line.c_str(), PATHKEY, pathLen) != 0)
			continue;

		wstring path;
		wchar_t szPath[MAX_PATH];
	
		SHGetFolderPath(NULL, CSIDL_APPDATA|CSIDL_FLAG_CREATE,  NULL, 0, szPath);
		profileLocation = szPath;
		profileLocation += L"\\Mozilla\\Firefox\\";
		profileLocation += (wchar_t *)&line[pathLen];
		return true;
	}

	return false;
}

bool FirefoxAction::_getPreferencesFile(wstring &location)
{
	wstring profileIni;

	_getProfilesIniLocation(profileIni);
	
	if (_getProfileLocationFromProfilesIni(profileIni, location))
	{	
		location += L"\\prefs.js";
		return true;
	}
	else
	{
		return false;
	}
}

void FirefoxAction::ParseLanguage(wstring regvalue)
{
	wstring language;
	
	m_languages.clear();
	for (unsigned int i = 0; i < regvalue.size (); i++)
	{
		if (regvalue[i] == L',')
		{
			m_languages.push_back(language);
			language.clear();
			continue;
		}

		language += regvalue[i];		
	}

	if (language.empty() == false)
	{
		m_languages.push_back(language);
	}
}


#define USER_PREF L"user_pref(\"intl.accept_languages\", \""

bool FirefoxAction::_readLanguageCode(wstring& langcode)
{
	wstring location, line;
	wifstream reader;

	langcode.erase();
	
	if (_getPreferencesFile(location) == false)
	{
		g_log.Log(L"FirefoxAction::_readLanguageCode. No preferences file found. Firefox is not installed");
		return false;
	}
	reader.open(location.c_str());

	if (reader.is_open())
	{		
		int start, end;

		while(!(getline(reader,line)).eof())
		{			
			start = line.find(USER_PREF);

			if (start == wstring::npos)
				continue;

			start+=wcslen(USER_PREF);

			end = line.find(L"\"", start);

			if (end == wstring::npos)
				continue;

			langcode = line.substr(start, end - start);
			break;
		}
	}
	else
	{
		g_log.Log(L"FirefoxAction::_readLanguageCode cannot open %s", (wchar_t *) location.c_str());
		return false;
	}

	reader.close();
	g_log.Log(L"FirefoxAction::_readLanguageCode open %s", (wchar_t *) location.c_str());
	return true;
}
void FirefoxAction::AddCatalanToArrayAndRemoveOldIfExists()
{	
	wstring regvalue;
	vector <wstring>languages;
	vector<wstring>::iterator it;

	// Delete previous ocurrences of Catalan locale that were not first
	for (it = m_languages.begin(); it != m_languages.end(); ++it)
	{
		wstring element = *it;
		std::transform(element.begin(), element.end(), element.begin(), ::tolower);

		if (element.compare(L"ca-es") == 0 || element.compare(L"ca") == 0)
		{
			m_languages.erase(it);
			break;
		}
	}

	wstring str(L"ca");
	it = m_languages.begin();
	m_languages.insert(it, str);
}

void FirefoxAction::CreatePrefsString(wstring& string)
{
	int languages = m_languages.size();	
	
	if (languages == 1)
	{
		string = m_languages.at(0);
		return;
	}
		
	string = m_languages.at(0);
	for (int i = 1; i < languages; i++)
	{
		string += L"," + m_languages.at(i);
	}
}

void FirefoxAction::_getPrefLine(wstring langcode, wstring& line)
{
	line = USER_PREF;
	line += langcode;
	line += L"\");";
}

void FirefoxAction::_writeLanguageCode(wstring &langcode)
{
	wifstream reader;
	wofstream writer;
	wstring line, filer, filew;
	bool ret, written;
	
	_getPreferencesFile(filer);
	filew += filer;
	filew += L".new";

	reader.open(filer.c_str());

	if (!reader.is_open())
		return;

	writer.open(filew.c_str());

	if (!writer.is_open())
	{
		reader.close();
		return;
	}

	written = false;
	while(!(getline(reader,line)).eof())
	{
		if (line.find(USER_PREF) != wstring::npos)
		{
			written = true;
			_getPrefLine(langcode, line);
		}

		writer << line << L"\n";
	}

	if (written == false)
	{		
		_getPrefLine(langcode, line);
		writer << line << L"\n";
	}

	writer.close();
	reader.close();

	ret = MoveFileEx(filew.c_str(), filer.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
	
	if (ret)
	{
		status = Successful;
	} 
	else 
	{
		status = FinishedWithError;
	}
}


void FirefoxAction::Execute()
{
	wstring regvalue;

	AddCatalanToArrayAndRemoveOldIfExists();
	CreatePrefsString(regvalue);
	_writeLanguageCode(regvalue);
}

const char* FirefoxAction::GetVersion()
{
	if (m_version.length() == 0)
	{
		_readVersionAndLocale();
	}
	return m_version.c_str();
}

bool FirefoxAction::_readVersionAndLocale()
{
	if (m_registry->OpenKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Mozilla\\Mozilla Firefox", false) == false)
	{
		g_log.Log(L"FirefoxAction::_readVersionAndLocale. Cannot open registry key");
		return false;
	}
	
	wstring sreg, version, locale;
	wchar_t szVersion[1024];
	int start, end;

	if (m_registry->GetString(L"CurrentVersion", szVersion, sizeof(szVersion)))
	{
		sreg = szVersion;
		start = sreg.find(L" ");

		if (start != wstring::npos)
		{
			version = sreg.substr(0, start);
			StringConversion::ToMultiByte(wstring(version), m_version);			

			start = sreg.find(L"(", start);

			if (start != wstring::npos)
			{
				start++;
				end = sreg.find(L")", start);
				if (end != wstring::npos)
				{
					m_locale = sreg.substr(start, end-start);
				}
			}
		}

		g_log.Log(L"FirefoxAction::_readVersionAndLocale. Firefox version %s, version %s, locale %s", 
			(wchar_t*) szVersion, (wchar_t*) version.c_str(), (wchar_t*)  m_locale.c_str());
	}
	m_registry->Close();
	return true;	
}