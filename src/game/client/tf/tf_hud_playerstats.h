//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TF_HUD_PLAYERSTATS_H
#define TF_HUD_PLAYERSTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Displays player stats when gf_toggle_player_stats is enabled
//-----------------------------------------------------------------------------
class CTFHudPlayerStats : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CTFHudPlayerStats, vgui::Panel );

public:
	CTFHudPlayerStats( const char *pElementName );

	virtual void Init();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint();
	virtual bool ShouldDraw();

protected:
	virtual void OnThink();

private:
	vgui::HFont m_hTextFont;
	Color m_TextColor;
	
	CPanelAnimationVarAliasType( float, m_flXPos, "xpos", "c-100", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flYPos, "ypos", "c100", "proportional_float" );
};

#endif // TF_HUD_PLAYERSTATS_H
