@echo off
mkdir distribution
xcopy dll\*.dll distribution\
xcopy /s /e /i ..\data distribution\data
if exist distribution\data\mods\Arcana_BN rmdir /s /q distribution\data\mods\Arcana_BN
xcopy /s /e /i ..\external\cdda-arcana-mod\Arcana_BN distribution\data\mods\Arcana_BN
xcopy /s /e /i ..\config distribution\config
xcopy /s /e /i ..\gfx distribution\gfx
xcopy /s /e /i ..\lang\mo distribution\lang\mo
copy ..\Cataclysm*.exe distribution\
echo Distribution files has been put into `distribution\' directory.
pause
