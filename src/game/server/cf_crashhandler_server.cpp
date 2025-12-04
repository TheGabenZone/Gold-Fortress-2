//========= Copyright Valve Corporation & BetterFortress, All rights reserved. ============//
//
// Purpose: Custom Fortress 2 Server Crash Handler Implementation
//         Captures crash information and logs stack traces on Linux dedicated servers
//
//=============================================================================//

#include "cbase.h"
#include "cf_crashhandler_server.h"
#include "filesystem.h"
#include "tier1/strtools.h"

#ifdef POSIX
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <errno.h>
#endif

#include <time.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
#define MAX_STACK_FRAMES 64
#define CRASH_LOG_DIR "customfortress/crashes"

//-----------------------------------------------------------------------------
// ConVars
//-----------------------------------------------------------------------------
ConVar sv_crashreporting_enabled( "sv_crashreporting_enabled", "1", FCVAR_NONE,
	"Enable server crash reporting and stack trace logging." );

//-----------------------------------------------------------------------------
// Static members
//-----------------------------------------------------------------------------
ServerCrashMetadata_t CServerCrashHandler::s_Metadata;
bool CServerCrashHandler::s_bEnabled = false;
bool CServerCrashHandler::s_bInitialized = false;

#ifdef POSIX
// Store original signal handlers
static struct sigaction s_OldSIGSEGV;
static struct sigaction s_OldSIGABRT;
static struct sigaction s_OldSIGFPE;
static struct sigaction s_OldSIGILL;
static struct sigaction s_OldSIGBUS;

// Flag to prevent recursive crashes
static volatile sig_atomic_t s_bInCrashHandler = 0;
#endif

//-----------------------------------------------------------------------------
// Purpose: Get signal name from signal number
//-----------------------------------------------------------------------------
#ifdef POSIX
static const char* GetSignalName( int nSignal )
{
	switch ( nSignal )
	{
		case SIGSEGV: return "SIGSEGV (Segmentation Fault)";
		case SIGABRT: return "SIGABRT (Abort)";
		case SIGFPE:  return "SIGFPE (Floating Point Exception)";
		case SIGILL:  return "SIGILL (Illegal Instruction)";
		case SIGBUS:  return "SIGBUS (Bus Error)";
		default:      return "Unknown Signal";
	}
}

