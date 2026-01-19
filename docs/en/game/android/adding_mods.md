
# Adding Third Party Mods on Android

## MUST

Give Manage All Storage permissions
- This is a limitation by google, apps can request all storage access or get no user accessible storage
- We only access /Documents/cataclysm-bn/, so if you somehow have granular permissions just give that folder

## DON'T

Give access to media storage
- Media storage does not grant access to documents folders
- It appears to grant access to aggregated music folders instead

Refuse storage permissions
- We can't really do anything with that
- The game is still playable; you just cant use third party mods


## After granting Permission

Launch the game once and the subfolder should be generated in Documents/cataclysm-bn/
Input mods into the mods subfolder, and they should appear

