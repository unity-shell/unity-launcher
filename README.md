## unity-launcher

library for the Unity launcher and dash for use in unity-shell.

It builds on [Astal](https://github.com/unity-shell/astal) and exposes two windows:

- **`UnityLauncher`**: the vertical panel. Shows pinned and running apps, tracks Wayland toplevels, and launches or focuses them on click.
- **`UnityDash`**: the full-screen overlay. Browses installed apps in a grid and filters them as you type.

The app list is modelled separately so the launcher and dash share from common source:

- **`UnityAppList`**: a `GListModel` of `UnityAppEntry` objects, seeded from pinned app ids and updated as toplevels come and go.
- **`UnityAppEntry`**: one application — its `GAppInfo`, its toplevels, and its pinned / running / activated state.

### build

install the deps:

- `gtk4` (>= 4.22)
- `libadwaita-1` (>= 1.8)
- `gio-2.0`
- `gio-unix-2.0`
- `graphene-1.0`
- `wayland-client`
- `astal-4-4.0`
- `astal-apps-0.1`
- `astal-wlr-0.1`
- `meson`

```sh
meson setup build
ninja -C build
sudo meson install -C build
```
