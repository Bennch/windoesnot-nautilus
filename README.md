# WinDoesNot Files (Nautilus 51.1 with a This PC view)

A small, honest fork of [GNOME Files (Nautilus)](https://gitlab.gnome.org/GNOME/nautilus) 51.1 for
[WinDoesNot](https://windoesnot.me). It adds a **This PC** page like the one in Windows Explorer and a few sidebar
improvements people have been asking for. Everything else is upstream Nautilus, untouched.
Upstream's own readme is kept as [README.upstream.md](README.upstream.md).

*Deutsch: siehe weiter unten.*

## What is different

| Change | What you get |
|---|---|
| **This PC** in the sidebar | A built-in place directly below *Home* that opens `computer:///`. |
| **Drives view** at `computer:///` | Collapsible categories: **Devices and Drives** (system and local disks), **External Drives** (USB sticks, removable disks, DVD/CD, floppy) and **Network Addresses**. Every drive is a tile with an icon, a **fill bar** (it follows the accent colour and turns red above 90 %) and "233 GB free of 1.8 TB". Removable drives have an eject button. |
| **Icon size** | `Ctrl` + scroll or `Ctrl` + `+`/`-` in the drives view (small, medium, large; remembered in `computer-view-zoom-level`). |
| **Windows-like order** | System disk, local disks, removable disks, optical drives, network. |
| **Ctrl + click** in the sidebar | Opens the place in a **new tab**. |
| **Hide built-in places** | The GSettings key `org.gnome.nautilus.preferences sidebar-hidden-places` hides `recent`, `starred`, `network`, `trash` and `this-pc` from the sidebar. |

The fill values come from a small Nautilus extension in [`windoesnot/windoesnot-computer.py`](windoesnot/windoesnot-computer.py)
(needs `nautilus-python`). Without it the drives view still works, it just shows names and icons without bars.
The extension sets these attributes on the items of `computer:///`: `wdn_order` (sort key and category),
`wdn_percent` and `wdn_space` (fill bar and free-space text), `wdn_kind` and `wdn_usage` (list columns).

## Build and install

Arch Linux / CachyOS (replaces the `nautilus` package, the extension library stays with `libnautilus-extension`):

```sh
cd windoesnot
makepkg -si
```

Anywhere else:

```sh
meson setup build -Dtests=none -Ddocs=false -Dselinux=false
meson compile -C build
```

Then copy `windoesnot/windoesnot-computer.py` to `~/.local/share/nautilus-python/extensions/` and restart Files (`nautilus -q`).

## How the fork is kept

The changes are a handful of commits on top of the upstream tag `51.1` (see `git log 51.1..`). When GNOME releases a new
Nautilus, the commits are rebased onto the new tag, built and tested again. The fork never changes upstream behaviour
outside the points listed above. If a change is useful for everyone, it is better sent upstream; the sidebar and
view patches are small enough to be proposed there.

## License and credits

Nautilus is free software under the **GPL-3.0-or-later**; this fork keeps that license and every upstream copyright
notice. All credit for Files goes to the GNOME Nautilus maintainers and contributors. The WinDoesNot changes are by
the WinDoesNot project. The new view is modelled on Nautilus' own network view (`nautilus-network-view.c`).

---

# WinDoesNot Files (Nautilus 51.1 mit „Dieser PC“)

Ein kleiner, ehrlicher Fork von [GNOME Dateien (Nautilus)](https://gitlab.gnome.org/GNOME/nautilus) 51.1 für
[WinDoesNot](https://windoesnot.me). Er bringt eine **„Dieser PC“-Seite** wie im Windows-Explorer und ein paar
Verbesserungen der Seitenleiste, die sich Nutzer schon lange wünschen. Alles andere ist unverändertes Nautilus.

## Was anders ist

- **„Dieser PC“** steht in der Seitenleiste direkt unter „Persönlicher Ordner“ und öffnet `computer:///`.
- **Laufwerksansicht** mit einklappbaren Kategorien: **Geräte und Laufwerke** (System und lokale Platten),
  **Externe Laufwerke** (USB-Sticks, Wechseldatenträger, DVD/CD, Diskette) und **Netzwerkadressen**. Jedes Laufwerk ist
  eine Kachel mit Symbol, **Füllbalken** (in der Akzentfarbe, rot ab 90 %) und „233 GB frei von 1,8 TB“,
  Wechselmedien mit Auswerfen-Knopf.
- **Symbolgröße** mit Strg + Scrollen oder Strg + Plus/Minus (klein, mittel, groß; wird gespeichert).
- **Reihenfolge wie bei Windows:** Systemplatte, lokale Platten, Wechseldatenträger, optische Laufwerke, Netzwerk.
- **Strg + Klick** in der Seitenleiste öffnet den Ort in einem **neuen Tab**.
- **Eingebaute Orte ausblenden** (Zuletzt, Favoriten, Netzwerk, Papierkorb, Dieser PC) über den GSettings-Schlüssel
  `org.gnome.nautilus.preferences sidebar-hidden-places`.

Die Füllwerte liefert die kleine Erweiterung `windoesnot/windoesnot-computer.py` (braucht `nautilus-python`). Ohne sie
funktioniert die Ansicht weiter, zeigt aber nur Namen und Symbole.

## Bauen und installieren

Arch Linux / CachyOS: im Ordner `windoesnot` `makepkg -si` (ersetzt das Paket `nautilus`). Sonst mit Meson bauen
(siehe oben) und die Erweiterung nach `~/.local/share/nautilus-python/extensions/` kopieren.

## Wartung und Lizenz

Die Änderungen liegen als wenige Commits auf dem Upstream-Tag `51.1` und werden bei jeder neuen Nautilus-Version neu
daraufgesetzt, gebaut und getestet. Lizenz **GPL-3.0-or-later** wie das Original, alle Urheberrechtshinweise bleiben.
