@echo off

set dbgpath=\"C:\Program Files (x86)\Debugging Tools for Windows (x86)\windbg.exe\"


REG ADD "HKCU\Software\Classes\.dmp" /f /d dmpfile

set val=%dbgpath%,0
REG ADD "HKCU\Software\Classes\dmpfile\DefaultIcon" /f /d "%val%"

set val=%dbgpath% -z \"%%1\"
REG ADD "HKCU\Software\Classes\dmpfile\Shell\open\command" /f /d "%val%

set val=%dbgpath% -c \"!wow64exts.sw; !analyze -v\" -z \"%%1\"
REG ADD "HKCU\Software\Classes\dmpfile\Shell\Analyze With Windbg  - wow64\command" /f /d "%val%"

set val=%dbgpath% -c \"!analyze -v\" -z \"%%1\"
REG ADD "HKCU\Software\Classes\dmpfile\Shell\Analyze With Windbg\command" /f /d "%val%"

REG DELETE "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.dmp\UserChoice" /f

set /p finish="finsh!"