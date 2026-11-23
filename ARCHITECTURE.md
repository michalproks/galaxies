# Galaxies Architecture

The game runs on a client-server architecture to support multiplayer. While the server and the
client are internally separate C++ projects, from the user's POV they are compiled to the same
executable.

For every library project there is a separate test project (e.g. `Foo` library and `Foo.Test` test project).

## Repository Structure

```
galaxies/
|-- assets/         # Assets used in the game
|-- docs/           # Misc documentation for the game and the code
|-- specs/          # Specifications for game features and tech stack
`-- src/            # Game code
    |-- Client      # Rendering, input handling and communication with the server
    |-- Main        # Main application executable
    |-- Server      # Communication with the clients, managing game state
    `-- Simulation  # Representation and evolving of the game state; the game core itself
```
