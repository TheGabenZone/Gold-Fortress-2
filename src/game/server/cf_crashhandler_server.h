//========= Copyright Valve Corporation & BetterFortress, All rights reserved. ============//
//
// Purpose: Custom Fortress 2 Server Crash Handler - Collects crash data on Linux dedicated servers
//
//=============================================================================//

#ifndef CF_CRASHHANDLER_SERVER_H
#define CF_CRASHHANDLER_SERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"

//-----------------------------------------------------------------------------
// Server Crash Metadata Structure
//-----------------------------------------------------------------------------
struct ServerCrashMetadata_t
{
	char szVersion[64];				// Game version/build number
	char szMap[128];				// Current map name
	char szGameMode[64];			// Game mode (pl, cp, koth, etc)
	char szTimestamp[64];			// UTC timestamp
	char szCrashID[40];				// Unique crash identifier
	
	int nMemoryUsageMB;				// Current memory usage
	int nUptime;					// Server uptime in seconds
	int nPlayerCount;				// Number of connected players
	int nTickRate;					// Server tick rate
	
	char szOS[128];					// OS version
	char szSignal[64];				// Signal that caused crash (Linux)
	
	bool bDedicated;				// Always true for server
};

//-----------------------------------------------------------------------------
// Server Crash Handler Interface
//-----------------------------------------------------------------------------
class CServerCrashHandler
{
public:
	// Initialize crash handler - call early in server startup
	static void Init();
	static void Shutdown();
	
	// Enable/disable crash reporting
	static void SetEnabled( bool bEnabled );
	static bool IsEnabled();
	
	// Set game metadata (called when map loads, etc)
	static void SetCurrentMap( const char *pszMapName );
	static void SetGameMode( const char *pszGameMode );
	static void SetPlayerCount( int nPlayers );
	
	// Get crash metadata
	static const ServerCrashMetadata_t& GetMetadata();
	
	// Test crash command
	static void TestCrash();
	
private:
	static void CollectMetadata();
	static void CollectSystemInfo();
	static void GenerateCrashID();
	static bool WriteCrashReport( const char *pszSignal, void *pFaultAddress );
	
#ifdef POSIX
	static void SignalHandler( int nSignal, siginfo_t *pInfo, void *pContext );
	static void InstallSignalHandlers();
#endif
	
	static ServerCrashMetadata_t s_Metadata;
	static bool s_bEnabled;
	static bool s_bInitialized;
};

//-----------------------------------------------------------------------------
// ConVars
//-----------------------------------------------------------------------------
extern ConVar sv_crashreporting_enabled;

#endif // CF_CRASHHANDLER_SERVER_H
