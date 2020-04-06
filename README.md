# x-agent
Xorg session agent

`x-agent` starts sibling window manager process(`WM`) and provides
keybinds to control this process.  If `WM` crashes or exits for some
reason, `x-agent` starts `WM` again.

Handy for window manager developers.

## x-agent keybindings

* <kbd>C-Sh-F6</kbd> - Kill `WM` with SIGABORT signal, this will cause
  `WM` to dump core for futher investigation.
* <kbd>C-Sh-F9</kbd> - Kill `WM` with SIGKILL signal, this will cause
  `WM` to always exit.
* <kbd>C-Sh-F11</kbd> - Kill `WM` with SIGTERM signal, this will cause
  `WM` to terminate normally.
* <kbd>C-Sh-ESC</kbd> - Exit x-agent.

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
