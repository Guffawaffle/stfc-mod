# Trusted signing controller icons

This controller stamps the selected Pre or Release icon into the extracted Mac
app before signing. Its channel icons use the approved blue waffle badge from
`play-dev` commit `8ba8dd76281a6388a7272795c0436a702ce861dd` (DEV.4).
The PNG and ICNS files are exact copies from that source; the unlabelled artwork
is preserved in `masterwaffle-badge.png`.

Channel accents stay cool: cyan for Release, icy blue for Pre, muted violet for
DEV and silver-blue for CI. The source package verifies all native and Retina
ICNS representations. The controller consumes these committed files directly.

For a future artwork refresh, update the approved PNG masters and pack native
icons using `scripts/generate-launcher-icons.py` on the current `play-dev`
source, then copy the resulting icon assets into this controller. Older icon
generators predate this artwork and must not be used to redraw it.
