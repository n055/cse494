# MagWindow: Improved Magnification and Contrast Software for Windows for Low Vision Users

## Contributors

Karl Noss (@n055)

Abir Rahman (@arahma12)

Tanner Siffren (@tanner-siffren)

## Instructions

[Download](https://github.com/n055/cse494/releases)

Requires amd64/x86_64 Windows, though source code can be compiled for x86 if desired. Tested on Windows 10. Compiled in Visual Studio 2017.

The toolbar provides all needed controls. It is also keyboard navigable with the TAB key.

### Color Filters

On the toolbar, use the RGB factors to multiply the Red, Blue, or Green values in the pixels of the magnified image. Likewise, use the RGB offsets to add or subtract from the RGB values in the magnifier window.

### Zoom

Use the slider to adjust the zoom level.

### Lock Window

Use the checkbox to lock the magnifier window into place (so all clicks pass through it). Uncheck to allow the user to move and resize the magnifier window. Users can also use the ESC key to toggle these modes.

### Presets

Users may load a text file containing preset color and zoom values. The format is essentially CSV but we recommend editing in Notepad or equivalent. An example settings file is included in the Releases zip file.

Meaning of each comma-delimited column of the settings file:

`Preset Name (Text, cannot contain comma), Red Factor (Number), Green Factor (Number), Blue Factor (Number), Red Offset (Number), Green Offset (Number), Blue Offset (Number)`

After the user loads a preset file, the dropdown menu at the bottom of the toolbar is populated with the presets.
