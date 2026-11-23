#ifndef GALAXIES_CLIENT_CLIENT_H
#define GALAXIES_CLIENT_CLIENT_H

namespace Client {

/** Runs the SDL event loop, updates the client camera, and renders server snapshots until the window closes. */
int runGame();

/** Initializes SDL and Vulkan, presents one planetary-system frame, and exits for automated smoke testing. */
int runSmokeTest();

} // namespace Client

#endif // GALAXIES_CLIENT_CLIENT_H
