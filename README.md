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

- reboot or restart X11
