# ============================================================================
# TFT_eSPI Configuration Swap Utility
# T-Deck PitchComm Project
# ============================================================================
# Usage:
#   .\swap_display_config.ps1 -Device TDeckPlus   # For Coach Unit
#   .\swap_display_config.ps1 -Device TWatchS3    # For Catcher Unit
#   .\swap_display_config.ps1 -Status             # Show current config
# ============================================================================

param(
    [Parameter(Position=0)]
    [ValidateSet("TDeckPlus", "TWatchS3")]
    [string]$Device,
    
    [switch]$Status
)

$LibPath = "D:\Arduino\libraries\TFT_eSPI"
$UserSetup = Join-Path $LibPath "User_Setup.h"
$TDeckConfig = Join-Path $LibPath "User_Setup_TDeckPlus.h"
$TWatchConfig = Join-Path $LibPath "User_Setup_TWatchS3.h"

function Get-CurrentConfig {
    if (Test-Path $UserSetup) {
        $content = Get-Content $UserSetup -Raw
        if ($content -match "320x240") {
            return "TDeckPlus"
        } elseif ($content -match "240x240") {
            return "TWatchS3"
        }
    }
    return "Unknown"
}

function Show-Status {
    $current = Get-CurrentConfig
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  TFT_eSPI Configuration Status" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Library Path:    $LibPath"
    Write-Host "  Current Config:  " -NoNewline
    
    switch ($current) {
        "TDeckPlus" {
            Write-Host "T-DECK PLUS (Coach Unit)" -ForegroundColor Green
            Write-Host "                   320x240 Landscape"
        }
        "TWatchS3" {
            Write-Host "T-WATCH S3 (Catcher Unit)" -ForegroundColor Yellow
            Write-Host "                   240x240 Portrait"
        }
        default {
            Write-Host "UNKNOWN" -ForegroundColor Red
        }
    }
    Write-Host ""
}

function Set-Config {
    param([string]$Target)
    
    $sourceFile = if ($Target -eq "TDeckPlus") { $TDeckConfig } else { $TWatchConfig }
    
    if (-not (Test-Path $sourceFile)) {
        Write-Host "ERROR: Configuration file not found: $sourceFile" -ForegroundColor Red
        return $false
    }
    
    try {
        Copy-Item -Path $sourceFile -Destination $UserSetup -Force
        Write-Host ""
        Write-Host "SUCCESS: Configuration switched to $Target" -ForegroundColor Green
        
        if ($Target -eq "TDeckPlus") {
            Write-Host "         Ready to compile Coach Unit firmware" -ForegroundColor Cyan
        } else {
            Write-Host "         Ready to compile Catcher Unit firmware" -ForegroundColor Cyan
        }
        Write-Host ""
        return $true
    }
    catch {
        Write-Host "ERROR: Failed to copy configuration: $_" -ForegroundColor Red
        return $false
    }
}

# Main execution
if ($Status -or (-not $Device)) {
    Show-Status
    if (-not $Device) {
        Write-Host "  Usage:" -ForegroundColor Gray
        Write-Host "    .\swap_display_config.ps1 -Device TDeckPlus" -ForegroundColor Gray
        Write-Host "    .\swap_display_config.ps1 -Device TWatchS3" -ForegroundColor Gray
        Write-Host ""
    }
} else {
    $current = Get-CurrentConfig
    if ($current -eq $Device) {
        Write-Host ""
        Write-Host "Configuration already set to $Device" -ForegroundColor Yellow
        Write-Host ""
    } else {
        Set-Config -Target $Device
    }
}
