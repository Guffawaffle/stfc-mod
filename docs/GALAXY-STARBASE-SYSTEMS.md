# Alliance Starbase system indicator

Enable **Alliance Starbase systems** in the mod's Galaxy settings, or set
`graphics.galaxy_starbase_systems = true`. The default is off.

A yellow orbital station marks known Plasma Storm systems at the same native detail/name
visibility used by the cyan Housing indicator. The two settings are independent;
systems that support both can show both icons. The marker describes a static
deployment system. It does not report vacant anchoring slots or current Alliance
Starbase occupancy.

The station glyph is embedded with the mod; its editable vector source is
`assets/icons/alliance-starbase.svg`. Housing keeps the native cyan house.

## Catalog

The embedded community reference contains 107 unique systems: 42 Collisional,
35 Magnetic, and 30 Active Plasma systems. Plasma membership comes from the
supplied community list. System IDs and current English names come from
stfc.space's public system summary and English system translations. Systems
omitted from the reference remain unknown and receive no marker.

All 104 unchanged names matched uniquely. The other three mappings were explicitly
confirmed by the contributor:

| Reference name | Public name | System ID |
| --- | --- | ---: |
| Lo'Uren | Lo'Uren Co | 350735951 |
| Pioioab | Piuioab | 703174915 |
| Tiwogua | Tiwoqua | 964152827 |

The input was `plasma_storm_systems.csv`, SHA-256
`7ba23811bd5b1ab2ca42fb8b0c1c69d7fe3ae59206172042246563e23301c4d4`.
The public catalog version was `83f49c58-ae2f-4659-9e15-fc072dc9b844`:

- [System summary](https://data.stfc.space/system/summary.json), SHA-256
  `2d81e1f25a1498287102e64c96b775f7106feb1950c3a48b6d9dae556ad4e7e0`.
- [English system names](https://data.stfc.space/translations/en/systems.json), SHA-256
  `ca6ee4138af6b52a72dae9825c93df29bf6996be242bb8b272cafd26de6b1306`.

`assets/data/starbase_systems.json` retains these source pins, the approved name
corrections, and sorted positive IDs for each Plasma kind. The parser supports
multiple kinds per system and gives no negative result for an unlisted ID.
Catalog updates require a new reconciled reference; the mod does not download
catalogs or expand membership from the player's explored-system cache.
