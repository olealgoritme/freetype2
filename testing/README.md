- Test which applications actually use freetype2

- Skewed version immediately shows if freetype2 is used or not
```
LD_PRELOAD="./libfreetype_skewed.so" alacritty
LD_PRELOAD="./libfreetype.so.6.18.3" telegram-desktop
```
