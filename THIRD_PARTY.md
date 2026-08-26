# Third-party components

The repository's MIT License covers only original ASRTU Series Receiver code owned by BG7ZDQ. Every dependency and bundled component remains under its upstream license. When an upstream notice conflicts with this summary, the upstream notice controls.

| Component | Upstream | License / distribution note |
| --- | --- | --- |
| GNU Radio 3.x | https://github.com/gnuradio/gnuradio | GPL-3.0-or-later; the receiver links against GNU Radio 3.10 libraries |
| gr-lilacsat | https://github.com/bg2bhc/gr-lilacsat | GPL-3.0-or-later in the 3.10-compatible source used by this project |
| gr-hyacinthsat | https://github.com/HyacinthSat/gr-hyacinthsat | GPL-3.0-or-later; local audio-source compatibility changes retain this license |
| Qt 5 | https://www.qt.io/ | Use under the license applicable to the selected Qt distribution, commonly LGPLv3/GPLv3 for open-source Qt builds |
| Qwt | https://qwt.sourceforge.io/ | Qwt License 1.0 |
| SGP4 C99 | vendored in `asrtu-qt/src/third_party/` | MIT; the complete upstream copyright and license text is retained in `SGP4_LICENSE.txt` |
| SDR# | https://airspy.com/download/ | Separate Airspy/SDR# terms; not covered by this repository's MIT License |
| SDR# plugin reference API | supplied with the applicable SDR# SDK | Reference license; SDK source is not included in this repository |
| Telemetry upload proxy | supplied separately by its owner | Follow the proxy's own upstream license; not relicensed by this project |
| Inno Setup English messages | https://github.com/jrsoftware/issrc | Inno Setup License; `asrtu-suite/English.isl` is pinned to the official 6.4.3 source so local compiler translations cannot alter the English installer |

The Windows installer also carries transitive runtime libraries from the selected radioconda/Qt/GNU Radio environment. A public binary release must retain their notices and satisfy source-offer or source-distribution obligations where required. Do not infer permission to redistribute SDR# or the telemetry proxy merely because the build script can package a locally supplied copy.
