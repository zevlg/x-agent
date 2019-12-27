# x-agent
Xorg session agent

Handy for window manager developers

## Example of using x-agent with [EXWM](https://github.com/ch11ng/exwm)

`$ cat ~/.xinitrc`
```console
xrdb -merge $HOME/.Xdefaults
xmodmap $HOME/.Xmodmap

xset s off -dpms
xset m 5 3
xset r rate 250 50

unclutter -idle 1 &
xterm &
exec x-agent -f $HOME/.emacs.d/emacs.log -- emacs -L $HOME/.emacs.d/modules
```
