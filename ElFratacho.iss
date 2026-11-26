; ============================================================================
; Instalador de "EFRT" (compresor FET estilo 1176/CLA-76) — FSMAudio
; Generado con Inno Setup (gratis: https://jrsoftware.org/isinfo.php)
;
; Qué hace:
;   - Instala el VST3 en la carpeta estándar de Windows (con permisos de
;     administrador manejados prolijamente por el instalador, en vez de
;     que el usuario tenga que copiar archivos a mano).
;   - Instala el Standalone (.exe) en Program Files, con accesos directos
;     en el menú inicio y, opcionalmente, en el escritorio.
;   - Genera un desinstalador automático.
; ============================================================================

#define MyAppName "EFRT"
#define MyAppVersion "1.1.0"
#define MyAppPublisher "FSMAudio"
#define MyAppExeName "EFRT.exe"

[Setup]
AppId={{B4B6F1B0-6E7A-4C3E-9C1A-EL-FRATACHO-1176}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=Installer
OutputBaseFilename=ElFratacho_Setup
Compression=lzma
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
LicenseFile=EULA.txt

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear un acceso directo en el escritorio"; GroupDescription: "Accesos directos adicionales:"

; ----------------------------------------------------------------------------
; IMPORTANTE: estas rutas "Source" asumen que compilás este .iss estando
; parado en la carpeta raíz del proyecto (C:\AudioDev\ElFratacho\), con la
; carpeta build\ ya generada por CMake. Si tu carpeta build quedó en otro
; lado, ajustá las rutas de abajo antes de compilar.
; ----------------------------------------------------------------------------
[Files]
; El VST3 (carpeta completa, con su estructura interna Contents\...)
Source: "build\ElFratacho_artefacts\Release\VST3\EFRT.vst3\*"; DestDir: "{commoncf64}\VST3\EFRT.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

; El standalone, para usar sin DAW
Source: "build\ElFratacho_artefacts\Release\Standalone\EFRT.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Desinstalar {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Abrir {#MyAppName} ahora"; Flags: nowait postinstall skipifsilent
