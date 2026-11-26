; ============================================================================
; Instalador de EFRT (FSMAudio) - Inno Setup 6  (https://jrsoftware.org/isinfo.php)
;
; Compilar (desde la raíz del repo, con el build de Release ya hecho):
;     iscc installer\EFRT.iss
; o con otra versión:
;     iscc /DMyAppVersion=1.2.0 installer\EFRT.iss
;
; Genera:  dist\EFRT_Setup_<1.1.0>.exe
;
; Qué hace: muestra el EULA (hay que aceptarlo), instala el VST3 en la carpeta
; estándar de Windows y, opcionalmente, el Standalone con sus accesos directos.
; Incluye desinstalador.
;
; Las rutas son relativas a ESTE archivo (por eso empiezan con ..\).
; ============================================================================

#ifndef MyAppVersion
  #define MyAppVersion "1.1.0"
#endif
#define MyAppName      "EFRT"
#define MyAppPublisher "FSMAudio"
#define MyAppExeName   "EFRT.exe"
#define BuildDir       "..\build\ElFratacho_artefacts\Release"

[Setup]
AppId={{69CB5375-2F2C-43FE-A1AA-4C6592886E0D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppCopyright=© 2026 {#MyAppPublisher}. Todos los derechos reservados.
VersionInfoVersion={#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
DisableDirPage=auto
OutputDir=..\dist
OutputBaseFilename=EFRT_Setup_{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile=..\EULA.txt
UninstallDisplayName={#MyAppName} {#MyAppVersion}

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Components]
Name: "vst3";       Description: "Plugin VST3 (para tu DAW)"; Types: full compact custom; Flags: fixed
Name: "standalone"; Description: "Aplicación independiente (sin DAW)"; Types: full

[Tasks]
Name: "desktopicon"; Description: "Crear un acceso directo en el escritorio"; Components: standalone; Flags: unchecked

[Files]
; VST3 completo (la carpeta .vst3 con su estructura interna)
Source: "{#BuildDir}\VST3\EFRT.vst3\*"; DestDir: "{commoncf64}\VST3\EFRT.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
; Standalone
Source: "{#BuildDir}\Standalone\EFRT.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
; Documentos legales
Source: "..\EULA.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Components: standalone
Name: "{autoprograms}\Desinstalar {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Abrir {#MyAppName} ahora"; Flags: nowait postinstall skipifsilent unchecked; Components: standalone
