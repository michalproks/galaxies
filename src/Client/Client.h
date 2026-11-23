#ifndef GALAXIES_CLIENT_CLIENT_H
#define GALAXIES_CLIENT_CLIENT_H

namespace Client {

/** Runs the SDL event loop and renders black frames until the window closes. */
int runGame();

/** Initializes SDL and Vulkan, presents one black frame, and exits for automated smoke testing. */
int runSmokeTest();

} // namespace Client

#endif // GALAXIES_CLIENT_CLIENT_H
