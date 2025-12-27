//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "tf_hud_playerstats.h"
#include "iclientmode.h"
#include "c_tf_player.h"
#include "c_tf_playerresource.h"
#include "tf_weaponbase.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "tf_shareddefs.h"
#include "tf_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

ConVar gf_toggle_player_stats( "gf_toggle_player_stats", "0", FCVAR_ARCHIVE | FCVAR_CHEAT, "Toggle display of player stats (health, class, ammo, etc.)" );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFHudPlayerStats::CTFHudPlayerStats( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudPlayerStats" )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	
	SetHiddenBits( HIDEHUD_MISCSTATUS );
	
	SetVisible( true );
	SetEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerStats::Init()
{
	CHudElement::Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerStats::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_hTextFont = pScheme->GetFont( "DefaultVerySmall", true );
	m_TextColor = pScheme->GetColor( "TanLight", Color( 255, 255, 255, 255 ) );
	
	SetPaintBackgroundEnabled( false );
	
	// Set bounds large enough to cover the whole screen so text can draw anywhere
	int screenWide, screenTall;
	GetHudSize( screenWide, screenTall );
	SetBounds( 0, 0, screenWide, screenTall );
	SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFHudPlayerStats::ShouldDraw()
{
	if ( !gf_toggle_player_stats.GetBool() )
		return false;

	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pPlayer )
		return false;

	// Get the entity the player is looking at
	int iTargetEntIndex = pPlayer->GetIDTarget();
	if ( iTargetEntIndex <= 0 )
		return false;

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerStats::OnThink()
{
	BaseClass::OnThink();
	
	// Force update every frame when enabled
	if ( gf_toggle_player_stats.GetBool() )
	{
		SetVisible( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerStats::Paint()
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pLocalPlayer )
		return;

	// Get the entity the player is looking at
	int iTargetEntIndex = pLocalPlayer->GetIDTarget();
	if ( iTargetEntIndex <= 0 )
		return;

	C_BaseEntity *pTargetEntity = ClientEntityList().GetEnt( iTargetEntIndex );
	if ( !pTargetEntity )
		return;

	// Get screen dimensions to position near crosshair
	int screenWide, screenTall;
	GetHudSize( screenWide, screenTall );
	
	// Position to the right of crosshair center
	int xPos = (screenWide / 2) + 50;
	int yPos = (screenTall / 2) - 50;
	int lineHeight = surface()->GetFontTall( m_hTextFont ) + 2;

	surface()->DrawSetTextFont( m_hTextFont );

	// Helper lambda to draw text with outline
	auto DrawTextWithOutline = [&](int x, int y, const wchar_t* text, int len) {
		// Draw black outline
		surface()->DrawSetTextColor( Color( 0, 0, 0, 255 ) );
		surface()->DrawSetTextPos( x - 1, y - 1 );
		surface()->DrawPrintText( text, len );
		surface()->DrawSetTextPos( x + 1, y - 1 );
		surface()->DrawPrintText( text, len );
		surface()->DrawSetTextPos( x - 1, y + 1 );
		surface()->DrawPrintText( text, len );
		surface()->DrawSetTextPos( x + 1, y + 1 );
		surface()->DrawPrintText( text, len );
		
		// Draw main text
		surface()->DrawSetTextColor( m_TextColor );
		surface()->DrawSetTextPos( x, y );
		surface()->DrawPrintText( text, len );
	};

	// Check if the target is a player
	C_TFPlayer *pTargetPlayer = ToTFPlayer( pTargetEntity );
	if ( pTargetPlayer )
	{
		// Draw player name
		wchar_t wzName[64];
		_snwprintf( wzName, ARRAYSIZE(wzName), L"Name: %S", pTargetPlayer->GetPlayerName() );
		wzName[ARRAYSIZE(wzName)-1] = '\0';
		
		DrawTextWithOutline( xPos, yPos, wzName, wcslen(wzName) );
		yPos += lineHeight;

		// Draw health
		wchar_t wzHealth[64];
		int iHealth = pTargetPlayer->GetHealth();
		int iMaxHealth = pTargetPlayer->GetMaxHealth();
		_snwprintf( wzHealth, ARRAYSIZE(wzHealth), L"Health: %d / %d", iHealth, iMaxHealth );
		wzHealth[ARRAYSIZE(wzHealth)-1] = '\0';
		
		DrawTextWithOutline( xPos, yPos, wzHealth, wcslen(wzHealth) );
		yPos += lineHeight;

		// Draw class
		wchar_t wzClass[64];
		const char *pszClassName = "Unknown";
		int iClass = pTargetPlayer->GetPlayerClass()->GetClassIndex();
		
		switch ( iClass )
		{
			case TF_CLASS_SCOUT:		pszClassName = "Scout"; break;
			case TF_CLASS_SNIPER:		pszClassName = "Sniper"; break;
			case TF_CLASS_SOLDIER:		pszClassName = "Soldier"; break;
			case TF_CLASS_DEMOMAN:		pszClassName = "Demoman"; break;
			case TF_CLASS_MEDIC:		pszClassName = "Medic"; break;
			case TF_CLASS_HEAVYWEAPONS:	pszClassName = "Heavy"; break;
			case TF_CLASS_PYRO:			pszClassName = "Pyro"; break;
			case TF_CLASS_SPY:			pszClassName = "Spy"; break;
			case TF_CLASS_ENGINEER:		pszClassName = "Engineer"; break;
			case TF_CLASS_CIVILIAN:		pszClassName = "Civilian"; break;
		}
		
		_snwprintf( wzClass, ARRAYSIZE(wzClass), L"Class: %S", pszClassName );
		wzClass[ARRAYSIZE(wzClass)-1] = '\0';
		
		DrawTextWithOutline( xPos, yPos, wzClass, wcslen(wzClass) );
		yPos += lineHeight;

		// Draw class-specific stats
		if ( iClass == TF_CLASS_ENGINEER )
		{
			// Show metal count
			int iMetal = pTargetPlayer->GetAmmoCount( TF_AMMO_METAL );
			wchar_t wzMetal[64];
			_snwprintf( wzMetal, ARRAYSIZE(wzMetal), L"Metal: %d", iMetal );
			wzMetal[ARRAYSIZE(wzMetal)-1] = '\0';
			DrawTextWithOutline( xPos, yPos, wzMetal, wcslen(wzMetal) );
			yPos += lineHeight;
		}
		else if ( iClass == TF_CLASS_MEDIC )
		{
			// Show ubercharge percentage
			CTFWeaponBase *pWeapon = pTargetPlayer->GetActiveTFWeapon();
			if ( pWeapon )
			{
				float flCharge = 0.0f;
				pTargetPlayer->MedicGetChargeLevel( &pWeapon );
				if ( pWeapon )
				{
					CBaseEntity *pMedigun = pWeapon;
					if ( pMedigun )
					{
						flCharge = pTargetPlayer->MedicGetChargeLevel();
					}
				}
				wchar_t wzUber[64];
				_snwprintf( wzUber, ARRAYSIZE(wzUber), L"Ubercharge: %.0f%%", flCharge * 100.0f );
				wzUber[ARRAYSIZE(wzUber)-1] = '\0';
				DrawTextWithOutline( xPos, yPos, wzUber, wcslen(wzUber) );
				yPos += lineHeight;
			}
		}
		else if ( iClass == TF_CLASS_SPY )
		{
			// Show disguise info
			int iDisguiseClass = pTargetPlayer->m_Shared.GetDisguiseClass();
			int iDisguiseTeam = pTargetPlayer->m_Shared.GetDisguiseTeam();
			
			if ( iDisguiseClass != TF_CLASS_UNDEFINED )
			{
				const char *pszDisguiseClass = "Unknown";
				switch ( iDisguiseClass )
				{
					case TF_CLASS_SCOUT:		pszDisguiseClass = "Scout"; break;
					case TF_CLASS_SNIPER:		pszDisguiseClass = "Sniper"; break;
					case TF_CLASS_SOLDIER:		pszDisguiseClass = "Soldier"; break;
					case TF_CLASS_DEMOMAN:		pszDisguiseClass = "Demoman"; break;
					case TF_CLASS_MEDIC:		pszDisguiseClass = "Medic"; break;
					case TF_CLASS_HEAVYWEAPONS:	pszDisguiseClass = "Heavy"; break;
					case TF_CLASS_PYRO:			pszDisguiseClass = "Pyro"; break;
					case TF_CLASS_SPY:			pszDisguiseClass = "Spy"; break;
					case TF_CLASS_ENGINEER:		pszDisguiseClass = "Engineer"; break;
				}
				
				const char *pszDisguiseTeam = "Unknown";
				if ( iDisguiseTeam == TF_TEAM_RED ) pszDisguiseTeam = "RED";
				else if ( iDisguiseTeam == TF_TEAM_BLUE ) pszDisguiseTeam = "BLU";
				
				wchar_t wzDisguise[128];
				_snwprintf( wzDisguise, ARRAYSIZE(wzDisguise), L"Disguise: %S %S", pszDisguiseTeam, pszDisguiseClass );
				wzDisguise[ARRAYSIZE(wzDisguise)-1] = '\0';
				DrawTextWithOutline( xPos, yPos, wzDisguise, wcslen(wzDisguise) );
				yPos += lineHeight;
			}
			
			// Show cloak meter
			float flCloakMeter = pTargetPlayer->m_Shared.GetSpyCloakMeter();
			bool bCloaked = pTargetPlayer->m_Shared.IsStealthed();
			wchar_t wzCloak[64];
			if ( bCloaked )
			{
				_snwprintf( wzCloak, ARRAYSIZE(wzCloak), L"Cloak: %.0f%% (Draining)", flCloakMeter );
			}
			else
			{
				_snwprintf( wzCloak, ARRAYSIZE(wzCloak), L"Cloak: %.0f%% (Recharging)", flCloakMeter );
			}
			wzCloak[ARRAYSIZE(wzCloak)-1] = '\0';
			DrawTextWithOutline( xPos, yPos, wzCloak, wcslen(wzCloak) );
			yPos += lineHeight;
		}

		// Draw weapon and ammo info
		CTFWeaponBase *pWeapon = pTargetPlayer->GetActiveTFWeapon();
		if ( pWeapon )
		{
			wchar_t wzWeapon[128];
			const char *pszWeaponName = pWeapon->GetPrintName();
			
			// Localize the weapon name
			const wchar_t *pszLocalizedName = g_pVGuiLocalize->Find( pszWeaponName );
			if ( pszLocalizedName )
			{
				_snwprintf( wzWeapon, ARRAYSIZE(wzWeapon), L"Weapon: %s", pszLocalizedName );
			}
			else
			{
				// Fallback to unlocalized name
				if ( !pszWeaponName || !pszWeaponName[0] )
					pszWeaponName = pWeapon->GetName();
				_snwprintf( wzWeapon, ARRAYSIZE(wzWeapon), L"Weapon: %S", pszWeaponName );
			}
			wzWeapon[ARRAYSIZE(wzWeapon)-1] = '\0';
			
			DrawTextWithOutline( xPos, yPos, wzWeapon, wcslen(wzWeapon) );
			yPos += lineHeight;

			// Draw ammo
			if ( pWeapon->UsesPrimaryAmmo() )
			{
				int iAmmoType = pWeapon->GetPrimaryAmmoType();
				int iAmmoInClip = pWeapon->Clip1();
				int iAmmoReserve = pTargetPlayer->GetAmmoCount( iAmmoType );
				
				wchar_t wzAmmo[64];
				if ( iAmmoInClip >= 0 )
				{
					// Has clip
					_snwprintf( wzAmmo, ARRAYSIZE(wzAmmo), L"Ammo: %d / %d", iAmmoInClip, iAmmoReserve );
				}
				else
				{
					// No clip (e.g., melee weapons)
					_snwprintf( wzAmmo, ARRAYSIZE(wzAmmo), L"Ammo: %d", iAmmoReserve );
				}
				wzAmmo[ARRAYSIZE(wzAmmo)-1] = '\0';
				
				DrawTextWithOutline( xPos, yPos, wzAmmo, wcslen(wzAmmo) );
				yPos += lineHeight;
			}
		}

		// Draw team
		wchar_t wzTeam[64];
		const char *pszTeamName = "Unknown";
		int iTeam = pTargetPlayer->GetTeamNumber();
		
		switch ( iTeam )
		{
			case TF_TEAM_RED:	pszTeamName = "RED"; break;
			case TF_TEAM_BLUE:	pszTeamName = "BLU"; break;
			default:			pszTeamName = "Spectator"; break;
		}
		
		_snwprintf( wzTeam, ARRAYSIZE(wzTeam), L"Team: %S", pszTeamName );
		wzTeam[ARRAYSIZE(wzTeam)-1] = '\0';
		
		DrawTextWithOutline( xPos, yPos, wzTeam, wcslen(wzTeam) );
	}
	else
	{
		// Display basic entity info for non-players
		wchar_t wzEntityInfo[128];
		_snwprintf( wzEntityInfo, ARRAYSIZE(wzEntityInfo), L"Entity: %S", pTargetEntity->GetClassname() );
		wzEntityInfo[ARRAYSIZE(wzEntityInfo)-1] = '\0';
		
		DrawTextWithOutline( xPos, yPos, wzEntityInfo, wcslen(wzEntityInfo) );

		// Show health if the entity has health
		if ( pTargetEntity->GetHealth() > 0 )
		{
			wchar_t wzHealth[64];
			_snwprintf( wzHealth, ARRAYSIZE(wzHealth), L"Health: %d / %d", pTargetEntity->GetHealth(), pTargetEntity->GetMaxHealth() );
			wzHealth[ARRAYSIZE(wzHealth)-1] = '\0';
			
			DrawTextWithOutline( xPos, yPos, wzHealth, wcslen(wzHealth) );
		}
	}
}

DECLARE_HUDELEMENT( CTFHudPlayerStats );
