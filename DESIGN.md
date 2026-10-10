# MoobFS documentation design

## Intent

An illustrated engineering field guide: large, direct explanations followed by
reproducible evidence. The physical reference is a circuit diagram on a clean
white workbench. The shared storage object, not generic AI imagery, carries identity.

## Color

Committed warm red for the shared object and important links, pure white background,
near-black ink and pale neutral panels. Palette seed hue 34.8, generated using the
Impeccable palette tool. CSS tokens in `site/style.css` use OKLCH. Dark blue is a
secondary accent for code/evidence. All ordinary text targets AA contrast.

## Typography

Local Verdana for the wide counters and clear small documentation text, Arial for
large diagram-like headings, monospace only for actual code/addresses. A font
catalog review considered Atkinson Hyperlegible Next; system fonts avoid downloads
and keep the entire site independent of external font services. No bundled fonts.

## Layout and components

Max width 1180px; generous asymmetric hero, actual SVG storage diagram, bordered
status note, code panels, linked documentation rows. Narrow reading measure in the
guide. Header wraps on small screens; hero and two-column sections stack. Visible
focus, skip link, semantic landmarks, SVG accessible title/description. No motion,
JavaScript, analytics, cookie banners or external UI dependencies.
