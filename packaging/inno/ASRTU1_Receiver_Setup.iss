#define AppName "ASRTU Series Satellite Receiver and Upload"
#ifndef AppVersion
#define AppVersion "1.5.4"
#endif
#ifndef AppBuildSuffix
#define AppBuildSuffix ""
#endif


[Setup]
AppId={{957BAE5B-4E42-4ACB-932D-9759FB28DD44}
AppName={cm:ProgramGroup}
AppVersion={#AppVersion}
AppPublisher=BG7ZDQ
VersionInfoCompany=BG7ZDQ
VersionInfoCopyright=By BG7ZDQ
VersionInfoDescription=ASRTU Series satellite reception, decoding, recording and telemetry upload suite
VersionInfoProductName=ASRTU Series Satellite Receiver and Upload
DefaultDirName={localappdata}\Programs\ASRTU_Series_Receiver
DefaultGroupName={cm:ProgramGroup}
UninstallDisplayName={cm:ProgramGroup}
UninstallDisplayIcon={app}\launcher\ASRTU1_Launcher.exe
SetupIconFile=..\..\assets\branding\astro_series_launcher.ico
OutputDir=dist
OutputBaseFilename=ASRTU_Series_Receiver-{#AppVersion}-WinInstaller(Recommend){#AppBuildSuffix}
Compression=lzma2/ultra64
SolidCompression=yes
MergeDuplicateFiles=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
DisableProgramGroupPage=yes
ShowLanguageDialog=yes
UsePreviousLanguage=no
LanguageDetectionMethod=none

[Languages]
Name: "english"; MessagesFile: "English.isl"; InfoBeforeFile: "THIRD_PARTY_NOTICE.en.txt"
Name: "chinesesimp"; MessagesFile: "Languages\ChineseSimplified.isl"; InfoBeforeFile: "THIRD_PARTY_NOTICE.txt"
Name: "japanese"; MessagesFile: "Languages\Japanese.isl"; InfoBeforeFile: "THIRD_PARTY_NOTICE.ja.txt"

[CustomMessages]
english.UiLanguage=en
chinesesimp.UiLanguage=zh
japanese.UiLanguage=ja
english.RemoveUserDataPrompt=Delete all ASRTU user data?%n%nThis includes the callsign, coordinates, TinyDoppler satellites/frequencies/ephemeris, recordings/logs and launcher/decoder settings.%n%nChoose No to keep them for a later reinstall.
chinesesimp.RemoveUserDataPrompt=是否删除所有 ASRTU 用户数据？%n%n包括呼号、经纬度、TinyDoppler 卫星/频率/星历、录音日志以及启动器/解码器设置。%n%n选择“否”将保留这些数据，重新安装后仍可使用。
japanese.RemoveUserDataPrompt=ASRTU のユーザーデータをすべて削除しますか？%n%nコールサイン、緯度経度、TinyDoppler の衛星/周波数/星暦、録音・ログ、ランチャー/デコーダー設定が含まれます。%n%n「いいえ」を選ぶと再インストール用に保持されます。
english.ProgramGroup=ASRTU Series Satellite Receiver and Upload
chinesesimp.ProgramGroup=阿斯图系列卫星接收与上传
japanese.ProgramGroup=ASRTUシリーズ衛星受信・アップロード
english.FullInstall=Full installation (including SDR# telemetry preset)
chinesesimp.FullInstall=完整安装（含 SDR# 遥测预设版）
japanese.FullInstall=完全インストール（SDR#テレメトリプリセットを含む）
english.CompactInstall=ASRTU reception, decoding and upload only
chinesesimp.CompactInstall=仅安装 ASRTU 接收、解码与上传
japanese.CompactInstall=ASRTU受信・デコード・アップロードのみ
english.CustomInstall=Custom installation
chinesesimp.CustomInstall=自定义安装
japanese.CustomInstall=カスタムインストール
english.CoreComponent=ASRTU reception, decoding, recording and telemetry upload
chinesesimp.CoreComponent=ASRTU 接收、解码、录音与遥测上传
japanese.CoreComponent=ASRTU受信・デコード・録音・テレメトリアップロード
english.SdrComponent=SDR# Studio v1920 telemetry preset (with local RAW I/Q bridge; optional third-party software)
chinesesimp.SdrComponent=SDR# Studio v1920 遥测预设版（含本地 RAW I/Q 桥接，第三方软件，可选）
japanese.SdrComponent=SDR# Studio v1920テレメトリプリセット（ローカルRAW I/Qブリッジ付き、任意の第三者ソフトウェア）
english.LauncherShortcut=ASRTU Series Satellite Launcher
chinesesimp.LauncherShortcut=阿斯图系列卫星启动器
japanese.LauncherShortcut=ASRTUシリーズ衛星ランチャー
english.SdrShortcut=SDR# Telemetry Preset
chinesesimp.SdrShortcut=SDR# 遥测预设版
japanese.SdrShortcut=SDR#テレメトリプリセット
english.SdrppComponent=SDR++ v1.3.0 Nightly telemetry preset (shared-memory I/Q + Doppler; more SDR devices)
chinesesimp.SdrppComponent=SDR++ Ver1.3.0 Nightly 遥测预设版（含共享内存 I/Q 与多普勒插件，支持更多 SDR 设备）
japanese.SdrppComponent=SDR++ Ver1.3.0 Nightly テレメトリプリセット（共有メモリ I/Q・ドップラー、対応SDR機器を拡大）
english.SdrppShortcut=SDR++ ASRTU Receiver
chinesesimp.SdrppShortcut=SDR++ ASRTU 接收
japanese.SdrppShortcut=SDR++ ASRTU受信
english.InstallPlutoDriver=Install PlutoSDR Windows driver
chinesesimp.InstallPlutoDriver=安装 PlutoSDR Windows 驱动
japanese.InstallPlutoDriver=PlutoSDR Windowsドライバーをインストール
english.OpenLauncher=Open the ASRTU Series Satellite Launcher
chinesesimp.OpenLauncher=打开阿斯图系列卫星启动器
japanese.OpenLauncher=ASRTUシリーズ衛星ランチャーを開く
english.StationTitle=Ground Station Details
chinesesimp.StationTitle=地面站资料
japanese.StationTitle=地上局情報
english.StationSubtitle=Enter the information required by the upload proxy
chinesesimp.StationSubtitle=填写上传代理所需资料
japanese.StationSubtitle=アップロードプロキシに必要な情報を入力してください
english.StationDescription=The satellite and server are selected per session in the launcher. Enter only the ground-station details here.
chinesesimp.StationDescription=卫星和服务器在启动器中按每次接收任务选择；这里仅填写地面站资料。
japanese.StationDescription=衛星とサーバーはセッションごとにランチャーで選択します。ここでは地上局情報のみ入力してください。
english.Callsign=Callsign:
chinesesimp.Callsign=呼号：
japanese.Callsign=コールサイン：
english.Longitude=Longitude (-180 to 180):
chinesesimp.Longitude=经度（-180 至 180）：
japanese.Longitude=経度（-180～180）：
english.Latitude=Latitude (-90 to 90):
chinesesimp.Latitude=纬度（-90 至 90）：
japanese.Latitude=緯度（-90～90）：
english.Altitude=Altitude (metres):
chinesesimp.Altitude=海拔（米）：
japanese.Altitude=標高（メートル）：
english.EnterCallsign=Enter a callsign.
chinesesimp.EnterCallsign=请填写呼号。
japanese.EnterCallsign=コールサインを入力してください。
english.InvalidCallsign=The callsign cannot contain a double quote or semicolon.
chinesesimp.InvalidCallsign=呼号不能包含双引号或分号。
japanese.InvalidCallsign=コールサインに二重引用符またはセミコロンは使用できません。
english.InvalidLongitude=Longitude must be a number from -180 to 180.
chinesesimp.InvalidLongitude=经度必须是 -180 至 180 之间的数字。
japanese.InvalidLongitude=経度は-180～180の数値で入力してください。
english.InvalidLatitude=Latitude must be a number from -90 to 90.
chinesesimp.InvalidLatitude=纬度必须是 -90 至 90 之间的数字。
japanese.InvalidLatitude=緯度は-90～90の数値で入力してください。
english.InvalidAltitude=Altitude must be a number from -500 to 10000 metres.
chinesesimp.InvalidAltitude=海拔必须是 -500 至 10000 米之间的数字。
japanese.InvalidAltitude=標高は-500～10000メートルの数値で入力してください。
english.ConfigWriteFailed=Unable to write the upload proxy configuration file.
chinesesimp.ConfigWriteFailed=无法写入上传代理配置文件。
japanese.ConfigWriteFailed=アップロードプロキシの設定ファイルを書き込めません。

[Types]
Name: "full"; Description: "{cm:FullInstall}"
Name: "compact"; Description: "{cm:CompactInstall}"
Name: "custom"; Description: "{cm:CustomInstall}"; Flags: iscustom

[Components]
Name: "core"; Description: "{cm:CoreComponent}"; Types: full compact custom; Flags: fixed
Name: "sdrsharp"; Description: "{cm:SdrComponent}"; Types: full
#ifdef SdrppAvailable
Name: "sdrpp"; Description: "{cm:SdrppComponent}"; Types: full
#endif

[Files]
#include "stage\core-files.iss"
Source: "stage\proxy\*"; DestDir: "{app}\proxy"; Excludes: "config.cfg"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "stage\sdrsharp\*"; DestDir: "{app}\sdrsharp"; Excludes: "SDRSharp.config,SDRSharp.Layout.xml,Frequencies.xml,BandPlan.xml,notches.xml"; Components: sdrsharp; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "stage\sdrsharp\SDRSharp.config"; DestDir: "{app}\sdrsharp"; Components: sdrsharp; Flags: onlyifdoesntexist
Source: "stage\sdrsharp\SDRSharp.Layout.xml"; DestDir: "{app}\sdrsharp"; Components: sdrsharp; Flags: onlyifdoesntexist
Source: "stage\sdrsharp\Frequencies.xml"; DestDir: "{app}\sdrsharp"; Components: sdrsharp; Flags: onlyifdoesntexist
Source: "stage\sdrsharp\BandPlan.xml"; DestDir: "{app}\sdrsharp"; Components: sdrsharp; Flags: onlyifdoesntexist
Source: "stage\sdrsharp\notches.xml"; DestDir: "{app}\sdrsharp"; Components: sdrsharp; Flags: onlyifdoesntexist
#ifdef SdrppAvailable
Source: "stage\sdrpp\*"; DestDir: "{app}\sdrpp"; Excludes: "*.json"; Components: sdrpp; Flags: ignoreversion recursesubdirs createallsubdirs
; Resource JSON is application data, not user settings. The exclusion above
; applies recursively, so explicitly include the complete resource tree.
Source: "stage\sdrpp\res\*"; DestDir: "{app}\sdrpp\res"; Components: sdrpp; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "stage\sdrpp\*.json"; DestDir: "{app}\sdrpp"; Components: sdrpp; Flags: ignoreversion onlyifdoesntexist
#endif
#ifdef SdrppAvailable
#ifdef PlutoAvailable
Source: "stage\plutosdr-driver\*"; DestDir: "{app}\drivers\plutosdr"; Components: sdrpp; Flags: ignoreversion recursesubdirs createallsubdirs
#endif
#endif
Source: "..\..\assets\icons\sdrsharp.ico"; DestDir: "{app}\launcher"; Components: sdrsharp; Flags: ignoreversion
Source: "README.txt"; DestDir: "{app}"; Components: core; Flags: ignoreversion
Source: "SDRSHARP_NOTICE.txt"; DestDir: "{app}"; Languages: chinesesimp; Flags: ignoreversion
Source: "SDRSHARP_NOTICE.en.txt"; DestDir: "{app}"; DestName: "SDRSHARP_NOTICE.txt"; Languages: english; Flags: ignoreversion
Source: "SDRSHARP_NOTICE.ja.txt"; DestDir: "{app}"; DestName: "SDRSHARP_NOTICE.txt"; Languages: japanese; Flags: ignoreversion
Source: "THIRD_PARTY_NOTICE.txt"; DestDir: "{app}"; Languages: chinesesimp; Flags: ignoreversion
Source: "THIRD_PARTY_NOTICE.en.txt"; DestDir: "{app}"; DestName: "THIRD_PARTY_NOTICE.txt"; Languages: english; Flags: ignoreversion
Source: "THIRD_PARTY_NOTICE.ja.txt"; DestDir: "{app}"; DestName: "THIRD_PARTY_NOTICE.txt"; Languages: japanese; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{cm:ProgramGroup}\{cm:LauncherShortcut}"; Filename: "{app}\launcher\ASRTU1_Launcher.exe"; WorkingDir: "{app}\launcher"
Name: "{autodesktop}\{cm:LauncherShortcut}"; Filename: "{app}\launcher\ASRTU1_Launcher.exe"; WorkingDir: "{app}\launcher"
Name: "{autoprograms}\{cm:ProgramGroup}\{cm:SdrShortcut}"; Filename: "{app}\launcher\ASRTU1_Launcher.exe"; Parameters: "--sdrsharp"; WorkingDir: "{app}\launcher"; IconFilename: "{app}\launcher\sdrsharp.ico"; Components: sdrsharp
#ifdef SdrppAvailable
Name: "{autoprograms}\{cm:ProgramGroup}\{cm:SdrppShortcut}"; Filename: "{app}\launcher\ASRTU1_Launcher.exe"; Parameters: "--sdrpp"; WorkingDir: "{app}\launcher"; IconFilename: "{app}\sdrpp\res\icons\sdrpp.ico"; Components: sdrpp
#endif
#ifdef SdrppAvailable
#ifdef PlutoAvailable
Name: "{autoprograms}\{cm:ProgramGroup}\{cm:InstallPlutoDriver}"; Filename: "{app}\drivers\plutosdr\install-driver.cmd"; WorkingDir: "{app}\drivers\plutosdr"; IconFilename: "{app}\drivers\plutosdr\dpinst.exe"; Components: sdrpp
#endif
#endif

[INI]
Filename: "{app}\launcher\ui-language.ini"; Section: "UI"; Key: "Language"; String: "{cm:UiLanguage}"; Flags: uninsdeleteentry uninsdeletesectionifempty
Filename: "{app}\decoder\ui-language.ini"; Section: "UI"; Key: "Language"; String: "{cm:UiLanguage}"; Flags: uninsdeleteentry uninsdeletesectionifempty
Filename: "{app}\doppler\ui-language.ini"; Section: "UI"; Key: "Language"; String: "{cm:UiLanguage}"; Flags: uninsdeleteentry uninsdeletesectionifempty
Filename: "{app}\satnogs-uploader\ui-language.ini"; Section: "UI"; Key: "Language"; String: "{cm:UiLanguage}"; Flags: uninsdeleteentry uninsdeletesectionifempty

[Run]
Filename: "{app}\launcher\ASRTU1_Launcher.exe"; Parameters: "--language={cm:UiLanguage}"; Description: "{cm:OpenLauncher}"; Flags: nowait postinstall skipifsilent

[InstallDelete]
#include "stage\core-stale.iss"
Type: files; Name: "{app}\decoder\ASRTU1_Launcher.exe"
Type: files; Name: "{app}\decoder\TinyDoppler.exe"
Type: files; Name: "{app}\decoder\ASRTU_SatnogsUploader.exe"
Type: files; Name: "{app}\sdrsharp\VCRUNTIME140_1.dll"

[UninstallDelete]
Type: filesandordirs; Name: "{app}\gnuradio-cache"
Type: dirifempty; Name: "{app}\proxy"
Type: dirifempty; Name: "{app}\decoder"
Type: dirifempty; Name: "{app}\launcher"
Type: dirifempty; Name: "{app}\doppler"
Type: dirifempty; Name: "{app}\sdrpp"
Type: dirifempty; Name: "{app}\satnogs-uploader"
Type: dirifempty; Name: "{app}"

[Code]
var
  StationPage: TInputQueryWizardPage;
  StationValuesLoaded: Boolean;
  ParamNickname, ParamLongitude, ParamLatitude, ParamAltitude: String;

function CreateHardLink(lpFileName, lpExistingFileName: String;
  lpSecurityAttributes: Integer): Boolean;
  external 'CreateHardLinkW@kernel32.dll stdcall';

function CollapseHardLink(const Destination, Source: String): Boolean;
var
  Temporary: String;
begin
  Result := False;
  if (not FileExists(Destination)) or (not FileExists(Source)) then
    Exit;
  Temporary := Destination + '.asrtu-hardlink';
  DeleteFile(Temporary);
  if not CreateHardLink(Temporary, Source, 0) then
    Exit;
  if not DeleteFile(Destination) then
  begin
    DeleteFile(Temporary);
    Exit;
  end;
  if not RenameFile(Temporary, Destination) then
  begin
    DeleteFile(Temporary);
    Exit;
  end;
  Result := True;
end;

procedure CollapseDuplicateFiles;
begin
#include "stage\core-hardlinks.iss"
end;

function ConfigStringValue(const Text, Key, Fallback: String): String;
var
  Tail: String;
  P, Q: Integer;
begin
  Result := Fallback;
  P := Pos(Key, Text);
  if P = 0 then
    Exit;
  Tail := Copy(Text, P + Length(Key), MaxInt);
  P := Pos('"', Tail);
  if P = 0 then
    Exit;
  Delete(Tail, 1, P);
  Q := Pos('"', Tail);
  if Q > 0 then
    Result := Copy(Tail, 1, Q - 1);
end;

function ConfigNumberValue(const Text, Key, Fallback: String): String;
var
  Tail: String;
  P, Q: Integer;
begin
  Result := Fallback;
  P := Pos(Key, Text);
  if P = 0 then
    Exit;
  Tail := Copy(Text, P + Length(Key), MaxInt);
  P := Pos('=', Tail);
  if P = 0 then
    Exit;
  Delete(Tail, 1, P);
  Q := Pos(';', Tail);
  if Q > 0 then
    Result := Trim(Copy(Tail, 1, Q - 1));
end;

procedure LoadPreviousStationValues;
var
  ConfigText, ConfigFile: String;
  ConfigRaw: AnsiString;
begin
  if StationValuesLoaded then
    Exit;
  StationValuesLoaded := True;

  ConfigFile := ExpandConstant('{app}\proxy\config.cfg');
  if LoadStringFromFile(ConfigFile, ConfigRaw) then
  begin
    ConfigText := String(ConfigRaw);
    StationPage.Values[0] := ConfigStringValue(
      ConfigText, 'proxy_nickname', StationPage.Values[0]);
    StationPage.Values[1] := ConfigNumberValue(
      ConfigText, 'proxy_long', StationPage.Values[1]);
    StationPage.Values[2] := ConfigNumberValue(
      ConfigText, 'proxy_lat', StationPage.Values[2]);
    StationPage.Values[3] := ConfigNumberValue(
      ConfigText, 'proxy_alt', StationPage.Values[3]);
  end;

  { Explicit unattended-install parameters override saved values. }
  if ParamNickname <> '' then StationPage.Values[0] := ParamNickname;
  if ParamLongitude <> '' then StationPage.Values[1] := ParamLongitude;
  if ParamLatitude <> '' then StationPage.Values[2] := ParamLatitude;
  if ParamAltitude <> '' then StationPage.Values[3] := ParamAltitude;
end;

function ParseNumber(ValueText: String; var Value: Extended): Boolean;
var
  Alternate: String;
begin
  Result := False;
  try
    Value := StrToFloat(Trim(ValueText));
    Result := True;
  except
    Alternate := Trim(ValueText);
    StringChangeEx(Alternate, '.', ',', True);
    try
      Value := StrToFloat(Alternate);
      Result := True;
    except
      Result := False;
    end;
  end;
end;

function ConfigNumber(ValueText: String): String;
begin
  Result := Trim(ValueText);
  StringChangeEx(Result, ',', '.', True);
end;

procedure InitializeWizard;
begin
  StationValuesLoaded := False;
  ParamNickname := ExpandConstant('{param:NICKNAME|}');
  ParamLongitude := ExpandConstant('{param:LONGITUDE|}');
  ParamLatitude := ExpandConstant('{param:LATITUDE|}');
  ParamAltitude := ExpandConstant('{param:ALTITUDE|}');
  StationPage := CreateInputQueryPage(wpSelectDir,
    CustomMessage('StationTitle'), CustomMessage('StationSubtitle'),
    CustomMessage('StationDescription'));
  StationPage.Add(CustomMessage('Callsign'), False);
  StationPage.Add(CustomMessage('Longitude'), False);
  StationPage.Add(CustomMessage('Latitude'), False);
  StationPage.Add(CustomMessage('Altitude'), False);
  StationPage.Values[0] := ParamNickname;
  StationPage.Values[1] := '0.000000';
  StationPage.Values[2] := '0.000000';
  StationPage.Values[3] := '0.00';
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = StationPage.ID then
    LoadPreviousStationValues;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  LongitudeValue, LatitudeValue, AltitudeValue: Extended;
begin
  Result := True;
  if CurPageID <> StationPage.ID then
    Exit;

  if Trim(StationPage.Values[0]) = '' then
  begin
    MsgBox(CustomMessage('EnterCallsign'), mbError, MB_OK);
    Result := False;
    Exit;
  end;
  if (Pos('"', StationPage.Values[0]) > 0) or
     (Pos(';', StationPage.Values[0]) > 0) then
  begin
    MsgBox(CustomMessage('InvalidCallsign'), mbError, MB_OK);
    Result := False;
    Exit;
  end;
  if (not ParseNumber(StationPage.Values[1], LongitudeValue)) or
     (LongitudeValue < -180) or (LongitudeValue > 180) then
  begin
    MsgBox(CustomMessage('InvalidLongitude'), mbError, MB_OK);
    Result := False;
    Exit;
  end;
  if (not ParseNumber(StationPage.Values[2], LatitudeValue)) or
     (LatitudeValue < -90) or (LatitudeValue > 90) then
  begin
    MsgBox(CustomMessage('InvalidLatitude'), mbError, MB_OK);
    Result := False;
    Exit;
  end;
  if (not ParseNumber(StationPage.Values[3], AltitudeValue)) or
     (AltitudeValue < -500) or (AltitudeValue > 10000) then
  begin
    MsgBox(CustomMessage('InvalidAltitude'), mbError, MB_OK);
    Result := False;
  end;
end;

procedure SetSdrppAmplitudeRange;
var
  Lines: TArrayOfString;
  I: Integer;
  ConfigPath: String;
begin
  ConfigPath := ExpandConstant('{app}\sdrpp\config.json');
  if not FileExists(ConfigPath) then
    Exit;
  if not LoadStringsFromFile(ConfigPath, Lines) then
    Exit;
  for I := 0 to GetArrayLength(Lines) - 1 do
  begin
    { Only the top-level amplitude pair is indented by exactly four spaces.
      Nested module settings can also contain min/max keys and must be kept. }
    if Pos('    "min"', Lines[I]) = 1 then
    begin
      if Pos(',', Lines[I]) > 0 then
        Lines[I] := '    "min": -150.0,'
      else
        Lines[I] := '    "min": -150.0';
    end
    else if Pos('    "max"', Lines[I]) = 1 then
    begin
      if Pos(',', Lines[I]) > 0 then
        Lines[I] := '    "max": -65.0,'
      else
        Lines[I] := '    "max": -65.0';
    end;
  end;
  SaveStringsToFile(ConfigPath, Lines, False);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ConfigText: String;
  LegacySdrpp, NewSdrpp, Destination: String;
  ConfigFile: TFindRec;
begin
#ifdef SdrppAvailable
  if (CurStep = ssInstall) and WizardIsComponentSelected('sdrpp') then
  begin
    { Preserve existing presets when migrating from the old separate folder.
      Copy only top-level settings; resources always come from this release. }
    LegacySdrpp := ExpandConstant('{localappdata}\ASRTU Series Receiver\sdrpp');
    NewSdrpp := ExpandConstant('{app}\sdrpp');
    ForceDirectories(NewSdrpp);
    if FindFirst(LegacySdrpp + '\*.json', ConfigFile) then
    begin
      try
        repeat
          if (ConfigFile.Attributes and FILE_ATTRIBUTE_DIRECTORY) = 0 then
          begin
            Destination := NewSdrpp + '\' + ConfigFile.Name;
            if not FileExists(Destination) then
              if not FileCopy(LegacySdrpp + '\' + ConfigFile.Name, Destination, True) then
                RaiseException('Unable to preserve SDR++ settings: ' + ConfigFile.Name);
          end;
        until not FindNext(ConfigFile);
      finally
        FindClose(ConfigFile);
      end;
    end;
  end;
#endif
  if CurStep <> ssPostInstall then
    Exit;

  { Keep module-local DLL paths while sharing identical runtime files on disk. }
  CollapseDuplicateFiles;

#ifdef SdrppAvailable
  if WizardIsComponentSelected('sdrpp') then
    SetSdrppAmplitudeRange;
#endif

  { The launcher owns the runtime satellite selection. ASRTU-1 is only the
    initial profile shown on first launch. In a silent upgrade the custom
    station page is never shown, so never overwrite an existing config with
    the page's zero defaults. }
  if not (WizardSilent and
          FileExists(ExpandConstant('{app}\proxy\config.cfg'))) then
  begin
    ConfigText :=
      '# Server Config' + #13#10 +
      'zmq_address = "tcp://127.0.0.1:5555";' + #13#10 +
      'ws_address = "ws://1.92.100.130";' + #13#10 +
      'ws_port = 9000;' + #13#10#13#10 +
      '# Satellite Config' + #13#10 +
      'sat_name = "ASRTU-1";' + #13#10#13#10 +
      '# Proxy Config' + #13#10 +
      'physical_channel = 0;' + #13#10 +
      'proxy_nickname = "' + Trim(StationPage.Values[0]) + '";' + #13#10 +
      'proxy_long = ' + ConfigNumber(StationPage.Values[1]) + ';' + #13#10 +
      'proxy_lat = ' + ConfigNumber(StationPage.Values[2]) + ';' + #13#10 +
      'proxy_alt = ' + ConfigNumber(StationPage.Values[3]) + ';' + #13#10;

    ForceDirectories(ExpandConstant('{app}\proxy'));
    if not SaveStringToFile(ExpandConstant('{app}\proxy\config.cfg'),
                            ConfigText, False) then
      MsgBox(CustomMessage('ConfigWriteFailed'), mbError, MB_OK);
  end;
end;

var
  RemoveAllUserData: Boolean;

function AppUserDataExists: Boolean;
begin
  Result :=
    FileExists(ExpandConstant('{app}\proxy\config.cfg')) or
    FileExists(ExpandConstant('{app}\satellites.json')) or
    FileExists(ExpandConstant('{app}\tracker.ini')) or
    FileExists(ExpandConstant('{app}\standalone-tracker.ini')) or
    FileExists(ExpandConstant('{app}\ephemerides.json')) or
    DirExists(ExpandConstant('{app}\ASRTU1_Records')) or
    DirExists(ExpandConstant('{localappdata}\BG7ZDQ\TinyDoppler')) or
    DirExists(ExpandConstant('{userappdata}\BG7ZDQ\TinyDoppler')) or
    DirExists(ExpandConstant('{userappdata}\BG7ZDQ\TinyDopplerStandalone')) or
    DirExists(ExpandConstant('{userappdata}\ASRTU\ASRTU1_Launcher')) or
    DirExists(ExpandConstant('{userappdata}\ASRTU\ASRTU_Doppler')) or
    DirExists(ExpandConstant('{localappdata}\ASRTU\ASRTU1_Demod_CQt')) or
    RegKeyExists(HKCU, 'Software\ASRTU\AstroSeriesLauncher') or
    RegKeyExists(HKCU, 'Software\ASRTU\ASRTU1_Launcher') or
    RegKeyExists(HKCU, 'Software\ASRTU\ASRTU1_Demod_CQt_v3') or
    RegKeyExists(HKCU, 'Software\ASRTU\ASRTU1_Demod_CQt') or
    RegKeyExists(HKCU, 'Software\TinyDoppler') or
    RegKeyExists(HKCU, 'Software\ASRTU\AstroSeriesTracker');
end;

function InitializeUninstall(): Boolean;
begin
  RemoveAllUserData := False;
  if AppUserDataExists then
    RemoveAllUserData :=
      MsgBox(CustomMessage('RemoveUserDataPrompt'),
             mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES;
  Result := True;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usUninstall) and RemoveAllUserData then
  begin
    DeleteFile(ExpandConstant('{app}\proxy\config.cfg'));
    DeleteFile(ExpandConstant('{app}\satellites.json'));
    DeleteFile(ExpandConstant('{app}\tracker.ini'));
    DeleteFile(ExpandConstant('{app}\standalone-tracker.ini'));
    DeleteFile(ExpandConstant('{app}\ephemerides.json'));
    DelTree(ExpandConstant('{app}\ASRTU1_Records'), True, True, True);
    DelTree(ExpandConstant('{localappdata}\BG7ZDQ\TinyDoppler'),
            True, True, True);
    DelTree(ExpandConstant('{userappdata}\BG7ZDQ\TinyDoppler'),
            True, True, True);
    DelTree(ExpandConstant('{userappdata}\BG7ZDQ\TinyDopplerStandalone'),
            True, True, True);
    DelTree(ExpandConstant('{userappdata}\ASRTU\ASRTU1_Launcher'),
            True, True, True);
    DelTree(ExpandConstant('{userappdata}\ASRTU\ASRTU_Doppler'),
            True, True, True);
    DelTree(ExpandConstant('{localappdata}\ASRTU\ASRTU1_Demod_CQt'),
            True, True, True);
    RegDeleteKeyIncludingSubkeys(HKCU,
                                 'Software\ASRTU\AstroSeriesLauncher');
    RegDeleteKeyIncludingSubkeys(HKCU,
                                 'Software\ASRTU\ASRTU1_Launcher');
    RegDeleteKeyIncludingSubkeys(HKCU,
                                 'Software\ASRTU\ASRTU1_Demod_CQt_v3');
    RegDeleteKeyIncludingSubkeys(HKCU,
                                 'Software\ASRTU\ASRTU1_Demod_CQt');
    RegDeleteKeyIncludingSubkeys(HKCU, 'Software\TinyDoppler');
    RegDeleteKeyIncludingSubkeys(HKCU,
                                 'Software\ASRTU\AstroSeriesTracker');
  end;
end;