//-----------------------------------------------------------------------------
// Purpose: Demangle C++ symbol names
//-----------------------------------------------------------------------------
static const char* DemangleSymbol( const char* pszMangledName, char* pszBuffer, size_t nBufferSize )
{
	int nStatus = 0;
	char* pszDemangled = abi::__cxa_demangle( pszMangledName, NULL, NULL, &nStatus );
	
	if ( nStatus == 0 && pszDemangled )
	{
		V_strncpy( pszBuffer, pszDemangled, nBufferSize );
		free( pszDemangled );
		return pszBuffer;
	}
	
	V_strncpy( pszBuffer, pszMangledName, nBufferSize );
	return pszBuffer;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Initialize crash handler
//-----------------------------------------------------------------------------
void CServerCrashHandler::Init()
{
	if ( s_bInitialized )
	{
		Msg( "[ServerCrashHandler] Already initialized.\n" );
		return;
	}
	
	Msg( "[ServerCrashHandler] Initializing server crash reporting system...\n" );
	
	s_bInitialized = true;
	s_bEnabled = sv_crashreporting_enabled.GetBool();
	
	// Initialize metadata
	V_memset( &s_Metadata, 0, sizeof( s_Metadata ) );
	s_Metadata.bDedicated = true;
	
	// Collect system info
	CollectSystemInfo();
	
	// Set version
	V_snprintf( s_Metadata.szVersion, sizeof( s_Metadata.szVersion ), 
		"Custom Fortress 2 Server" );
	
#ifdef POSIX
	if ( s_bEnabled )
	{
		InstallSignalHandlers();
	}
#endif
	
	Msg( "[ServerCrashHandler] Initialized. Crash logs will be written to %s/\n", CRASH_LOG_DIR );
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown crash handler
//-----------------------------------------------------------------------------
void CServerCrashHandler::Shutdown()
{
	s_bInitialized = false;
	s_bEnabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: Enable/disable crash reporting
//-----------------------------------------------------------------------------
void CServerCrashHandler::SetEnabled( bool bEnabled )
{
	s_bEnabled = bEnabled;
	sv_crashreporting_enabled.SetValue( bEnabled );
}

bool CServerCrashHandler::IsEnabled()
{
	return s_bEnabled && s_bInitialized;
}

//-----------------------------------------------------------------------------
// Purpose: Set current map
//-----------------------------------------------------------------------------
void CServerCrashHandler::SetCurrentMap( const char *pszMapName )
{
	if ( pszMapName )
	{
		V_strncpy( s_Metadata.szMap, pszMapName, sizeof( s_Metadata.szMap ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set game mode
//-----------------------------------------------------------------------------
void CServerCrashHandler::SetGameMode( const char *pszGameMode )
{
	if ( pszGameMode )
	{
		V_strncpy( s_Metadata.szGameMode, pszGameMode, sizeof( s_Metadata.szGameMode ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set player count
//-----------------------------------------------------------------------------
void CServerCrashHandler::SetPlayerCount( int nPlayers )
{
	s_Metadata.nPlayerCount = nPlayers;
}

//-----------------------------------------------------------------------------
// Purpose: Collect all metadata
//-----------------------------------------------------------------------------
void CServerCrashHandler::CollectMetadata()
{
	// Generate unique crash ID
	GenerateCrashID();
	
	// Get timestamp
	time_t rawtime;
	time( &rawtime );
	struct tm timeinfo;
#ifdef _WIN32
	gmtime_s( &timeinfo, &rawtime );
#else
	gmtime_r( &rawtime, &timeinfo );
#endif
	strftime( s_Metadata.szTimestamp, sizeof( s_Metadata.szTimestamp ), 
		"%Y-%m-%d %H:%M:%S UTC", &timeinfo );
	
	// Get uptime
	s_Metadata.nUptime = (int)Plat_FloatTime();
	
#ifdef POSIX
	// Get memory usage on Linux
	struct rusage usage;
	if ( getrusage( RUSAGE_SELF, &usage ) == 0 )
	{
		s_Metadata.nMemoryUsageMB = (int)( usage.ru_maxrss / 1024 ); // ru_maxrss is in KB on Linux
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Collect system information
//-----------------------------------------------------------------------------
void CServerCrashHandler::CollectSystemInfo()
{
#ifdef POSIX
	// Get OS info from uname
	struct utsname unameData;
	if ( uname( &unameData ) == 0 )
	{
		V_snprintf( s_Metadata.szOS, sizeof( s_Metadata.szOS ),
			"%s %s %s", unameData.sysname, unameData.release, unameData.machine );
	}
	else
	{
		V_strncpy( s_Metadata.szOS, "Linux (unknown)", sizeof( s_Metadata.szOS ) );
	}
#else
	V_strncpy( s_Metadata.szOS, "Windows Server", sizeof( s_Metadata.szOS ) );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Generate unique crash ID
//-----------------------------------------------------------------------------
void CServerCrashHandler::GenerateCrashID()
{
	time_t rawtime;
	time( &rawtime );
	
	V_snprintf( s_Metadata.szCrashID, sizeof( s_Metadata.szCrashID ),
		"srv_%08x_%04x", (unsigned int)rawtime, rand() % 0xFFFF );
}

//-----------------------------------------------------------------------------
// Purpose: Get metadata
//-----------------------------------------------------------------------------
const ServerCrashMetadata_t& CServerCrashHandler::GetMetadata()
{
	return s_Metadata;
}

//-----------------------------------------------------------------------------
// Purpose: Write crash report to file
//-----------------------------------------------------------------------------
bool CServerCrashHandler::WriteCrashReport( const char *pszSignal, void *pFaultAddress )
{
#ifdef POSIX
	// Create crashes directory
	mkdir( CRASH_LOG_DIR, 0755 );

	// Generate filename with timestamp
	time_t tNow = time( NULL );
	struct tm* pTime = localtime( &tNow );
	
	char szFilename[256];
	V_snprintf( szFilename, sizeof(szFilename), 
		"%s/crash_%04d%02d%02d_%02d%02d%02d.log",
		CRASH_LOG_DIR,
		pTime->tm_year + 1900, pTime->tm_mon + 1, pTime->tm_mday,
		pTime->tm_hour, pTime->tm_min, pTime->tm_sec );

	FILE* pFile = fopen( szFilename, "w" );
	if ( !pFile )
	{
		// Try current directory as fallback
		V_snprintf( szFilename, sizeof(szFilename), "crash_%04d%02d%02d_%02d%02d%02d.log",
			pTime->tm_year + 1900, pTime->tm_mon + 1, pTime->tm_mday,
			pTime->tm_hour, pTime->tm_min, pTime->tm_sec );
		pFile = fopen( szFilename, "w" );
	}

	if ( !pFile )
		return false;

	// Write header
	fprintf( pFile, "=============================================================\n" );
	fprintf( pFile, "Custom Fortress 2 - Linux Dedicated Server Crash Report\n" );
	fprintf( pFile, "=============================================================\n\n" );

	// Write metadata
	fprintf( pFile, "Crash ID: %s\n", s_Metadata.szCrashID );
	fprintf( pFile, "Version: %s\n", s_Metadata.szVersion );
	fprintf( pFile, "Timestamp: %s\n", s_Metadata.szTimestamp );
	fprintf( pFile, "Map: %s\n", s_Metadata.szMap[0] ? s_Metadata.szMap : "(none)" );
	fprintf( pFile, "Game Mode: %s\n", s_Metadata.szGameMode[0] ? s_Metadata.szGameMode : "(unknown)" );
	fprintf( pFile, "Players: %d\n", s_Metadata.nPlayerCount );
	fprintf( pFile, "Uptime: %d seconds\n", s_Metadata.nUptime );
	fprintf( pFile, "Memory: %d MB\n", s_Metadata.nMemoryUsageMB );
	fprintf( pFile, "OS: %s\n\n", s_Metadata.szOS );

	// Write crash info
	fprintf( pFile, "Signal: %s\n", pszSignal );
	fprintf( pFile, "Fault Address: %p\n\n", pFaultAddress );

	// Get stack trace
	void* pStackFrames[MAX_STACK_FRAMES];
	int nFrameCount = backtrace( pStackFrames, MAX_STACK_FRAMES );

	fprintf( pFile, "Stack Trace (%d frames):\n", nFrameCount );
	fprintf( pFile, "-------------------------------------------------------------\n" );

	// Get symbol names
	char** ppSymbols = backtrace_symbols( pStackFrames, nFrameCount );
	
	if ( ppSymbols )
	{
		char szDemangled[512];
		
		for ( int i = 0; i < nFrameCount; i++ )
		{
			// Try to get more detailed info via dladdr
			Dl_info dlInfo;
			if ( dladdr( pStackFrames[i], &dlInfo ) && dlInfo.dli_sname )
			{
				DemangleSymbol( dlInfo.dli_sname, szDemangled, sizeof(szDemangled) );
				
				// Calculate offset from symbol start
				ptrdiff_t nOffset = (char*)pStackFrames[i] - (char*)dlInfo.dli_saddr;
				
				fprintf( pFile, "#%-2d %p in %s+0x%tx\n", 
					i, 
					pStackFrames[i],
					szDemangled,
					nOffset );
				fprintf( pFile, "    at %s\n", dlInfo.dli_fname ? dlInfo.dli_fname : "unknown" );
			}
			else
			{
				fprintf( pFile, "#%-2d %s\n", i, ppSymbols[i] );
			}
		}
		
		free( ppSymbols );
	}
	else
	{
		fprintf( pFile, "Failed to get symbol names\n" );
	}

	fprintf( pFile, "\n-------------------------------------------------------------\n" );
	
	// Write addr2line commands for easier debugging
	fprintf( pFile, "\nTo get source file and line numbers, run these commands:\n" );
	fprintf( pFile, "(Requires debug symbols - build with -g flag)\n\n" );
	
	for ( int i = 0; i < nFrameCount; i++ )
	{
		Dl_info dlInfo;
		if ( dladdr( pStackFrames[i], &dlInfo ) && dlInfo.dli_fname )
		{
			// Calculate offset within the shared object
			ptrdiff_t nModuleOffset = (char*)pStackFrames[i] - (char*)dlInfo.dli_fbase;
			fprintf( pFile, "addr2line -e %s -f -C 0x%tx\n", dlInfo.dli_fname, nModuleOffset );
		}
	}

	fprintf( pFile, "\n=============================================================\n" );
	fprintf( pFile, "End of crash report\n" );
	fprintf( pFile, "=============================================================\n" );

	fclose( pFile );

	// Also print to stderr
	fprintf( stderr, "\n*** SERVER CRASH: %s at %p ***\n", pszSignal, pFaultAddress );
	fprintf( stderr, "*** Crash log written to: %s ***\n\n", szFilename );
	
	return true;
#else
	return false;
#endif
}

#ifdef POSIX
//-----------------------------------------------------------------------------
// Purpose: Signal handler
//-----------------------------------------------------------------------------
void CServerCrashHandler::SignalHandler( int nSignal, siginfo_t *pInfo, void *pContext )
{
	// Prevent recursive crashes
	if ( s_bInCrashHandler )
	{
		// Already in crash handler, just exit
		_exit( 128 + nSignal );
	}
	s_bInCrashHandler = 1;

	void* pFaultAddress = pInfo ? pInfo->si_addr : NULL;
	const char* pszSignal = GetSignalName( nSignal );
	
	// Store signal info
	V_strncpy( s_Metadata.szSignal, pszSignal, sizeof( s_Metadata.szSignal ) );
	
	// Collect metadata
	CollectMetadata();
	
	// Write crash report
	WriteCrashReport( pszSignal, pFaultAddress );

	// Restore original handler and re-raise signal
	struct sigaction* pOldHandler = NULL;
	switch ( nSignal )
	{
		case SIGSEGV: pOldHandler = &s_OldSIGSEGV; break;
		case SIGABRT: pOldHandler = &s_OldSIGABRT; break;
		case SIGFPE:  pOldHandler = &s_OldSIGFPE;  break;
		case SIGILL:  pOldHandler = &s_OldSIGILL;  break;
		case SIGBUS:  pOldHandler = &s_OldSIGBUS;  break;
	}

	if ( pOldHandler )
	{
		sigaction( nSignal, pOldHandler, NULL );
	}

	// Re-raise to let Breakpad or default handler take over
	raise( nSignal );
}

//-----------------------------------------------------------------------------
// Purpose: Install signal handlers
//-----------------------------------------------------------------------------
void CServerCrashHandler::InstallSignalHandlers()
{
	struct sigaction sa;
	memset( &sa, 0, sizeof(sa) );
	
	sa.sa_sigaction = SignalHandler;
	sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
	sigemptyset( &sa.sa_mask );

	// Install handlers for common crash signals
	sigaction( SIGSEGV, &sa, &s_OldSIGSEGV );
	sigaction( SIGABRT, &sa, &s_OldSIGABRT );
	sigaction( SIGFPE,  &sa, &s_OldSIGFPE );
	sigaction( SIGILL,  &sa, &s_OldSIGILL );
	sigaction( SIGBUS,  &sa, &s_OldSIGBUS );

	Msg( "[ServerCrashHandler] Signal handlers installed for SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS\n" );
}
#endif // POSIX

//-----------------------------------------------------------------------------
// Purpose: Test crash command
//-----------------------------------------------------------------------------
void CServerCrashHandler::TestCrash()
{
	Msg( "[ServerCrashHandler] Triggering test crash...\n" );
	
	// Collect metadata before crash
	CollectMetadata();
	
	Msg( "[ServerCrashHandler] Crash ID: %s\n", s_Metadata.szCrashID );
	Msg( "[ServerCrashHandler] Map: %s\n", s_Metadata.szMap );
	Msg( "[ServerCrashHandler] Crashing now...\n" );
	
	// Trigger a segfault
	int *pNull = NULL;
	*pNull = 42;
}

//-----------------------------------------------------------------------------
// Console command to test crash
//-----------------------------------------------------------------------------
void CC_ServerCrashTest()
{
	CServerCrashHandler::TestCrash();
}

static ConCommand sv_crash_test( "sv_crash_test", CC_ServerCrashTest, 
	"Test server crash handler by triggering an intentional crash.", FCVAR_CHEAT );
