# FreeType2 Font Rendering for QD-OLED @ arch linux

- For my Samsung G8 OLED monitor
- Disabled ClearType Sub pixel font rendering
- Uses custom pixel layout (x/y) -18, -11, 2, 22, 16, -11

- Replace the default freetype2 package
```
cd trunk
makepkg --noextract -f
sudo pacman --remove --nodeps freetype2
sudo pacman --upgrade freetype2-2.12.1-1-x86_64.pkg.tar.zst
```

- Other settings
Disable gtk4 font hinting in ~/.config/gtk-4.0/settings.ini
```
[Settings]
gtk-hint-font-metrics=0
```

Set truetype interpreter version to v40 in /etc/profile.d/freetype2.sh
```
export FREETYPE_PROPERTIES="truetype:interpreter-version=40"
```

Set Font config in /etc/fonts/local.conf
```
<fontconfig>
    <match target="font">
        <edit name="antialias" mode="assign">
            <bool>true</bool>
        </edit>
        <edit name="autohint" mode="assign">
            <bool>false</bool>
        </edit>
        <edit name="hinting" mode="assign">
            <bool>true</bool>
        </edit>
        <edit name="hintstyle" mode="assign">
            <const>hintslight</const>
        </edit>
        <edit name="lcdfilter" mode="assign">
            <const>lcddefault</const>
        </edit>
        <edit name="rgba" mode="assign">
            <const>rgb</const>
        </edit>
        <edit name="embeddedbitmap" mode="assign">
            <bool>false</bool>
        </edit>
     </match>
    <match target="pattern">
    	<edit mode="assign" name="lcdfilter">
      	    <const>lcdlight</const>
    	</edit>
    </match>
</fontconfig>
```
- reboot or restart X11
