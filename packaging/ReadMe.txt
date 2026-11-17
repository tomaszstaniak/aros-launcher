Launcher 0.1, test build @LABEL@ (@TARGET@)

A keyboard launcher for AROS. Press the hotkey, type a few letters of a
program's name, press Return.

This is an early build for testing. It is not a release.


Starting it

Double-click Launcher. Nothing appears: it waits in the background for its
hotkey. It shows up in Exchange as "Launcher"; quit it from there.

To have it start with the system, put the Launcher icon into SYS:WBStartup.

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


What it looks for

Files with a tool icon in SYS:Tools, SYS:Utilities, SYS:System and
SYS:Prefs, and in the drawers directly inside them.

To use other drawers, create the text file ENVARC:Launcher/config with one
line per drawer and start Launcher again:

  folder=SYS:Tools
  folder=Work:Apps
  theme=dark
  compact=0
  glass=1

theme is dark or light. compact=1 gives smaller rows. glass=0 turns the
frosted background off.

The hotkey is the CX_POPKEY ToolType of the Launcher icon.


Not there yet

- Program icons: each result shows a coloured square with a letter.
- A settings window: settings are the text file above.
- Open windows, calculator, system commands and packages as results.
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
