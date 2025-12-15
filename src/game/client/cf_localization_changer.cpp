//========= Copyright Custom Fortress 2, All rights reserved. ============//
//
// Purpose: Client-side localization language changer
//
//=============================================================================//

#include "cbase.h"
#include <vgui/ILocalize.h>
#include "convar.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char *g_pszLocalizationFiles[] = 
{
	"resource/goldfortress_%s.txt",
	"resource/tf_%s.txt",
	"resource/chat_%s.txt",
	"resource/closecaption_%s.txt"
};

//-----------------------------------------------------------------------------
// Purpose: Loads localization files for the specified language
//-----------------------------------------------------------------------------
static void LoadLocalizationLanguage( const char *pszLanguage )
{
	if ( !g_pVGuiLocalize )
	{
		Warning( "Localization system not available!\n" );
		return;
	}

	// Remove all existing localization strings
	g_pVGuiLocalize->RemoveAll();

	// Reload localization files with the new language
	char szPath[MAX_PATH];
	bool bSuccess = false;

	for ( int i = 0; i < ARRAYSIZE( g_pszLocalizationFiles ); i++ )
	{
		V_snprintf( szPath, sizeof( szPath ), g_pszLocalizationFiles[i], pszLanguage );
		
		// Check if file exists before trying to load
		if ( g_pFullFileSystem->FileExists( szPath, "GAME" ) )
		{
			if ( g_pVGuiLocalize->AddFile( szPath, "GAME", false ) )
			{
				Msg( "Loaded: %s\n", szPath );
				bSuccess = true;
			}
			else
			{
				Warning( "Failed to load: %s\n", szPath );
			}
		}
	}

	// Reload the base localization files that exist
	// Using %language% token for automatic language substitution
	if ( g_pFullFileSystem->FileExists( "resource/valve_%language%.txt", "GAME" ) )
	{
		g_pVGuiLocalize->AddFile( "resource/valve_%language%.txt", "GAME", true );
	}

	if ( bSuccess )
	{
		Msg( "Successfully changed localization to: %s\n", pszLanguage );
		Msg( "Note: Some UI elements may require a map reload or restart to fully update.\n" );
	}
	else
	{
		Warning( "Failed to load localization files for language: %s\n", pszLanguage );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Callback when language preference changes
//-----------------------------------------------------------------------------
static void OnLocalizationLanguageChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConVar *pConVar = (ConVar *)var;
	const char *pszLanguage = pConVar->GetString();

	// Validate the language
	if ( Q_stricmp( pszLanguage, "english" ) != 0 && 
		 Q_stricmp( pszLanguage, "portuguese" ) != 0 )
	{
		Warning( "Invalid language '%s' in cl_localization_language. Using 'english'.\n", pszLanguage );
		pConVar->SetValue( "english" );
		return;
	}

	// Only load if it's actually changing
	if ( Q_stricmp( pszLanguage, pOldValue ) != 0 )
	{
		LoadLocalizationLanguage( pszLanguage );
	}
}

// ConVar to store the user's language preference
ConVar cl_localization_language( "cl_localization_language", "english", FCVAR_ARCHIVE, "Preferred localization language (english, portuguese)", OnLocalizationLanguageChanged );

//-----------------------------------------------------------------------------
// Purpose: Auto-completion function for language arguments
//-----------------------------------------------------------------------------
static int LanguageCompletionFunc( const char *partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH] )
{
	const char *pszLanguages[] = { "english", "portuguese" };
	int nMatches = 0;
	int nPartialLength = Q_strlen( partial );

	for ( int i = 0; i < ARRAYSIZE( pszLanguages ); i++ )
	{
		if ( nPartialLength == 0 || Q_strnicmp( pszLanguages[i], partial, nPartialLength ) == 0 )
		{
			Q_strncpy( commands[nMatches], pszLanguages[i], COMMAND_COMPLETION_ITEM_LENGTH );
			nMatches++;
		}
	}

	return nMatches;
}

//-----------------------------------------------------------------------------
// Purpose: Changes the game's localization language on the fly
//-----------------------------------------------------------------------------
static void CC_ChangeLocalization( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "Usage: cl_change_localization <language>\n" );
		Msg( "Available languages: english, portuguese\n" );
		Msg( "Current language: %s\n", cl_localization_language.GetString() );
		return;
	}

	const char *pszLanguage = args.Arg( 1 );

	// Validate the language
	if ( Q_stricmp( pszLanguage, "english" ) != 0 && 
		 Q_stricmp( pszLanguage, "portuguese" ) != 0 )
	{
		Warning( "Invalid language '%s'. Available languages: english, portuguese\n", pszLanguage );
		return;
	}

	// Save the preference (this will trigger the callback which loads the language)
	cl_localization_language.SetValue( pszLanguage );
}

static ConCommand cl_change_localization( "cl_change_localization", CC_ChangeLocalization, "Changes the game's localization language. Usage: cl_change_localization <language>\nAvailable languages: english, portuguese", 0, LanguageCompletionFunc );
