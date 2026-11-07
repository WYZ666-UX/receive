﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿<#
自动保存: 每 N 秒检查一次工作区, 有改动就提交一次。
由 .vscode/tasks.json 在打开工程时自动启动; 也可手动运行:
  powershell -NoProfile -ExecutionPolicy Bypass -File tools\git-autosave.ps1
  powershell -NoProfile -ExecutionPolicy Bypass -File tools\git-autosave.ps1 -Once
#>
param(
    [int]$IntervalSec = 20,
    [switch]$Once
)

$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

while ($true)
{
    $dirty = git status --porcelain 2>$null
    if ($dirty)
    {
        git add -A 2>$null
        $stamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        git commit -m "自动保存 $stamp" 2>$null | Out-Null
        if ($LASTEXITCODE -eq 0)
        {
            Write-Host "[autosave] $stamp"
        }
    }

    if ($Once)
    {
        break
    }
    Start-Sleep -Seconds $IntervalSec
}
