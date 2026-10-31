# File-based settings

ASRTU application preferences use explicitly named INI files on every platform.
They do not read or write Windows Registry settings, macOS CFPreferences/plist
settings, or QSettings organization/system fallbacks. All new INI files use UTF-8.

## Launcher and decoder

`launcher.ini` contains the input/audio selection, station details, last satellite,
SDR paths and custom SDR profiles. `decoder.ini` contains window geometry and the
two spectrum visibility settings. These files are written when settings change.

The station latitude and longitude fields also accept a Maidenhead grid locator
(4, 6 or 8 characters, for example `OL63PD`). A locator is only an input and
display convenience: the decimals shown beside it are what gets stored, passed
to the tracker and proxies, and used for SGP4. Decoding a locator always applies
the centre of its cell, so switching between the two forms moves the station by
at most half a cell at the entered precision.

Their directory is:

| Platform | Directory |
| --- | --- |
| Windows installed/portable shared-bin layout | `<suite>/config/` |
| Windows flat development build | `<executable directory>/config/` |
| Linux | `$XDG_CONFIG_HOME/ASRTU/`, normally `~/.config/ASRTU/` |
| macOS | `QStandardPaths::GenericConfigLocation/ASRTU/`, normally `~/Library/Preferences/ASRTU/` (actual `.ini` files, not system preferences) |

An absolute `ASRTU_CONFIG_DIR` overrides this directory, including in child
processes. Relative overrides are ignored. There is no silent fallback to a
registry, plist or different directory if writing fails. Explicit station/profile
saves report errors; the common settings layer also logs INI read/write failures.

## Existing file formats

- Language remains in `ui-language.ini` beside the executable, under `[UI]`
  with `Language=zh`, `en` or `ja`. Command-line `--language` takes precedence;
  without a saved choice the OS language is used. Translation `.qm` resources
  are not configuration files.
- TinyDoppler already uses explicit INI files: `tracker.ini` and
  `standalone-tracker.ini`. Existing locations remain compatible: the suite root
  on Windows, and its Qt application configuration directory on Linux/macOS.
  `TINY_DOPPLER_CONFIG_DIR` continues to override its data directory.
- `satellites.json` and `ephemerides.json` remain structured data/cache files.
- `proxy/config.cfg` (or the platform proxy path) is the generated compatibility
  input required by the external MMT proxy. The launcher uses INI as its source
  of saved station preferences. On first launch, existing station fields can be
  imported from this file; later launches do not overwrite INI with stale proxy
  values. The Windows installer invokes `--import-proxy-config` after explicitly
  saving its station form, so the same UTF-8 INI writer handles both paths.
- Bundled third-party SDR applications retain their own required file formats.

## Upgrades and portability

Old registry/CFPreferences-only preferences are not migrated: accessing them
would violate the file-only policy. Re-select audio/SDR preferences and window
state once. Existing language and tracking INI files are retained; station
details can be recovered from the existing proxy file without reading native
settings. To move a Windows portable installation, copy the entire suite folder.

The Windows uninstaller's optional user-data removal now deletes the known INI
files; it does not inspect or delete old application preference registry keys.
Normal Windows installation/uninstallation registration is installer metadata,
not application preference storage, and is unchanged.
