# Menu implementation split

`MicromouseMenu.cpp` is now only the stable public entry translation unit.
The implementation lives in normal `.cpp` files in this folder and shares
private declarations through `MenuInternal.h`.

Sections:

- `MenuInternal.h`: private includes, shared menu state declarations, and cross-unit helper prototypes.
- `MenuDisplayHardware.cpp`: shared U8G2 display instance.
- `MenuState.cpp`: local button, controller, UI-state, and battery state.
- `MenuComm.cpp`: UART callbacks, received-maze frame handling, controller input.
- `MenuGameSettings.cpp`: game role display data and editable menu settings.
- `MenuTree.cpp`: menu item definitions, current menu pointers, settings save.
- `MenuActions.cpp`: actions started from menu items.
- `MenuNavigation.cpp`: menu navigation and input state transitions.
- `MenuRender.cpp`: menu drawing and mapping status screens.
- `MenuCollisionRecovery.cpp`: blocking collision recovery UI.
- `MenuAppLoop.cpp`: `menuAppSetup()`, `menuAppActive()`, and `menuAppLoop()`.

Next cleanup step: reduce the shared state surface in `MenuInternal.h` by moving
closely related state into smaller owner modules.
