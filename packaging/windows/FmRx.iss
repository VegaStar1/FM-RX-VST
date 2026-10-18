; Установщик FM RX для Windows (Inno Setup 6).
; Сборка:  ISCC.exe /DAppVersion=1.0.0 /DPluginDir="...\FM RX.vst3" /DDocsDir="...\docs\manual" /DOutDir="..." FmRx.iss
; Ставит VST3 в общую папку VST3 (C:\Program Files\Common Files\VST3) и PDF-инструкции в C:\Program Files\NUWMUN\FM RX.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef PluginDir
  #define PluginDir "..\..\build\FmRx_artefacts\Release\VST3\FM RX.vst3"
#endif
#ifndef DocsDir
  #define DocsDir "..\..\docs\manual"
#endif
#ifndef OutDir
  #define OutDir "..\..\dist"
#endif

[Setup]
; AppId не менять между версиями: по нему обновление ставится поверх старой версии.
AppId={{B221CCE3-4044-434F-AF18-E053F49F1AF8}
AppName=FM RX
AppVersion={#AppVersion}
AppVerName=FM RX {#AppVersion}
AppPublisher=NUWMUN
AppCopyright=© NUWMUN
VersionInfoVersion={#AppVersion}
VersionInfoProductName=FM RX
VersionInfoCompany=NUWMUN
VersionInfoDescription=FM RX VST3 installer
DefaultDirName={commonpf64}\NUWMUN\FM RX
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0
OutputDir={#OutDir}
OutputBaseFilename=FM-RX-{#AppVersion}-Windows-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ShowLanguageDialog=auto
UninstallDisplayName=FM RX {#AppVersion}
CloseApplications=yes

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[CustomMessages]
en.ReadyInfo=FM RX (VST3) will be installed to:%n  C:\Program Files\Common Files\VST3\FM RX.vst3%n%nThe PDF manuals go to:%n  C:\Program Files\NUWMUN\FM RX%n%nAfter installing, rescan plug-ins in your DAW.
ru.ReadyInfo=FM RX (VST3) будет установлен в:%n  C:\Program Files\Common Files\VST3\FM RX.vst3%n%nPDF-инструкции - в:%n  C:\Program Files\NUWMUN\FM RX%n%nПосле установки пересканируйте плагины в DAW.
en.OpenManual=Open the manual
ru.OpenManual=Открыть инструкцию
en.ManualLink=FM RX Manual
ru.ManualLink=Инструкция FM RX

[Files]
Source: "{#PluginDir}\*"; DestDir: "{commoncf64}\VST3\FM RX.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#DocsDir}\FM RX Manual EN.pdf"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DocsDir}\FM RX Manual RU.pdf"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\NUWMUN\{cm:ManualLink}"; Filename: "{app}\FM RX Manual EN.pdf"; Languages: en
Name: "{autoprograms}\NUWMUN\{cm:ManualLink}"; Filename: "{app}\FM RX Manual RU.pdf"; Languages: ru

[Run]
Filename: "{app}\FM RX Manual EN.pdf"; Description: "{cm:OpenManual}"; Flags: postinstall shellexec skipifsilent unchecked; Languages: en
Filename: "{app}\FM RX Manual RU.pdf"; Description: "{cm:OpenManual}"; Flags: postinstall shellexec skipifsilent unchecked; Languages: ru

[InstallDelete]
; Чистая установка поверх: старое содержимое бандла не смешивается с новым.
Type: filesandordirs; Name: "{commoncf64}\VST3\FM RX.vst3"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\FM RX.vst3"
Type: dirifempty; Name: "{app}"
Type: dirifempty; Name: "{commonpf64}\NUWMUN"

[Code]
procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpReady then
    WizardForm.ReadyMemo.Lines.Text := ExpandConstant('{cm:ReadyInfo}');
end;
