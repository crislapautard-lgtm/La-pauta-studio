; Script para el instalador de La Pauta Studio
; Requiere NSIS (Nullsoft Scriptable Install System)

!define APPNAME "La Pauta Studio"
!define COMPANYNAME "La Pauta"
!define DESCRIPTION "Un DAW moderno, ligero y eficiente inspirado en REAPER"
!define VERSIONMAJOR 1
!define VERSIONMINOR 0
!define VERSIONBUILD 0

Name "${APPNAME}"
OutFile "LaPautaStudio_Installer.exe"
InstallDir "$PROGRAMFILES64\${APPNAME}"

; Solicitar privilegios de Administrador para Windows
RequestExecutionLevel admin

!include "MUI2.nsh"

!define MUI_ABORTWARNING

; Páginas del instalador
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Páginas del desinstalador
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "Spanish"

Section "Instalación" SecInstall
    SetOutPath "$INSTDIR"
    
    ; Copiar el ejecutable compilado del DAW
    File "build\La-Pauta-Studio_artefacts\Release\VST3\La-Pauta-Studio.exe"
    
    ; Crear accesos directos
    CreateShortCut "$DESKTOP\La Pauta Studio.lnk" "$INSTDIR\La-Pauta-Studio.exe"
    CreateDirectory "$SMPROGRAMS\${APPNAME}"
    CreateShortCut "$SMPROGRAMS\${APPNAME}\La Pauta Studio.lnk" "$INSTDIR\La-Pauta-Studio.exe"
    CreateShortCut "$SMPROGRAMS\${APPNAME}\Desinstalar.lnk" "$INSTDIR\uninstall.exe"

    ; Crear desinstalador
    WriteUninstaller "$INSTDIR\uninstall.exe"
    
    ; Entradas en el registro para Agregar/Quitar programas en Windows
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Publisher" "${COMPANYNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayVersion" "${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "EstimatedSize" 5000
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\La-Pauta-Studio.exe"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"

    Delete "$DESKTOP\La Pauta Studio.lnk"
    Delete "$SMPROGRAMS\${APPNAME}\La Pauta Studio.lnk"
    Delete "$SMPROGRAMS\${APPNAME}\Desinstalar.lnk"
    RMDir "$SMPROGRAMS\${APPNAME}"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
SectionEnd
