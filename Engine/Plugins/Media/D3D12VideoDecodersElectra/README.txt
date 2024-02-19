This plugin uses GPU hardware accelerated video decoding on Windows via Direct3D 12 Video.
See: https://learn.microsoft.com/en-us/windows/win32/medfound/direct3d-12-video-overview

At present this plugin supports the following codecs, provided there is support by your GPU
(via vendor provided extensions) with some limitations:

H.264 / AVC
- Only Baseline, Main and High profiles are supported
- Constrained Baseline profile can be used (no FMO, ASO and RS)
- PAFF and MBAFF are not supported (interlaced video)


H.265 / HEVC
- Only Main and Main10 profiles up to level 6.3 are supported
