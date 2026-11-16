# Fork launcher icons

Release, Pre, DEV and CI use the approved blue waffle badge. The unlabelled
artwork is preserved in `masterwaffle-badge.png`; the four `launcher-*.png` files
are the approved channel masters, including their lower channel labels. Their
palette stays cool: cyan for Release, icy blue for Pre, muted violet for DEV and
silver-blue for CI, with no orange or gold accents. The separate loading-screen
image at `assets/guffawaffle-build-badge.png` is unchanged.

CI already selects the matching committed icon automatically. Raw builds use CI;
the explicit development package uses DEV; promotion selects Pre or Release.
Neither the icon nor its label enables development features.

`scripts/generate-launcher-icons.py` packs the approved PNG masters into native
ICNS files. It does not redraw the artwork or labels. Regenerate the ICNS files
with Python and Pillow 12.3.0:

```sh
python -m pip install Pillow==12.3.0
python scripts/generate-launcher-icons.py
```

The PNG masters are square RGBA images of at least 1024 pixels. Each ICNS contains
native PNG representations at 16, 32, 64, 128, 256, 512 and 1024 pixels, plus the
16@2x, 32@2x, 128@2x and 256@2x Retina entries. The committed files are used directly
by builds; CI does not need Pillow or an icon generator.

An optional `--contact-sheet <path.png>` writes a preview at large, 64, 32 and 16
pixel sizes. Use `--source-dir <directory>` to select another set of approved
channel masters. `--output-dir <directory>` exports byte-identical PNG copies
and regenerated ICNS files for checking without modifying the committed assets.
When source and output directories match, the PNG masters are left untouched.
