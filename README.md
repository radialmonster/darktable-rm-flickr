# darktable rm Flickr

A darktable Lua plugin for publishing and managing Flickr photos.

## Install

darktable 5.6 includes the Lua script manager, so no separate lua-scripts install is needed.

In darktable's `scripts` widget:

1. Set `action` to `install/update scripts`.
2. Under `add more scripts`, paste this into `URL to download additional scripts from`:

```text
https://github.com/radialmonster/darktable-rm-flickr
```

3. Enter this in `new folder to place scripts in`:

```text
dtrmflickr
```

4. Click `install additional scripts`.
5. Change back to `start/stop scripts`, select the `dtrmflickr` folder/category, and click the power button next to `dtrmflickr` to enable it.

After enabling it, restart darktable and choose `Flickr` as the target storage in Export.

The plugin's Lua preferences appear under `dtrmflickr` after the script is enabled. If the preferences window was already open, close and reopen it after enabling or restart darktable.

## Setup

In `Preferences` > `Lua options` > `dtrmflickr`, set up both the Flickr app credentials and your Flickr account login.

1. Request a Flickr API key and secret from Flickr:

```text
https://www.flickr.com/services/apps/create/apply/
```

Flickr's API documentation starts here:

```text
https://www.flickr.com/services/developer/api/
```

2. Enter the API key in `Flickr: API key` and the secret in `Flickr: API secret`.

These values identify the Flickr app used by the plugin. They are not your Flickr username or password, and they do not log in to your Flickr account by themselves.

3. In that same `dtrmflickr` Lua Options section, use `log in to Flickr...`. This opens Flickr in your browser so you can authorize this plugin to upload and update photos in your Flickr account. After authorization, paste the verifier code back into darktable and click `complete login`.

After login, the plugin saves the Flickr account token using darktable's password storage. On Windows this uses Windows Credential Manager. Use `log out` in Lua Options, or in the lighttable `Flickr` panel, to clear that saved account token.

The Export `Flickr` section controls the current export. The lighttable `Flickr` panel is for the selected published photo or photos, including refreshing Flickr settings, syncing title/description, claiming an existing Flickr photo, setting a link, or clearing the local link.

## What It Does

- Uploads rendered darktable exports to Flickr
- Saves the Flickr photo ID back to the darktable image with a `dtrmflickr` tag
- Supports Flickr privacy, safety, content type, license, commenting, and tag/notes/people permissions
- Sends title, description, keywords, date taken, and GPS when enabled
- Skips darktable keywords marked private when publishing Flickr tags
- Adds photos to an existing Flickr album or creates a new one
- Adds a lighttable `Flickr` panel for selected published photos
- Can sync title/description and selected Flickr settings without re-exporting
- Can claim/link existing Flickr photos using date taken plus filename/title/original filename clues, or clear the local link without deleting anything on Flickr
- Includes the Windows no-popup HTTP helper in the repository install; non-Windows systems ignore it

## Notes

This is an active development plugin. The source code, tests, build tooling, and
roadmap live in the development repository:

```text
https://github.com/radialmonster/darktable-rm-flickr-dev
```

`dtrmflickr.lua` is generated there from the split sources and published here.
The Windows HTTP helper `dtrmflickr_winhttp.dll` is built from `native/winhttp/`
by the GitHub Actions workflow in this repo.

## Manual Install

If you are installing by hand, download into a `dtrmflickr` folder in darktable's
Lua directory. On Windows, download **both** files (the `.dll` is the no-popup
HTTP helper; non-Windows systems don't need it):

```text
https://raw.githubusercontent.com/radialmonster/darktable-rm-flickr/main/dtrmflickr.lua
https://raw.githubusercontent.com/radialmonster/darktable-rm-flickr/main/dtrmflickr_winhttp.dll
```

Then add this to `luarc`:

```lua
require "dtrmflickr/dtrmflickr"
```
