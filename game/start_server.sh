#!/bin/bash
# Custom Fortress 2 - Linux Dedicated Server Launcher
# 
# REQUIREMENTS:
# 1. Install TF2 Dedicated Server via SteamCMD:
#    steamcmd +force_install_dir /path/to/tf2ds +login anonymous +app_update 232250 validate +quit
#
# 2. Place this mod's files in the same directory as the TF2 dedicated server
#
# 3. Get a Game Server Login Token (GSLT) from:
#    https://steamcommunity.com/dev/managegameservers
#    Use App ID 440 (TF2) when creating the token
#
# USAGE:
#    ./start_server.sh [additional srcds parameters]
#
# EXAMPLE:
#    ./start_server.sh +map ctf_2fort +maxplayers 24
#
# WITH GSLT (recommended):
#    ./start_server.sh +sv_setsteamaccount YOUR_GSLT_TOKEN_HERE +map ctf_2fort +maxplayers 24

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Check if srcds_linux64 exists
if [ ! -f "$SCRIPT_DIR/srcds_linux64" ]; then
    echo "ERROR: srcds_linux64 not found!"
    echo ""
    echo "You need to install the TF2 Dedicated Server first:"
    echo "  steamcmd +force_install_dir \"$SCRIPT_DIR\" +login anonymous +app_update 232250 validate +quit"
    echo ""
    exit 1
fi

# Check if customfortress folder exists
if [ ! -d "$SCRIPT_DIR/customfortress" ]; then
    echo "ERROR: customfortress folder not found!"
    echo "Make sure the Custom Fortress 2 mod files are installed."
    exit 1
fi

# Use server-specific gameinfo.txt (handles paths for dedicated server)
if [ -f "$SCRIPT_DIR/customfortress/gameinfo_server.txt" ]; then
    cp "$SCRIPT_DIR/customfortress/gameinfo_server.txt" "$SCRIPT_DIR/customfortress/gameinfo.txt"
    echo "Using server gameinfo configuration."
fi

# Create server binary symlinks if they don't exist
# srcds looks for *_srv.so but our mod builds *.so
MODBIN="$SCRIPT_DIR/customfortress/bin/linux64"
if [ -f "$MODBIN/server.so" ] && [ ! -f "$MODBIN/server_srv.so" ]; then
    ln -s server.so "$MODBIN/server_srv.so"
    echo "Created server_srv.so symlink."
fi

# Launch the server
cd "$SCRIPT_DIR"

# Set library path for Source engine shared libraries
# Include both the main bin and the mod's bin directories
export LD_LIBRARY_PATH="$SCRIPT_DIR/bin/linux64:$SCRIPT_DIR/customfortress/bin/linux64:$SCRIPT_DIR/tf/bin/linux64:$LD_LIBRARY_PATH"

# Note: -insecure disables VAC. Remove it for production servers that need VAC protection.
./srcds_linux64 -game customfortress -console "$@"