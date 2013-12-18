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

#include <string>
#include <vector>
#include <windows.h>
#include <assert.h>
#include "tp_stub.h"
#include "sqlite3/sqlite3.h"

#ifndef SQLITE_DEFAULT_SECTOR_SIZE
#	define SQLITE_DEFAULT_SECTOR_SIZE 512
#endif

//--------------------------------------------------------------------------------------------------
// ���[�e�B���e�B
//--------------------------------------------------------------------------------------------------
static int sqlite3_os_type = 0;
int isNT()
{
	if( sqlite3_os_type==0 ){
		OSVERSIONINFO sInfo;
		sInfo.dwOSVersionInfoSize = sizeof(sInfo);
		::GetVersionEx(&sInfo);
		sqlite3_os_type = sInfo.dwPlatformId==VER_PLATFORM_WIN32_NT ? 2 : 1;
	}
	return sqlite3_os_type==2;
}

bool utf8ToUtf16( const char* source, std::wstring& output )
{
	int	len = ::MultiByteToWideChar( CP_UTF8, 0, source, -1, NULL, 0);
	std::vector<wchar_t> outbuf( len+1, 0 );
	int	ret = ::MultiByteToWideChar( CP_UTF8, 0, source, -1, &(outbuf[0]), len );
	if( ret ) {
		outbuf[ret] = L'\0';
		output.assign( &(outbuf[0]) );
		return true;
	}
	return false;
}
bool utf16ToMbs( const wchar_t* source, std::string& output )
{
	int	len = ::WideCharToMultiByte( CP_ACP, 0, source, -1, NULL, 0, NULL, NULL );
	std::vector<char> outbuf( len+1, 0 );
	int	ret = ::WideCharToMultiByte( CP_ACP, 0, source, -1, &(outbuf[0]), len, NULL, NULL );
	if( ret ) {
		outbuf[ret] = L'\0';
		output.assign( &(outbuf[0]) );
		return true;
	}
	return false;
}

bool utf8ToMbs( const char* source, std::string& output )
{
	std::wstring	utf16Name;
	bool ret = utf8ToUtf16( source, utf16Name );
	if( ret ) {
		return utf16ToMbs( utf16Name.c_str(), output );
	}
	return false;
}

//--------------------------------------------------------------------------------------------------
// �t�@�C���A�N�Z�X
//--------------------------------------------------------------------------------------------------
struct xp3File
{
	const sqlite3_io_methods *pMethod;	// Must be first
	IStream*		stream_;			// stream pointer
};

static int xp3Close( sqlite3_file *id )
{
	xp3File*	file = (xp3File*)id;
	if( file->stream_ ) {
		file->stream_->Release();
		file->stream_ = NULL;
	}
	return SQLITE_OK;
}
static int xp3Read( sqlite3_file *id, void *pBuf, int iAmt, sqlite3_int64 iOfst )
{
	xp3File*	file = (xp3File*)id;
	if( file->stream_ ) {
		HRESULT hr;
		LARGE_INTEGER	dlibMove;
		dlibMove.LowPart = (DWORD)( iOfst & 0xffffffff );
		dlibMove.HighPart = (LONG)( (iOfst>>32) & 0x7fffffff );
		hr = file->stream_->Seek( dlibMove, STREAM_SEEK_SET, NULL );
		if( hr != S_OK ) {
			return SQLITE_FULL;
		}

		ULONG	numOfRead = 0;
		hr = file->stream_->Read( pBuf, iAmt, &numOfRead );
		if( hr != S_OK ) {
			return SQLITE_IOERR_READ;
		}

		if( numOfRead == (ULONG)iAmt ) {
			return SQLITE_OK;
		} else {
			memset( &((char*)pBuf)[numOfRead], 0, iAmt-numOfRead);
			return SQLITE_IOERR_SHORT_READ;
		}
	}
	return SQLITE_IOERR_READ;
}

// ���Ԃ�A�A�[�J�C�u�̎��͎��s���邾�낤���ǁA�ꉞ�����Ă���
static int xp3Write( sqlite3_file *id, const void *pBuf, int iAmt, sqlite3_int64 iOfst )
{
	xp3File*	file = (xp3File*)id;
	if( file->stream_ ) {
		HRESULT hr;
		LARGE_INTEGER	dlibMove;
		dlibMove.LowPart = (DWORD)( iOfst & 0xffffffff );
		dlibMove.HighPart = (LONG)( (iOfst>>32) & 0x7fffffff );
		hr = file->stream_->Seek( dlibMove, STREAM_SEEK_SET, NULL );
		if( hr != S_OK ) {
			return SQLITE_FULL;
		}

		ULONG	numOfWrote;
		do {
			numOfWrote = 0;
			hr = file->stream_->Write( pBuf, iAmt, &numOfWrote );
			iAmt -= numOfWrote;
			pBuf = &((char*)pBuf)[numOfWrote];
		} while( iAmt > 0 && hr == S_OK && numOfWrote > 0 );

		if( hr == S_OK && iAmt <= 0 ) {
			return SQLITE_OK;
		} else {
			return SQLITE_FULL;
		}
	}
	return SQLITE_IOERR_WRITE;
}

