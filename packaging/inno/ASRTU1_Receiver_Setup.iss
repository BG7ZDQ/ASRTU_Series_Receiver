#define AppName "ASRTU Series Satellite Receiver and Upload"
#define AppVersion "1.5.3"

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
UninstallDisplayIcon={app}\decoder\ASRTU1_Launcher.exe
SetupIconFile=..\..\assets\branding\astro_series_launcher.ico
OutputDir=dist
OutputBaseFilename=ASRTU_Series_Receiver_Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
InfoBeforeFile=THIRD_PARTY_NOTICE.txt
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
DisableProgramGroupPage=yes
ShowLanguageDialog=yes
UsePreviousLanguage=no
LanguageDetectionMethod=none

[Languages]
Name: "english"; MessagesFile: "English.isl"
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[CustomMessages]
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

[Files]
Source: "stage\decoder\*"; DestDir: "{app}\decoder"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "stage\proxy\*"; DestDir: "{app}\proxy"; Excludes: "config.cfg"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "stage\sdrsharp\*"; DestDir: "{app}\sdrsharp"; Components: sdrsharp; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "README.txt"; DestDir: "{app}"; Components: core; Flags: ignoreversion
Source: "SDRSHARP_NOTICE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "THIRD_PARTY_NOTICE.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{cm:ProgramGroup}\{cm:LauncherShortcut}"; Filename: "{app}\decoder\ASRTU1_Launcher.exe"; WorkingDir: "{app}\decoder"
Name: "{autodesktop}\{cm:LauncherShortcut}"; Filename: "{app}\decoder\ASRTU1_Launcher.exe"; WorkingDir: "{app}\decoder"
Name: "{autoprograms}\{cm:ProgramGroup}\{cm:SdrShortcut}"; Filename: "{app}\decoder\ASRTU1_Launcher.exe"; Parameters: "--sdrsharp"; WorkingDir: "{app}\decoder"; Components: sdrsharp

[Run]
Filename: "{app}\decoder\ASRTU1_Launcher.exe"; Description: "{cm:OpenLauncher}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\proxy\config.cfg"
Type: dirifempty; Name: "{app}\proxy"
Type: dirifempty; Name: "{app}\decoder"
Type: dirifempty; Name: "{app}"

[Code]
var
  StationPage: TInputQueryWizardPage;
  StationValuesLoaded: Boolean;
  ParamNickname, ParamLongitude, ParamLatitude, ParamAltitude: String;

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

procedure CurStepChanged(CurStep: TSetupStep);
var
  ConfigText: String;
begin
  if CurStep <> ssPostInstall then
    Exit;

  { The launcher owns the runtime satellite selection. ASRTU-1 is only the
    initial profile shown on first launch. }
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
