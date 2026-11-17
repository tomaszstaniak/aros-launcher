Launcher 0.2, test build @LABEL@ (@TARGET@)

A keyboard launcher for AROS. Press the hotkey, type a few letters of a
program's name, press Return.

This is an early build for testing. It is not a release.


Starting it

Double-click Launcher. Nothing appears: it waits in the background for its
hotkey. The first time it is started from its icon it also sets itself up
to start with the system (a start icon in SYS:WBStartup); this can be
switched off in its settings.

Starting Launcher again while it runs shows its panel.


Using it

  Ctrl+Alt+Space   show or hide the panel
  type             find programs; letters may be scattered ("wc" finds
                   WordCraft), case and accents do not matter
  Up, Down         choose a result (Page Up, Page Down move by eight)
  Return           start the chosen program
  Esc              clear what was typed; Esc again closes the panel
  click            choose a result; double-click starts it; a click
                   outside the panel closes it

In the typed text: Left, Right, Home, End move; with Shift they select;
with Ctrl or Alt they move by words. Backspace and Delete remove a
character, with Ctrl or Alt a word. Amiga or Ctrl with A, C, X, V select
all, copy, cut and paste; with Z undo, with Y redo.

Programs you start often move up the list.


Calculator

Type a calculation and the answer is the first result; Return copies it
to the clipboard.

  2+2    (3+4)^2    10/4    7%3    2,5*2    0x1f+1
  sqrt(2)   sin(pi/2)   ln(e)   log(100)   abs(-3)   round(2.5)


Open windows

Windows that are open are found by their title, beside programs; Return
brings the window to the front.


Shell commands

Start the line with ">" to run it as a Shell command in a console of its
own:   > list sys:


Packages

If apkg is installed (C:apkg or the arospkg package), packages of its
catalogue that are not installed yet are found by name or by a word of
their description. Return asks once,
a second Return installs the package in a console window.


System

"Restart Computer" and "Shut Down Computer" ask for a second Return.


Settings

Type "settings" and press Return on "Launcher Settings": the folders that
are searched, dark or light theme, compact rows, frosted glass, start with
the system. "Rescan Programs" and "Quit Launcher" are found the same way.

It looks for programs in the chosen folders and in the drawers directly
inside them: files with a tool icon, and scripts with an icon of their own
(a project icon started through IconX). To begin with these are SYS:Tools,
SYS:Utilities, SYS:System, SYS:Prefs and SYS:Packages (where apkg
installs).

The hotkey is the CX_POPKEY ToolType of the Launcher icon.


Not there yet

- The Tab key (actions for a result) does nothing.


What to look at, please

- Does the hotkey always bring the panel up, also while another program's
  window is active or another screen is in front?
- Can you type at once, without clicking? Do your keyboard's national
  characters and dead keys arrive correctly?
- Does the panel look right on your screen (size, colours, the blurred
  background, the shadow, the corners)? A screenshot helps.
- Does the right program start, and does the panel go away cleanly?
- Anything that hangs, crashes or leaves marks on the screen.

Please say which system you ran it on (distribution or nightly date, and
the graphics driver).


Requirements

@REQUIRES@
TrueType fonts in FONTS:TrueType (DejaVu Sans, as installed with the
system).
