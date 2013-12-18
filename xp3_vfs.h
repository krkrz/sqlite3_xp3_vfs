/****************************************************************************/
/*! @file
@brief �g���g����XP3 �p VFS

�ǂݎ���p�ŁA���b�N�Ȃǂ��T�|�[�g���Ă��Ȃ�
�������̃��\�b�h�� SQLite �� os_win.c ���痬�p���Ă���

-----------------------------------------------------------------------------
	Copyright (C) 2008 T.Imoto <http://www.kaede-software.com>
-----------------------------------------------------------------------------
@author		T.Imoto
@date		2008/04/15
@note
*****************************************************************************/

#ifndef __XP3_VFS_H__
#define __XP3_VFS_H__

sqlite3_vfs *getXp3Vfs();


int isNT();
bool utf8ToUtf16( const char* source, std::wstring& output );
bool utf16ToMbs( const wchar_t* source, std::string& output );
bool utf8ToMbs( const char* source, std::string& output );

#endif