// �t�@�C���T�C�Y��ύX����
// ���Ԃ�A�@�\���Ȃ�
// SetSize �� E_NOTIMPL ���Ԃ��Ă���͂�
static int xp3Truncate( sqlite3_file *id, sqlite3_int64 size )
{
	xp3File*	file = (xp3File*)id;
	if( file->stream_ ) {
		HRESULT hr;
		ULARGE_INTEGER	libNewSize;
		libNewSize.LowPart = (DWORD)( size & 0xffffffff );
		libNewSize.HighPart = (DWORD)( (size>>32) & 0x7fffffff );
		hr = file->stream_->SetSize( libNewSize );
		if( hr == S_OK ) {
			return SQLITE_OK;
		} else {
			return SQLITE_IOERR_TRUNCATE;
		}
	}
	return SQLITE_IOERR_TRUNCATE;
}

// ���������݂̃f�[�^����������
static int xp3Sync(sqlite3_file *id, int flags)
{
	// �Y������̂���������Ȃ��̂ŁA�������Ȃ�
	// �n���h���ł� FlushFileBuffers ���g���݂���
	return SQLITE_OK;
}

// Determine the current size of a file in bytes
static int xp3FileSize(sqlite3_file *id, sqlite3_int64 *pSize)
{
	xp3File*	file = (xp3File*)id;
	if( file->stream_ ) {
		STATSTG		statData;
		HRESULT		hr = file->stream_->Stat( &statData, STATFLAG_NONAME );
		if( S_OK == hr ) {
			*pSize = statData.cbSize.QuadPart;
			return SQLITE_OK;
		} else {
			return SQLITE_IOERR_FSTAT;
		}
	}
	return SQLITE_IOERR_FSTAT;
}

// IStream::LockRegion/UnlockRegion���g���Ύ����ł���Ǝv�����A
// �g���g���ł͎�������Ă��Ȃ��̂ŁA�������Ȃ�
static int xp3Lock(sqlite3_file *id, int locktype)
{
	return SQLITE_OK;
}

static int xp3CheckReservedLock(sqlite3_file *id, int *pResOut)
{
	// �������b�N���Ă��Ȃ�
	return 0;
}

static int xp3Unlock(sqlite3_file *id, int locktype)
{
	// �������b�N���Ă��Ȃ�
	return SQLITE_OK;
}

static int xp3FileControl(sqlite3_file *id, int op, void *pArg)
{
	switch( op ) {
	case SQLITE_FCNTL_LOCKSTATE:
		*(int*)pArg = 0;
		return SQLITE_OK;
	}
	return SQLITE_ERROR;
}

static int xp3SectorSize(sqlite3_file *id)
{
	return SQLITE_DEFAULT_SECTOR_SIZE;
}
static int xp3DeviceCharacteristics(sqlite3_file *id)
{
	return 0;
}
static const sqlite3_io_methods xp3IoMethod = {
	1,
	xp3Close,
	xp3Read,
	xp3Write,
	xp3Truncate,
	xp3Sync,
	xp3FileSize,
	xp3Lock,
	xp3Unlock,
	xp3CheckReservedLock,
	xp3FileControl,
	xp3SectorSize,
	xp3DeviceCharacteristics
};

//--------------------------------------------------------------------------------------------------
// VFS �֐�
//--------------------------------------------------------------------------------------------------
static int xp3Open( sqlite3_vfs *vfs, const char *zName, sqlite3_file *id, int flags, int *pOutFlags )
{
	std::wstring	utf16Name;
	bool ret = utf8ToUtf16( zName, utf16Name );
	if( ret == false )
		return SQLITE_NOMEM;

	if( flags & (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_DELETEONCLOSE) ) {
		// �������݃A�N�Z�X�␶���A�������ɍ폜����Ȃǂ͏o���Ȃ�
		return SQLITE_READONLY;
	}

	IStream* stream;
	if( (stream = TVPCreateIStream(utf16Name.c_str(), TJS_BS_READ)) == NULL) {
		return SQLITE_CANTOPEN;
	}

	if( pOutFlags ) {
		*pOutFlags = SQLITE_OPEN_READONLY;
	}

	xp3File *pFile = (xp3File*)id;
	memset(pFile, 0, sizeof(*pFile));
	pFile->pMethod = &xp3IoMethod;
	pFile->stream_ = stream;
	return SQLITE_OK;
}

static int xp3Delete( sqlite3_vfs *vfs, const char *zFilename, int syncDir )
{
	return SQLITE_READONLY;
}

static int xp3Access( sqlite3_vfs *vfs, const char *zFilename, int flags, int *pResOut )
{
	std::wstring	utf16Name;
	bool ret = utf8ToUtf16( zFilename, utf16Name );
	if( ret == false )
		return SQLITE_NOMEM;

	int rc = 0;
	switch( flags ) {
	case SQLITE_ACCESS_READ:
	case SQLITE_ACCESS_EXISTS:
		if( TVPIsExistentStorage(utf16Name.c_str()) ) {
			rc = 1;
		} else {
			rc = 0;
		}
		break;
	case SQLITE_ACCESS_READWRITE:
		rc = 0;
		break;
	default:
		assert(!"Invalid flags argument");
	}
	*pResOut = rc;
	return SQLITE_OK;
}

