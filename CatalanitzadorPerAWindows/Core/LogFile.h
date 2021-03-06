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
 
#pragma once

#include <windows.h>

#define BUFFER_SIZE 2048

class LogFile
{
public:

	LogFile();
	~LogFile();
	bool CreateLog(wchar_t* logFileName, wchar_t* appName);
	void Close();

	void Log(wchar_t* string);
	void Log(wchar_t* format, wchar_t* string);
	void Log(wchar_t* format, wchar_t* string1, wchar_t* string2);
	void Log(wchar_t* format, wchar_t* string1, wchar_t* string2, wchar_t* string3);

private:

	void _stringTime();
	void _write(wchar_t* string);
	void _writeLine(wchar_t* string);
	void _writeCompileTime(wchar_t* appName);
	void _today();

	wchar_t m_szFilename [MAX_PATH];
	wchar_t m_szText [BUFFER_SIZE];
	HANDLE m_hLog;	
		
};