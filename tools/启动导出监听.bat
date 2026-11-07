@echo off
rem 双击一次即可：常驻后台，之后板上点 SAVE 会自动保存到 exports\ 目录
cd /d "%~dp0"
start "export-watcher" /min pythonw "%~dp0export_watcher.py"
exit