// XP3���ł̓t���p�X��Ԃ����A���̂܂ܕԂ�
// �p�b�`�Ȃǂ𓖂ĂāA�����瑤��ǂޕK�v���o��\��������̂ŁA�t���p�X�œǂނ̂͑Ó��ł͂Ȃ��Ǝv����
static int xp3FullPathname( sqlite3_vfs *vfs, const char *zRelative, int nFull, char *zFull )
{
	sqlite3_snprintf(nFull, zFull, "%s", zRelative);
	return SQLITE_OK;
}
// DLL �� XP3 ���ł͂Ȃ����̂Ƃ���
static void *xp3DlOpen(sqlite3_vfs *pVfs, const char *zFilename)
{
	HANDLE h;
	if( isNT() ){
		std::wstring	utf16Name;
		bool ret = utf8ToUtf16( zFilename, utf16Name );
		if( ret == false ) return NULL;
		h = ::LoadLibraryW( utf16Name.c_str()  );
	} else {
		std::string	mbsName;
		bool ret = utf8ToMbs( zFilename, mbsName );
		if( ret == false ) return NULL;
		h = ::LoadLibraryA( mbsName.c_str() );
	}
	return (void*)h;
}

static void xp3DlError(sqlite3_vfs *pVfs, int nBuf, char *zBufOut )
{
	::FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, zBufOut, nBuf-1, 0 );
}

void (*xp3DlSym( sqlite3_vfs *pVfs, void *pHandle, const char *zSymbol ))(void)
{
	return (void(*)(void))::GetProcAddress((HMODULE)pHandle, zSymbol);
}
void xp3DlClose( sqlite3_vfs *pVfs, void *pHandle )
{
	::FreeLibrary( (HMODULE)pHandle );
}

static int xp3Randomness(sqlite3_vfs *pVfs, int nBuf, char *zBuf)
{
	int n = 0;
	if( sizeof(SYSTEMTIME) <= nBuf-n ) {
		SYSTEMTIME x;
		GetSystemTime(&x);
		memcpy(&zBuf[n], &x, sizeof(x));
		n += sizeof(x);
	}
	if( sizeof(DWORD) <= nBuf-n ) {
		DWORD pid = GetCurrentProcessId();
		memcpy(&zBuf[n], &pid, sizeof(pid));
		n += sizeof(pid);
	}
	if( sizeof(DWORD) <= nBuf-n ) {
		DWORD cnt = GetTickCount();
		memcpy(&zBuf[n], &cnt, sizeof(cnt));
		n += sizeof(cnt);
	}
	if( sizeof(LARGE_INTEGER) <= nBuf-n ) {
		LARGE_INTEGER i;
		QueryPerformanceCounter(&i);
		memcpy(&zBuf[n], &i, sizeof(i));
		n += sizeof(i);
	}
	return n;
}
static int xp3Sleep(sqlite3_vfs *pVfs, int microsec)
{
	::Sleep( (microsec+999)/1000 );
	return ((microsec+999)/1000)*1000;
}

static int xp3CurrentTime( sqlite3_vfs *pVfs, double *prNow )
{
	FILETIME ft;
	/* FILETIME structure is a 64-bit value representing the number of 
		100-nanosecond intervals since January 1, 1601 (= JD 2305813.5). 
	*/

	double now;
	GetSystemTimeAsFileTime( &ft );
	now = ((double)ft.dwHighDateTime) * 4294967296.0;
	*prNow = (now + ft.dwLowDateTime)/864000000000.0 + 2305813.5;
	return 0;
}

static int xp3GetLastError(sqlite3_vfs *pVfs, int nBuf, char *zBuf){
	DWORD error = GetLastError();
	if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
						NULL,
						error,
						0,
						zBuf,
						nBuf-1,
						0))
	{
		sqlite3_snprintf(nBuf, zBuf, "OsError 0x%x (%u)", error, error);
	}
	return SQLITE_OK;
}


sqlite3_vfs *getXp3Vfs()
{
	static sqlite3_vfs xp3Vfs = {
		1,					// iVersion
		sizeof(xp3File),	// szOsFile
		MAX_PATH,			// mxPathname
		0,					// pNext
		"xp3",				// zName
		0,					// pAppData

		xp3Open,			// xOpen
		xp3Delete,			// xDelete
		xp3Access,			// xAccess
		xp3FullPathname,	// xFullPathname
		xp3DlOpen,			// xDlOpen
		xp3DlError,			// xDlError
		xp3DlSym,			// xDlSym
		xp3DlClose,			// xDlClose
		xp3Randomness,		// xRandomness
		xp3Sleep,			// xSleep
		xp3CurrentTime,		// xCurrentTime
		xp3GetLastError     // xGetLastError
		};
	return &xp3Vfs;
}


